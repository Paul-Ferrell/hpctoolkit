// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2019-2022, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include "../util/vgannotations.hpp"

#include "hpctracedb2.hpp"

#include "../util/log.hpp"
#include "../util/cache.hpp"
#include "../mpi/all.hpp"

#include "lib/prof-lean/formats/tracedb.h"

#include <iomanip>
#include <sstream>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <unistd.h>

using namespace hpctoolkit;
using namespace sinks;

static constexpr uint64_t align(uint64_t v, uint8_t a) {
  return (v + a - 1) / a * a;
}

HPCTraceDB2::HPCTraceDB2(const stdshim::filesystem::path& dir) {
  if(!dir.empty()) {
    stdshim::filesystem::create_directory(dir);
    tracefile = util::File(dir / "trace.db", true);
  } else {
    util::log::info() << "TraceDB issuing a dry run!";
  }
}

HPCTraceDB2::udThread::udThread(const Thread& t, HPCTraceDB2& tdb)
  : uds(tdb.uds), hdr(t, tdb) {}

static constexpr uint64_t pCtxTraces = align(FMT_TRACEDB_SZ_FHdr, 8);
static constexpr uint64_t ctx_pTraces = align(pCtxTraces + FMT_TRACEDB_SZ_CtxTraceSHdr, 8);

void HPCTraceDB2::notifyWavefront(DataClass d){
  if(!d.hasThreads()) return;
  auto wd_sem = threadsReady.signal();
  util::File::Instance traceinst;
  {
    auto mpiSem = src.enterOrderedWavefront();
    if(tracefile) {
      tracefile->synchronize();
      traceinst = tracefile->open(true, true);
    }

    totalNumTraces = getTotalNumTraces();

    //calculate the offsets for later stored in start and end
    //assign the values of the hdrs
    assignHdrs(calcStartEnd());
  }

  // Write out the headers for threads that have no timepoints
  for(const auto& t : src.threads().iterate()) {
    const auto& hdr = t->userdata[uds.thread].hdr;
    if(hdr.start == hdr.end && tracefile) {
      fmt_tracedb_ctxTrace_t thdr = {
        .profIndex = hdr.prof_info_idx,
        .pStart = hdr.start,
        .pEnd = hdr.end,
      };
      char buf[FMT_TRACEDB_SZ_CtxTrace];
      fmt_tracedb_ctxTrace_write(buf, &thdr);
      traceinst.writeat(ctx_pTraces + (hdr.prof_info_idx - 1) * FMT_TRACEDB_SZ_CtxTrace,
                        sizeof buf, buf);
    }
  }
}

void HPCTraceDB2::notifyThread(const Thread& t) {
  t.userdata[uds.thread].has_trace = false; 
}

void HPCTraceDB2::notifyTimepoints(const Thread& t, const std::vector<
    std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>& tps) {
  assert(!tps.empty());

  threadsReady.wait();
  auto& ud = t.userdata[uds.thread];
  if(!ud.has_trace) {
    has_traces.exchange(true, std::memory_order_relaxed);
    ud.has_trace = true;
    if(tracefile) ud.inst = tracefile->open(true, true);
  }

  util::linear_lru_cache<util::reference_index<const Context>, unsigned int,
                         2> cache;

  for(const auto& [tm, cr]: tps) {
    const Context& c = cr;
    // Try to cache our work as much as possible
    auto id = cache.lookup(c, [&](util::reference_index<const Context> c){
      return c.get().userdata[src.identifier()];
    });

    fmt_tracedb_ctxSample_t datum = {
      .timestamp = static_cast<uint64_t>(tm.count()),
      .ctxId = id,
    };
    if(ud.inst) {
      if(ud.cursor == ud.buffer.data())
        ud.off = ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample;
      assert(ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample < ud.hdr.end);
      fmt_tracedb_ctxSample_write(ud.cursor, &datum);
      ud.cursor += FMT_TRACEDB_SZ_CtxSample;
      if(ud.cursor == &ud.buffer[ud.buffer.size()]) {
        ud.inst->writeat(ud.off, ud.buffer);
        ud.cursor = ud.buffer.data();
      }
      ud.tmcntr++;
    }
  }
}

void HPCTraceDB2::notifyCtxTimepointRewindStart(const Thread& t) {
  auto& ud = t.userdata[uds.thread];
  ud.cursor = ud.buffer.data();
  ud.off = -1;
  ud.tmcntr = 0;
}

void HPCTraceDB2::notifyThreadFinal(const PerThreadTemporary& tt) {
  auto& ud = tt.thread().userdata[uds.thread];
  if(ud.inst) {
    if(ud.cursor != ud.buffer.data())
      ud.inst->writeat(ud.off, ud.cursor - ud.buffer.data(), ud.buffer.data());

    //write the hdr
    auto new_end = ud.hdr.start + ud.tmcntr * FMT_TRACEDB_SZ_CtxSample;
    assert(new_end <= ud.hdr.end);
    ud.hdr.end = new_end;
    fmt_tracedb_ctxTrace_t hdr = {
      .profIndex = ud.hdr.prof_info_idx,
      .pStart = ud.hdr.start,
      .pEnd = ud.hdr.end,
    };
    assert((hdr.pStart != (uint64_t)INVALID_HDR) | (hdr.pEnd != (uint64_t)INVALID_HDR));
    char buf[FMT_TRACEDB_SZ_CtxTrace];
    fmt_tracedb_ctxTrace_write(buf, &hdr);
    ud.inst->writeat(ctx_pTraces + (ud.hdr.prof_info_idx - 1) * FMT_TRACEDB_SZ_CtxTrace,
                     sizeof buf, buf);

    ud.inst = std::nullopt;
  }
}

void HPCTraceDB2::notifyPipeline() noexcept {
  auto& ss = src.structs();
  uds.thread = ss.thread.add<udThread>(std::ref(*this));
  src.registerOrderedWavefront();
}

std::string HPCTraceDB2::exmlTag() {
  if(!has_traces.load(std::memory_order_relaxed)) return "";
  auto [min, max] = src.timepointBounds().value_or(std::make_pair(
      std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero()));
  std::ostringstream ss;
  ss << "<TraceDB"
        " i=\"0\""
        " db-min-time=\"" << min.count() << "\""
        " db-max-time=\"" << max.count() << "\""
        " u=\"1000000000\"/>\n";
  return ss.str();
}

void HPCTraceDB2::write() {
  if(mpi::World::rank() != 0) return;
  if(!tracefile) return;
  auto traceinst = tracefile->open(true, true);

  auto [min, max] = src.timepointBounds().value_or(std::make_pair(
      std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero()));

  // Write out the static headers
  {
    fmt_tracedb_fHdr_t fhdr = {
      .szCtxTraces = ctx_pTraces + totalNumTraces * FMT_TRACEDB_SZ_CtxTrace - pCtxTraces,
      .pCtxTraces = pCtxTraces,
    };
    char buf[FMT_TRACEDB_SZ_FHdr];
    fmt_tracedb_fHdr_write(buf, &fhdr);
    traceinst.writeat(0, sizeof buf, buf);
  }
  {
    fmt_tracedb_ctxTraceSHdr_t shdr = {
      .pTraces = ctx_pTraces,
      .nTraces = (uint32_t)totalNumTraces,
      .minTimestamp = (uint64_t)min.count(), .maxTimestamp = (uint64_t)max.count(),
    };
    char buf[FMT_TRACEDB_SZ_CtxTraceSHdr];
    fmt_tracedb_ctxTraceSHdr_write(buf, &shdr);
    traceinst.writeat(pCtxTraces, sizeof buf, buf);
  }

  // TODO: Add an mpi::barrier to ensure the footer is only written when all
  // ranks have finished their writing.
  if(mpi::World::rank() + 1 == mpi::World::size())
    traceinst.writeat(footerPos, sizeof fmt_tracedb_footer, fmt_tracedb_footer);
}


//***************************************************************************
// trace_hdr
//***************************************************************************
HPCTraceDB2::traceHdr::traceHdr(const Thread& t, HPCTraceDB2& tdb)
  : prof_info_idx(t.userdata[tdb.src.identifier()] + 1),
   start(INVALID_HDR), end(INVALID_HDR) {}

uint64_t HPCTraceDB2::getTotalNumTraces() {
  uint32_t rank_num_traces = src.threads().size();
  return mpi::allreduce<uint32_t>(rank_num_traces, mpi::Op::sum());
}

std::vector<uint64_t> HPCTraceDB2::calcStartEnd() {
  //get the size of all traces
  std::vector<uint64_t> trace_sizes;
  uint64_t total_size = 0;
  for(const auto& t : src.threads().iterate()){
    uint64_t trace_sz = align(t->attributes.ctxTimepointMaxCount() * FMT_TRACEDB_SZ_CtxSample, 8);
    trace_sizes.emplace_back(trace_sz);
    total_size += trace_sz;
  }

  //get the offset of this rank's traces section
  uint64_t my_off = mpi::exscan(total_size, mpi::Op::sum()).value_or(0);
  my_off += align(ctx_pTraces + totalNumTraces * FMT_TRACEDB_SZ_CtxTrace, 8);

  //get the individual offsets of this rank's traces
  std::vector<uint64_t> trace_offs(trace_sizes.size() + 1);
  trace_sizes.emplace_back(0);
  exscan<uint64_t>(trace_sizes);
  for(uint i = 0; i < trace_sizes.size();i++){
    trace_offs[i] = trace_sizes[i] + my_off;
  }

  return trace_offs;

}

void HPCTraceDB2::assignHdrs(const std::vector<uint64_t>& trace_offs) {
  int i = 0;
  for(const auto& t : src.threads().iterate()){
    auto& hdr = t->userdata[uds.thread].hdr;
    hdr.start = trace_offs[i];
    hdr.end = trace_offs[i] + t->attributes.ctxTimepointMaxCount() * FMT_TRACEDB_SZ_CtxSample;
    i++;
  }
  footerPos = trace_offs.back();
}

template <typename T>
void HPCTraceDB2::exscan(std::vector<T>& data) {
  int n = data.size();
  int rounds = ceil(std::log2(n));
  std::vector<T> tmp (n);

  for(int i = 0; i<rounds; i++){
    for(int j = 0; j < n; j++){
      int p = (int)pow(2.0,i);
      tmp.at(j) = (j<p) ?  data.at(j) : data.at(j) + data.at(j-p);
    }
    if(i<rounds-1) data = tmp;
  }

  if(n>0) data[0] = 0;
  for(int i = 1; i < n; i++){
    data[i] = tmp[i-1];
  }
}
