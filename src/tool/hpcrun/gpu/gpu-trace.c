//******************************************************************************
// local includes
//******************************************************************************

#include <pthread.h>



//******************************************************************************
// libmonitor 
//******************************************************************************

#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/trace.h>

#include "gpu-context-id-map.h"
#include "gpu-monitoring.h"
#include "gpu-trace.h"
#include "gpu-trace-channel.h"
#include "gpu-trace-item.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_t {
  pthread_t thread;
  gpu_trace_channel_t *trace_channel;
} gpu_trace_t;

typedef void *(*pthread_start_routine_t)(void *);



//******************************************************************************
// local variables
//******************************************************************************

static _Atomic(bool) stop_trace_flag;

static atomic_ullong stream_counter;

static atomic_ullong stream_id;

static __thread uint64_t stream_start = 0;



//******************************************************************************
// private operations
//******************************************************************************

static void
stream_start_set
(
 uint64_t start_time
)
{
  if (!stream_start) stream_start = start_time;
}


static uint64_t
stream_start_get
(
 void
)
{
  return stream_start;
}


static gpu_trace_t *
gpu_trace_alloc
(
 void
)
{
  gpu_trace_t *trace = hpcrun_malloc_safe(sizeof(gpu_trace_t));
  trace->trace_channel = gpu_trace_channel_alloc();
  return trace;
}


static cct_node_t *
gpu_trace_cct_root
(
 thread_data_t* td
)
{
  return td->core_profile_trace_data.epoch->csdata.tree_root;
}


static cct_node_t *
gpu_trace_cct_no_thread
(
 thread_data_t* td
)
{
  cct_node_t *no_thread = 
    td->core_profile_trace_data.epoch->csdata.special_no_thread_node;

  return no_thread;
}


static cct_node_t *
gpu_trace_cct_insert_context
(
 thread_data_t* td,
 cct_node_t *path
)
{
  cct_node_t *leaf = 
    hpcrun_cct_insert_path_return_leaf(gpu_trace_cct_root(td), path);

  return leaf;
}


static uint64_t
gpu_trace_time
(
 uint64_t gpu_time
)
{
  // return time in ns
  uint64_t time = gpu_time; 

  return time;
}


static void
gpu_trace_stream_append
(
 thread_data_t* td,
 cct_node_t *leaf,
 uint64_t time
)
{
  hpcrun_trace_append_stream(&td->core_profile_trace_data, leaf, 0, 
			     td->prev_dLCA, time); 
}


static void
gpu_trace_first
(
 thread_data_t* td,
 cct_node_t *no_thread,
 uint64_t start
)
{
  static __thread bool first = true;

  if (first) {
    first = false;
    gpu_trace_stream_append(td, no_thread, start - 1);
  }
}


static uint64_t
gpu_trace_start_adjust
(
 uint64_t start,
 uint64_t end
)
{
  static __thread uint64_t last_end = 0;

  if (start < last_end) {
    // If we have a hardware measurement error (Power9),
    // set the offset as the end of the last activity
    start = last_end + 1;
  }

  last_end = end;

  return start;
}


static void
consume_one_trace_item
(
 thread_data_t* td,
 cct_node_t *call_path,
 uint64_t start_time,
 uint64_t end_time
)
{

  cct_node_t *leaf = gpu_trace_cct_insert_context(td, call_path);

  cct_node_t *no_thread = gpu_trace_cct_no_thread(td);

  uint64_t start = gpu_trace_time(start_time);
  uint64_t end   = gpu_trace_time(end_time);

  stream_start_set(start_time);

  start = gpu_trace_start_adjust(start, end);

  int frequency = gpu_monitoring_trace_sample_frequency_get();

  bool append = false;

  if (frequency != -1) {
    uint64_t cur_start = start_time;
    uint64_t cur_end = end_time;
    uint64_t intervals = (cur_start - stream_start_get() - 1) / frequency + 1;
    uint64_t pivot = intervals * frequency + stream_start;

    if (pivot <= cur_end && pivot >= cur_start) {
      // only trace when the pivot is within the range
      PRINT("pivot %" PRIu64 " not in <%" PRIu64 ", %" PRIu64 
	    "> with intervals %" PRIu64 ", frequency %" PRIu64 "\n",
        pivot, cur_start, cur_end, intervals, frequency);
      append = true;
    }
  } else {
    append = true;
  }

  if (append) {
    gpu_trace_first(td, no_thread, start);
    
    gpu_trace_stream_append(td, leaf, start);
    
    gpu_trace_stream_append(td, no_thread, end + 1);
    
    PRINT("%p Append trace activity [%lu, %lu]\n", td, start, end);
  }
}


static void
gpu_trace_activities_process
(
 thread_data_t *td,
 gpu_trace_t *thread_args
)
{
  gpu_trace_channel_consume(thread_args->trace_channel, td, 
			    consume_one_trace_item);
}


static void
gpu_trace_activities_await
(
 gpu_trace_t* thread_args
)
{
  gpu_trace_channel_await(thread_args->trace_channel);
}


static int
gpu_trace_stream_id
(
 void
)
{
  // FIXME: this is a bad way to compute a stream id 
  int id = 500 + atomic_fetch_add(&stream_id, 1);

  return id;
}


static thread_data_t *
gpu_trace_stream_acquire
(
 void
)
{
  thread_data_t* td = NULL;

  int id = gpu_trace_stream_id();

  hpcrun_threadMgr_non_compact_data_get(id, NULL, &td);

  hpcrun_set_thread_data(td);

  return td;
}


static void
gpu_trace_stream_release
(
 thread_data_t *td
)
{
  epoch_t *epoch = TD_GET(core_profile_trace_data.epoch);

  hpcrun_threadMgr_data_put(epoch, td);

  atomic_fetch_add(&stream_counter, -1);
}



//******************************************************************************
// interface operations
//******************************************************************************

void 
gpu_trace_init
(
 void
)
{
  atomic_store(&stop_trace_flag, false);
  atomic_store(&stream_counter, 0);
  atomic_store(&stream_id, 0);
}


void *
gpu_trace_record
(
 gpu_trace_t *thread_args
)
{
  thread_data_t* td = gpu_trace_stream_acquire();

  while (!atomic_load(&stop_trace_flag)) {
    gpu_trace_activities_process(td, thread_args);
    gpu_trace_activities_await(thread_args);
  }

  gpu_trace_activities_process(td, thread_args);

  gpu_trace_stream_release(td);

  return NULL;
}


void
gpu_trace_fini
(
 void *arg
)
{
  PRINT("gpu_trace_fini called\n");

  atomic_store(&stop_trace_flag, true);

  gpu_context_stream_map_signal_all();

  while (atomic_load(&stream_counter));
}


gpu_trace_t *
gpu_trace_create
(
 void
)
{
  // Init variables
  gpu_trace_t *trace = gpu_trace_alloc();

  // Create a new thread for the stream without libmonitor watching
  monitor_disable_new_threads();

  atomic_fetch_add(&stream_counter, 1);

  pthread_create(&trace->thread, NULL, (pthread_start_routine_t) gpu_trace_record, 
		 trace);

  monitor_enable_new_threads();

  return trace;
}


void 
gpu_trace_produce
(
 gpu_trace_t *t,
 gpu_trace_item_t *ti
)
{
  gpu_trace_channel_produce(t->trace_channel, ti);
}


void 
gpu_trace_signal_consumer
(
 gpu_trace_t *t
)
{
  gpu_trace_channel_signal_consumer(t->trace_channel);
}