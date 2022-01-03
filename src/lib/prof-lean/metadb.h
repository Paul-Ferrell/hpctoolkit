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

//***************************************************************************
//
// Purpose:
//   Low-level types and functions for reading/writing meta.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef METADB_FMT_H
#define METADB_FMT_H

//************************* System Include Files ****************************

#include <stdbool.h>
#include <limits.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// metadb header
//***************************************************************************
#define HPCMETADB_FMT_Magic   "HPCTOOLKIT_expmt" //16 bytes
#define HPCMETADB_FMT_VersionMajor 4             //1  byte
#define HPCMETADB_FMT_VersionMinor 0             //1  byte
#define HPCMETADB_FMT_NumSec       8             //2  byte

#define HPCMETADB_FMT_MagicLen     (sizeof(HPCMETADB_FMT_Magic) - 1)
#define HPCMETADB_FMT_VersionLen   2
#define HPCMETADB_FMT_NumSecLen    2
#define HPCMETADB_FMT_PaddingLen   4
#define HPCMETADB_FMT_SecSizeLen   8
#define HPCMETADB_FMT_SecPtrLen    8
#define HPCMETADB_FMT_SecLen       (HPCMETADB_FMT_SecSizeLen + HPCMETADB_FMT_SecPtrLen)

#define HPCMETADB_FMT_HeaderLen  (HPCMETADB_FMT_MagicLen + HPCMETADB_FMT_VersionLen \
  + HPCMETADB_FMT_NumSecLen + HPCMETADB_FMT_PaddingLen \
  + HPCMETADB_FMT_SecLen * HPCMETADB_FMT_NumSec)

typedef struct metadb_hdr_t {
  uint8_t versionMajor;
  uint8_t versionMinor;
  uint16_t num_sec;

  uint64_t general_sec_size;
  uint64_t general_sec_ptr;
  uint64_t idtable_sec_size;
  uint64_t idtable_sec_ptr;
  uint64_t metric_sec_size;
  uint64_t metric_sec_ptr;
  uint64_t ctree_sec_size;
  uint64_t ctree_sec_ptr;
  uint64_t str_sec_size;
  uint64_t str_sec_ptr;
  uint64_t modules_sec_size;
  uint64_t modules_sec_ptr;
  uint64_t files_sec_size;
  uint64_t files_sec_ptr;
  uint64_t funcs_sec_size;
  uint64_t funcs_sec_ptr;
} metadb_hdr_t;

int metadb_hdr_fwrite(const metadb_hdr_t*, FILE* fs);

char* metadb_hdr_swrite(const metadb_hdr_t*, char* buf);

int metadb_hdr_fread(metadb_hdr_t*, FILE* infs);

int metadb_hdr_fprint(const metadb_hdr_t*, FILE* fs, const char* prefix);

//***************************************************************************
// general properties
//***************************************************************************

typedef struct metadb_general_t {
  char* title;
  char* description;
} metadb_general_t;

void metadb_general_free(metadb_general_t*, hpcfmt_free_fn dealloc);

int metadb_general_fwrite(const metadb_general_t*, FILE* fs);

int metadb_general_fread(metadb_general_t*, FILE* infs);

int metadb_general_fprint(const metadb_general_t*, FILE* fs, const char* prefix);

//***************************************************************************
// identifier table
//***************************************************************************

typedef struct metadb_idtable_t {
  uint16_t num_kinds;
  char** names;
} metadb_idtable_t;

void metadb_idtable_free(metadb_idtable_t*, hpcfmt_free_fn dealloc);

int metadb_idtable_fwrite(const metadb_idtable_t*, FILE* fs);

int metadb_idtable_fread(metadb_idtable_t*, FILE* infs);

int metadb_idtable_fprint(const metadb_idtable_t*, FILE* fs, const char* prefix);

//***************************************************************************
// performance metric
//***************************************************************************

typedef struct metadb_metric_t {
  uint16_t num_scopes;
  char* name;
  // Followed by num_scopes * metadb_metric_scope_t...
} metadb_metric_t;

void metadb_metric_free(metadb_metric_t*, hpcfmt_free_fn dealloc);

int metadb_metric_fwrite(const metadb_metric_t*, FILE* fs);

int metadb_metric_fread(metadb_metric_t*, FILE* infs);

int metadb_metric_fprint(const metadb_metric_t*, FILE* fs, const char* prefix);

typedef struct metadb_metric_scope_t {
  uint16_t raw_mid;
  uint16_t num_summaries;
  char* scope;
  // Followed by num_summaries * metadb_metric_summary_t
} metadb_metric_scope_t;

void metadb_metric_scope_free(metadb_metric_scope_t*, hpcfmt_free_fn dealloc);

int metadb_metric_scope_fwrite(const metadb_metric_scope_t*, FILE* fs);

int metadb_metric_scope_fread(metadb_metric_scope_t*, FILE* infs);

int metadb_metric_scope_fprint(const metadb_metric_scope_t*, FILE* fs, const char* prefix);

#define HPCMETADB_FMT_COMBINE_SUM 0
#define HPCMETADB_FMT_COMBINE_MIN 1
#define HPCMETADB_FMT_COMBINE_MAX 2

typedef struct metadb_metric_summary_t {
  uint16_t summary_mid;
  uint8_t combine;
  char* formula;
} metadb_metric_summary_t;

void metadb_metric_summary_free(metadb_metric_summary_t*, hpcfmt_free_fn dealloc);

int metadb_metric_summary_fwrite(const metadb_metric_summary_t*, FILE* fs);

int metadb_metric_summary_fread(metadb_metric_summary_t*, FILE* infs);

int metadb_metric_summary_fprint(const metadb_metric_summary_t*, FILE* fs, const char* prefix);

//***************************************************************************
// load modules
//***************************************************************************

#define HPCMETADB_FMT_LoadModuleLen 12
typedef struct metadb_loadmodule_t {
  uint64_t path_ptr;
} metadb_loadmodule_t;

int metadb_loadmodule_fwrite(const metadb_loadmodule_t*, FILE* fs);

char* metadb_loadmodule_swrite(const metadb_loadmodule_t*, char* buf);

int metadb_loadmodule_fread(metadb_loadmodule_t*, FILE* infs);

int metadb_loadmodule_fprint(const metadb_loadmodule_t*, FILE* fs, FILE* infs, const char* prefix);

//***************************************************************************
// source file
//***************************************************************************

#define HPCMETADB_FMT_SrcFileLen 12
typedef struct metadb_srcfile_t {
  bool is_copied : 1;
  uint64_t path_ptr;
} metadb_srcfile_t;

int metadb_srcfile_fwrite(const metadb_srcfile_t*, FILE* fs);

char* metadb_srcfile_swrite(const metadb_srcfile_t*, char* buf);

int metadb_srcfile_fread(metadb_srcfile_t*, FILE* infs);

int metadb_srcfile_fprint(const metadb_srcfile_t*, FILE* fs, FILE* infs, const char* prefix);

//***************************************************************************
// function
//***************************************************************************

#define HPCMETADB_FMT_FuncLen 40
typedef struct metadb_func_t {
  uint64_t name_ptr;
  uint64_t module_ptr;
  uint64_t module_offset;
  uint64_t def_fileptr;
  uint32_t def_line;
} metadb_func_t;

int metadb_func_fwrite(const metadb_func_t*, FILE* fs);

char* metadb_func_swrite(const metadb_func_t*, char* buf);

int metadb_func_fread(metadb_func_t*, FILE* infs);

int metadb_func_fprint(const metadb_func_t*, FILE* fs, FILE* infs, const char* prefix);

//***************************************************************************
// context tree node
//***************************************************************************

#define HPCMETADB_FMT_CALL_Normal  0
#define HPCMETADB_FMT_CALL_Inlined 1
#define HPCMETADB_FMT_CALL_None    0xFF

#define HPCMETADB_FMT_LEX_Function 0
#define HPCMETADB_FMT_LEX_Loop     1
#define HPCMETADB_FMT_LEX_SrcLine  2
#define HPCMETADB_FMT_LEX_Point    3

typedef struct metadb_ctxnode_t {
  uint32_t ctx_id;
  uint64_t block_ptr;
  uint64_t block_size;

  // Callee specification
  uint8_t call_type;  // CALL_None if not a callee
  uint64_t caller_fileptr;  // 0 if no source file/line associated
  uint32_t caller_line;     // valid iff caller_fileptr is not 0

  // Lexical specification
  uint8_t lex_type;
  bool is_call_srcline : 1;
  uint64_t func_ptr;  // 0 if no function associated
  uint64_t src_fileptr;  // 0 if no source file/line associated
  uint32_t src_line;     // valid iff src_fileptr is not 0
  uint64_t module_ptr;     // 0 if no load module/offset associated
  uint64_t module_offset;  // valid iff module_ptr is not 0
} metadb_ctxnode_t;

int metadb_ctxnode_fwrite(const metadb_ctxnode_t*, FILE* fs);

int metadb_ctxnode_fread(metadb_ctxnode_t*, FILE* infs);

int metadb_ctxnode_fprint_long(const metadb_ctxnode_t*, FILE* fs, FILE* infs, const char* prefix);

int metadb_ctxnode_fprint_short(const metadb_ctxnode_t*, FILE* fs, FILE* infs, const char* prefix);

//***************************************************************************
// footer
//***************************************************************************
#define METADBft 0x50524f4644426d6e

//***************************************************************************
#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // METADB_FMT_H
