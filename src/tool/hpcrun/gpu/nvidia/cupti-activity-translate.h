#ifndef cupti_activity_translate_h
#define cupti_activity_translate_h


//******************************************************************************
// nvidia includes
//******************************************************************************

#include <cupti_activity.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;
typedef struct cct_node_t cct_node_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_activity_translate
(
 gpu_activity_t *entry,
 CUpti_Activity *activity,
 cct_node_t *cct_node
);

#endif
