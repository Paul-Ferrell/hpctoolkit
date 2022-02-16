// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

#include "hpcrun-placeholders.h"

#include "hpcrun/fnbounds/fnbounds_interface.h"
#include "hpcrun/hpcrun-initializers.h"
#include "hpcrun/safe-sampling.h"

#include "lib/prof-lean/placeholders.h"

#include <assert.h>
#include <pthread.h>

static placeholder_t hpcrun_placeholders[hpcrun_placeholder_type_count];

static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;

void hpcrun_no_activity(void) {
  // this function is not meant to be called
  assert(0);
}

static void hpcrun_default_placeholders_init(void) {
  init_placeholder(&hpcrun_placeholders[hpcrun_placeholder_type_no_activity], hpcrun_no_activity);
}

load_module_t* pc_to_lm(void* pc) {
  void *func_start_pc, *func_end_pc;
  load_module_t* lm = NULL;
  fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc, &lm);
  return lm;
}

void init_placeholder(placeholder_t* p, void* pc) {
  // protect against receiving a sample here. if we do, we may get
  // deadlock trying to acquire a lock associated with
  // fnbounds_enclosing_addr
  hpcrun_safe_enter();
  {
    void* cpc = canonicalize_placeholder(pc);
    p->pc = cpc;
    p->pc_norm = hpcrun_normalize_ip(cpc, pc_to_lm(cpc));
  }
  hpcrun_safe_exit();
}

placeholder_t* hpcrun_placeholder_get(hpcrun_placeholder_type_t ph_type) {
  pthread_once(&is_initialized, hpcrun_default_placeholders_init);

  return &hpcrun_placeholders[ph_type];
}
