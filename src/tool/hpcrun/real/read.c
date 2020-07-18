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
// Copyright ((c)) 2002-2020, Rice University
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

//------------------------------------------------------------------------------
// File: read.c 
//  
// Purpose: 
//   wrapper for libc read to avoid interception.
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// local includes
//******************************************************************************

#include <real/read.h>

#include <monitor-exts/monitor_ext.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef 
ssize_t 
read_fn_t (
 int fd, 
 void *buf, 
 size_t count
);



//******************************************************************************
// local data
//******************************************************************************

#ifdef HPCRUN_STATIC_LINK
extern read_fn_t  __real_read;
#endif

static read_fn_t *real_read = NULL;



//******************************************************************************
// local operations
//******************************************************************************

static void 
find_read(void)
{
#ifdef HPCRUN_STATIC_LINK
  real_read = __real_read;
#else
  // don't just look for the next symbol, get it from the source
  void *libc = monitor_real_dlopen("libc.so", RTLD_LAZY);
  real_read = (read_fn_t *) dlsym(libc, "read");
#endif

  assert(real_read);
}



//******************************************************************************
// interface operations
//******************************************************************************

ssize_t 
hpcrun_real_read
(
 int fd, 
 void *buf, 
 size_t count
)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_read);
  
  // call the real libc read operation without getting intercepted
  ssize_t ret = (* real_read) (fd, buf, count);

  return ret;
}
