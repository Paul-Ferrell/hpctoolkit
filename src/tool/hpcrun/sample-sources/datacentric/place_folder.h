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
// Copyright ((c)) 2002-2017, Rice University
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

#ifndef __PLACE_FORLDER_H__
#define __PLACE_FORLDER_H__

#define FUNCTION_FOLDER_NAME(name) monitor_data ## _ ## name

#define FUNCTION_FOLDER_CALL(name) FUNCTION_FOLDER_NAME(name) ## _()

#define FUNCTION_FOLDER(name) 	void \
			 	static monitor_data ## _ ## name(void) \
				{}

#define FUNCTION_DATA_FOLDER_NAME(start,end) range ## _ ## start ## _ ## end

#define FUNCTION_DATA_FOLDER(start,end)      FUNCTION_FOLDER(range_ ## start ## _ ## end)


#if defined(__PPC64__) || defined(HOST_CPU_IA64)
#define POINTER_TO_FUNCTION *(void**)
#else
#define POINTER_TO_FUNCTION
#endif

/********************************
 * place folder for data centric
 *******************************/

#if 0
FUNCTION_FOLDER(heap)
FUNCTION_FOLDER(stack)
FUNCTION_FOLDER(unknown)
FUNCTION_FOLDER(access_unknown)
FUNCTION_FOLDER(access_heap)
FUNCTION_FOLDER(heap_allocation)

FUNCTION_DATA_FOLDER(1, 5)
FUNCTION_DATA_FOLDER(2, 5)
FUNCTION_DATA_FOLDER(3, 5)
FUNCTION_DATA_FOLDER(4, 5)
FUNCTION_DATA_FOLDER(5, 5)
#endif

#ifdef TEST_PF_DEBUG
int main()
{
  FUNCTION_FOLDER_NAME(head_data_allocation)();

  FUNCTION_DATA_FOLDER_NAME(1, 5) ();
}
#endif // TEST_PF_DEBUG

#endif // __PLACE_FORLDER_H__


