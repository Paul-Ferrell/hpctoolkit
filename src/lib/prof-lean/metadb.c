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

//************************* System Include Files ****************************

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "hpcio.h"
#include "hpcio-buffer.h"
#include "hpcfmt.h"
#include "hpcrun-fmt.h"
#include "metadb.h"


//***************************************************************************

// strcat, but malloc's the output to be large enough
static char* strccat(const char* a, const char* b) {
  char* out = malloc(strlen(a) + strlen(b) + 1);
  if(out != NULL) {
    strcpy(out, a);
    strcat(out, b);
  }
  return out;
}

//***************************************************************************
// metadb header
//***************************************************************************
int metadb_hdr_fwrite(const metadb_hdr_t* hdr, FILE* fs) {
  fwrite(HPCMETADB_FMT_Magic, 1, HPCMETADB_FMT_MagicLen, fs);
  uint8_t versionMajor = HPCMETADB_FMT_VersionMajor;
  uint8_t versionMinor = HPCMETADB_FMT_VersionMinor;
  fwrite(&versionMajor, 1, 1, fs);
  fwrite(&versionMinor, 1, 1, fs);

  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(HPCMETADB_FMT_NumSec, fs));
  _Static_assert(HPCMETADB_FMT_PaddingLen == 4, "Following line needs to be adjusted!");
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(0, fs));

  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->general_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->general_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->idtable_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->idtable_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->metric_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->metric_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->ctree_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->ctree_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->str_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->str_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->modules_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->modules_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->files_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->files_sec_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->funcs_sec_size, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(hdr->funcs_sec_ptr, fs));
  return HPCFMT_OK;
}

char* metadb_hdr_swrite(const metadb_hdr_t* hdr, char* buf) {
  buf = memcpy(buf, HPCMETADB_FMT_Magic, HPCMETADB_FMT_MagicLen) + HPCMETADB_FMT_MagicLen;
  *(buf++) = HPCMETADB_FMT_VersionMajor;
  *(buf++) = HPCMETADB_FMT_VersionMinor;

  buf = hpcfmt_int2_swrite(HPCMETADB_FMT_NumSec, buf);
  buf = memset(buf, 0, HPCMETADB_FMT_PaddingLen) + HPCMETADB_FMT_PaddingLen;

  buf = hpcfmt_int8_swrite(hdr->general_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->general_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->idtable_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->idtable_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->metric_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->metric_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->ctree_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->ctree_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->str_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->str_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->modules_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->modules_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->files_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->files_sec_ptr, buf);
  buf = hpcfmt_int8_swrite(hdr->funcs_sec_size, buf);
  buf = hpcfmt_int8_swrite(hdr->funcs_sec_ptr, buf);
  return buf;
}

int metadb_hdr_fread(metadb_hdr_t* hdr, FILE* infs) {
  char magic[HPCMETADB_FMT_MagicLen];
  if(fread(magic, 1, HPCMETADB_FMT_MagicLen, infs) < HPCMETADB_FMT_MagicLen)
    return HPCFMT_ERR;
  if(strncmp(magic, HPCMETADB_FMT_Magic, HPCMETADB_FMT_MagicLen) != 0)
    return HPCFMT_ERR;

  if(fread(&hdr->versionMajor, 1, 1, infs) < 1) return HPCFMT_ERR;
  if(fread(&hdr->versionMinor, 1, 1, infs) < 1) return HPCFMT_ERR;
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&hdr->num_sec, infs));
  if(fseek(infs, HPCMETADB_FMT_PaddingLen, SEEK_CUR) != 0) return HPCFMT_ERR;

  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->general_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->general_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->idtable_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->idtable_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->metric_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->metric_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->ctree_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->ctree_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->str_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->str_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->modules_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->modules_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->files_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->files_sec_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->funcs_sec_size, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&hdr->funcs_sec_ptr, infs));
  return HPCFMT_OK;
}

int metadb_hdr_fprint(const metadb_hdr_t* hdr, FILE* fs, const char* pfx) {
  fprintf(fs, "%s\n", HPCMETADB_FMT_Magic);

  fprintf(fs, "%s[hdr:\n", pfx);
  fprintf(fs, "%s  (version: %"PRIu8".%"PRIu8")\n", pfx, hdr->versionMajor, hdr->versionMinor);
  fprintf(fs, "%s  (num_sec: %"PRIu32")\n", pfx, hdr->num_sec);
  fprintf(fs, "%s  (general_sec_size: 0x%"PRIx64")\n", pfx, hdr->general_sec_size);
  fprintf(fs, "%s  (general_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->general_sec_ptr);
  fprintf(fs, "%s  (idtable_sec_size: 0x%"PRIx64")\n", pfx, hdr->idtable_sec_size);
  fprintf(fs, "%s  (idtable_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->idtable_sec_ptr);
  fprintf(fs, "%s  (metric_sec_size: 0x%"PRIx64")\n", pfx, hdr->metric_sec_size);
  fprintf(fs, "%s  (metric_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->metric_sec_ptr);
  fprintf(fs, "%s  (ctree_sec_size: 0x%"PRIx64")\n", pfx, hdr->ctree_sec_size);
  fprintf(fs, "%s  (ctree_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->ctree_sec_ptr);
  fprintf(fs, "%s  (str_sec_size: 0x%"PRIx64")\n", pfx, hdr->str_sec_size);
  fprintf(fs, "%s  (str_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->str_sec_ptr);
  fprintf(fs, "%s  (modules_sec_size: 0x%"PRIx64")\n", pfx, hdr->modules_sec_size);
  fprintf(fs, "%s  (modules_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->modules_sec_ptr);
  fprintf(fs, "%s  (files_sec_size: 0x%"PRIx64")\n", pfx, hdr->files_sec_size);
  fprintf(fs, "%s  (files_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->files_sec_ptr);
  fprintf(fs, "%s  (funcs_sec_size: 0x%"PRIx64")\n", pfx, hdr->funcs_sec_size);
  fprintf(fs, "%s  (funcs_sec_ptr:  0x%"PRIx64")\n", pfx, hdr->funcs_sec_ptr);
  fprintf(fs, "%s]\n", pfx);

  return HPCFMT_OK;
}

//***************************************************************************
// general properties
//***************************************************************************
void metadb_general_free(metadb_general_t* gen, hpcfmt_free_fn dealloc) {
  if(dealloc == NULL) dealloc = free;
  dealloc(gen->title); gen->title = NULL;
  dealloc(gen->description); gen->description = NULL;
}

int metadb_general_fwrite(const metadb_general_t* gen, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(gen->title, fs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(gen->description, fs));
  return HPCFMT_OK;
}

int metadb_general_fread(metadb_general_t* gen, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&gen->title, infs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&gen->description, infs));
  return HPCFMT_OK;
}

int metadb_general_fprint(const metadb_general_t* gen, FILE* fs, const char* pfx) {
  fprintf(fs, "%s[general properties:\n", pfx);
  fprintf(fs, "%s  (title: %s)\n", pfx, gen->title);
  fprintf(fs, "%s  (description: %s)\n", pfx, gen->description);
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// identifier table
//***************************************************************************
void metadb_idtable_free(metadb_idtable_t* idt, hpcfmt_free_fn dealloc) {
  if(dealloc == NULL) dealloc = free;
  for(size_t i = 0; i < idt->num_kinds; i++)
    dealloc(idt->names[i]);
  dealloc(idt->names); idt->names = NULL;
}

int metadb_idtable_fwrite(const metadb_idtable_t* idt, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(idt->num_kinds, fs));
  for(size_t i = 0; i < idt->num_kinds; i++)
    HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(idt->names[i], fs));
  return HPCFMT_OK;
}

int metadb_idtable_fread(metadb_idtable_t* idt, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&idt->num_kinds, infs));
  idt->names = malloc(idt->num_kinds * sizeof idt->names[0]);
  for(size_t i = 0; i < idt->num_kinds; i++)
    HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&idt->names[i], infs));
  return HPCFMT_OK;
}

int metadb_idtable_fprint(const metadb_idtable_t* idt, FILE* fs, const char* pfx) {
  fprintf(fs, "%s[identifier table:\n", pfx);
  fprintf(fs, "%s  (num_kinds: %"PRIu16")\n", pfx, idt->num_kinds);
  fprintf(fs, "%s  [", pfx);
  for(size_t i = 0; i < idt->num_kinds; i++)
    fprintf(fs, "%s    (id: %zu) (name: %s)\n", pfx, i, idt->names[i]);
  fprintf(fs, "%s  ]\n", pfx);
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// performance metric
//***************************************************************************
void metadb_metric_free(metadb_metric_t* met, hpcfmt_free_fn dealloc) {
  if(dealloc == NULL) dealloc = free;
  dealloc(met->name);
}

int metadb_metric_fwrite(const metadb_metric_t* met, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(met->num_scopes, fs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(met->name, fs));
  return HPCFMT_OK;
}

int metadb_metric_fread(metadb_metric_t* met, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&met->num_scopes, infs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&met->name, infs));
  return HPCFMT_OK;
}

int metadb_metric_fprint(const metadb_metric_t* met, FILE* fs, const char* pfx) {
  fprintf(fs, "%s[metric (header):\n", pfx);
  fprintf(fs, "%s  (num_scopes: %"PRIu16")\n", pfx, met->num_scopes);
  fprintf(fs, "%s  (name: %s)\n", pfx, met->name);
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

void metadb_metric_scope_free(metadb_metric_scope_t* scope, hpcfmt_free_fn dealloc) {
  if(dealloc == NULL) dealloc = free;
  dealloc(scope->scope);
}

int metadb_metric_scope_fwrite(const metadb_metric_scope_t* scope, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(scope->raw_mid, fs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(scope->num_summaries, fs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(scope->scope, fs));
  return HPCFMT_OK;
}

int metadb_metric_scope_fread(metadb_metric_scope_t* scope, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&scope->raw_mid, infs));
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&scope->num_summaries, infs));
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&scope->scope, infs));
  return HPCFMT_OK;
}

int metadb_metric_scope_fprint(const metadb_metric_scope_t* scope, FILE* fs, const char* pfx) {
  fprintf(fs, "%s[metric scope (header):\n", pfx);
  fprintf(fs, "%s  (raw_mid: %"PRIu16")\n", pfx, scope->raw_mid);
  fprintf(fs, "%s  (num_summaries: %"PRIu16")\n", pfx, scope->num_summaries);
  fprintf(fs, "%s  (scope: %s)\n", pfx, scope->scope);
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

void metadb_metric_summary_free(metadb_metric_summary_t* sum, hpcfmt_free_fn dealloc) {
  if(dealloc == NULL) dealloc = free;
  dealloc(sum->formula);
}

int metadb_metric_summary_fwrite(const metadb_metric_summary_t* sum, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(sum->summary_mid, fs));
  if(fwrite(&sum->combine, 1, 1, fs) < 1) return HPCFMT_ERR;
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fwrite(sum->formula, fs));
  return HPCFMT_OK;
}

int metadb_metric_summary_fread(metadb_metric_summary_t* sum, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&sum->summary_mid, infs));
  if(fread(&sum->combine, 1, 1, infs) < 1) return HPCFMT_ERR;
  HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&sum->formula, infs));
  return HPCFMT_OK;
}

int metadb_metric_summary_fprint(const metadb_metric_summary_t* sum, FILE* fs, const char* pfx) {
  fprintf(fs, "%s[summarized metric:\n", pfx);
  fprintf(fs, "%s  (summary_mid: %"PRIu16")\n", pfx, sum->summary_mid);
  fprintf(fs, "%s  (combine: %"PRIu8" = %s)\n", pfx, sum->combine,
          sum->combine == 0 ? "SUM" : sum->combine == 1 ? "MIN" :
          sum->combine == 2 ? "MAX" : "???");
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// load modules
//***************************************************************************
int metadb_loadmodule_fwrite(const metadb_loadmodule_t* mod, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(0, fs));  // flags
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(mod->path_ptr, fs));
  return HPCFMT_OK;
}

char* metadb_loadmodule_swrite(const metadb_loadmodule_t* mod, char* buf) {
  buf = hpcfmt_int4_swrite(0, buf);
  buf = hpcfmt_int8_swrite(mod->path_ptr, buf);
  return buf;
}

int metadb_loadmodule_fread(metadb_loadmodule_t* mod, FILE* infs) {
  uint32_t flags;
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&flags, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&mod->path_ptr, infs));
  return HPCFMT_OK;
}

int metadb_loadmodule_fprint(const metadb_loadmodule_t* mod, FILE* fs, FILE* infs, const char* pfx) {
  fprintf(fs, "%s[load module:\n", pfx);
  fprintf(fs, "%s  (path_ptr: 0x%"PRIx64")", pfx, mod->path_ptr);
  if(infs != NULL) {
    fseeko(infs, mod->path_ptr, SEEK_SET);
    char* path;
    HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&path, infs));
    fprintf(fs, " (path: %s)", path);
    free(path);
  }
  fprintf(fs, "\n");
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// source file
//***************************************************************************
#define HPCMETADB_FMT_SrcFileFlags_IsCopied 0x1

int metadb_srcfile_fwrite(const metadb_srcfile_t* file, FILE* fs) {
  uint32_t flags = 0
    | (file->is_copied ? HPCMETADB_FMT_SrcFileFlags_IsCopied : 0);
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(flags, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(file->path_ptr, fs));
  return HPCFMT_OK;
}

char* metadb_srcfile_swrite(const metadb_srcfile_t* file, char* buf) {
  uint32_t flags = 0
    | (file->is_copied ? HPCMETADB_FMT_SrcFileFlags_IsCopied : 0);
  buf = hpcfmt_int4_swrite(flags, buf);
  buf = hpcfmt_int8_swrite(file->path_ptr, buf);
  return buf;
}

int metadb_srcfile_fread(metadb_srcfile_t* file, FILE* infs) {
  uint32_t flags;
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&flags, infs));
  file->is_copied = (flags & HPCMETADB_FMT_SrcFileFlags_IsCopied) != 0;
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&file->path_ptr, infs));
  return HPCFMT_OK;
}

int metadb_srcfile_fprint(const metadb_srcfile_t* file, FILE* fs, FILE* infs, const char* pfx) {
  fprintf(fs, "%s[source file:\n", pfx);
  fprintf(fs, "%s  (path_ptr: 0x%"PRIx64")\n", pfx, file->path_ptr);
  if(infs != NULL) {
    fseeko(infs, file->path_ptr, SEEK_SET);
    char* path;
    HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&path, infs));
    fprintf(fs, "%s    (path: %s)\n", pfx, path);
    free(path);
  }
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// function
//***************************************************************************
int metadb_func_fwrite(const metadb_func_t* func, FILE* fs) {
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(0, fs));  // flags
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(func->name_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(func->module_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(func->module_ptr == 0 ? UINT64_C(-1)
                                         : func->module_offset, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(func->def_fileptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(func->def_fileptr == 0 ? UINT32_C(0)
                                         : func->def_line, fs));
  return HPCFMT_OK;
}

char* metadb_func_swrite(const metadb_func_t* func, char* buf) {
  buf = hpcfmt_int4_swrite(0, buf);  // flags
  buf = hpcfmt_int8_swrite(func->name_ptr, buf);
  buf = hpcfmt_int8_swrite(func->module_ptr, buf);
  buf = hpcfmt_int8_swrite(func->module_ptr == 0 ? UINT64_C(-1)
                           : func->module_offset, buf);
  buf = hpcfmt_int8_swrite(func->def_fileptr, buf);
  buf = hpcfmt_int4_swrite(func->def_fileptr == 0 ? UINT32_C(0)
                           : func->def_line, buf);
  return buf;
}

int metadb_func_fread(metadb_func_t* func, FILE* infs) {
  uint32_t flags;
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&flags, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&func->name_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&func->module_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&func->module_offset, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&func->def_fileptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&func->def_line, infs));
  return HPCFMT_OK;
}

int metadb_func_fprint(const metadb_func_t* func, FILE* fs, FILE* infs, const char* pfx) {
  fprintf(fs, "%s[function:\n", pfx);
  fprintf(fs, "%s  (name_ptr: 0x%"PRIx64")\n", pfx, func->name_ptr);
  if(func->name_ptr != 0 && infs != NULL) {
    fseeko(infs, func->name_ptr, SEEK_SET);
    char* path;
    HPCFMT_ThrowIfError(hpcfmt_nullstr_fread(&path, infs));
    fprintf(fs, "%s    (name: %s)\n", pfx, path);
    free(path);
  }
  fprintf(fs, "%s  (module_ptr: 0x%"PRIx64") (module_offset: 0x%"PRIx64")\n",
          pfx, func->module_ptr, func->module_offset);
  if(func->module_ptr != 0 && infs != NULL) {
    fseeko(infs, func->module_ptr, SEEK_SET);
    metadb_loadmodule_t mod;
    HPCFMT_ThrowIfError(metadb_loadmodule_fread(&mod, infs));
    char* newpfx = strccat(pfx, "    ");
    if(newpfx == NULL) return HPCFMT_ERR;
    HPCFMT_ThrowIfError(metadb_loadmodule_fprint(&mod, fs, infs, newpfx));
    free(newpfx);
  }
  fprintf(fs, "%s  (def_fileptr: 0x%"PRIx64") (def_line: %"PRIu32")\n",
          pfx, func->def_fileptr, func->def_line);
  if(func->def_fileptr != 0 && infs != NULL) {
    fseeko(infs, func->def_fileptr, SEEK_SET);
    metadb_srcfile_t file;
    HPCFMT_ThrowIfError(metadb_srcfile_fread(&file, infs));
    char* newpfx = strccat(pfx, "    ");
    if(newpfx == NULL) return HPCFMT_ERR;
    HPCFMT_ThrowIfError(metadb_srcfile_fprint(&file, fs, infs, newpfx));
    free(newpfx);
  }
  fprintf(fs, "%s]", pfx);
  return HPCFMT_OK;
}

//***************************************************************************
// context tree node
//***************************************************************************
int metadb_ctxnode_fwrite(const metadb_ctxnode_t* ctx, FILE* fs) {
  // Calculate the sub-sizes before writing the header
  uint8_t callee_size = 0;
  if(ctx->call_type != HPCMETADB_FMT_CALL_None) {
    callee_size += 2;
    if(ctx->caller_fileptr != 0) callee_size += 12;
  }
  uint8_t lexical_size = 3;
  if(ctx->func_ptr != 0) lexical_size += 8;
  if(ctx->src_fileptr != 0) lexical_size += 12;
  if(ctx->module_ptr != 0) lexical_size += 16;

  // Constant context-node header
  HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(ctx->ctx_id, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->block_ptr, fs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->block_size, fs));
  if(fwrite(&callee_size, 1, 1, fs) < 1) return HPCFMT_ERR;
  if(fwrite(&lexical_size, 1, 1, fs) < 1) return HPCFMT_ERR;

  // Callee specification
  if(ctx->call_type != HPCMETADB_FMT_CALL_None) {
    if(fwrite(&ctx->call_type, 1, 1, fs) < 1) return HPCFMT_ERR;
    uint8_t call_flags = 0
      | (ctx->caller_fileptr != 0 ? 0x1 : 0);  // has_caller_srcline
    if(fwrite(&call_flags, 1, 1, fs) < 1) return HPCFMT_ERR;

    if(ctx->caller_fileptr != 0) {
      HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->caller_fileptr, fs));
      HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(ctx->caller_line, fs));
    }
  }

  // Lexical specification
  if(fwrite(&ctx->lex_type, 1, 1, fs) < 1) return HPCFMT_ERR;
  uint16_t lex_flags = 0
    | (ctx->func_ptr != 0 ? 0x1 : 0)  // has_function
    | (ctx->src_fileptr != 0 ? 0x2 : 0)  // has_srcline
    | (ctx->module_ptr != 0 ? 0x4 : 0)  // has_point
    | (ctx->is_call_srcline ? 0x8 : 0);
  HPCFMT_ThrowIfError(hpcfmt_int2_fwrite(lex_flags, fs));

  if(ctx->func_ptr != 0) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->func_ptr, fs));
  }
  if(ctx->src_fileptr != 0) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->src_fileptr, fs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fwrite(ctx->src_line, fs));
  }
  if(ctx->module_ptr != 0) {
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->module_ptr, fs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fwrite(ctx->module_offset, fs));
  }

  return HPCFMT_OK;
}

int metadb_ctxnode_fread(metadb_ctxnode_t* ctx, FILE* infs) {
  HPCFMT_ThrowIfError(hpcfmt_int4_fread(&ctx->ctx_id, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->block_ptr, infs));
  HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->block_size, infs));
  uint8_t callee_size;
  if(fread(&callee_size, 1, 1, infs) < 1) return HPCFMT_ERR;
  uint8_t lexical_size;
  if(fread(&lexical_size, 1, 1, infs) < 1) return HPCFMT_ERR;

  // Callee specification
  ctx->call_type = HPCMETADB_FMT_CALL_None;
  ctx->caller_fileptr = 0;
  if(callee_size > 0) {
    if(callee_size < 2) return HPCFMT_ERR;
    callee_size -= 2;
    if(fread(&ctx->call_type, 1, 1, infs) < 1) return HPCFMT_ERR;
    uint8_t call_flags;
    if(fread(&call_flags, 1, 1, infs) < 1) return HPCFMT_ERR;

    if((call_flags & 0x1) != 0) {  // has_caller_srcline
      if(callee_size < 12) return HPCFMT_ERR;
      callee_size -= 12;
      HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->caller_fileptr, infs));
      HPCFMT_ThrowIfError(hpcfmt_int4_fread(&ctx->caller_line, infs));
    }

    if(callee_size > 0)
      if(fseek(infs, callee_size, SEEK_CUR) != 0) return HPCFMT_ERR;
  }

  // Lexical specification
  if(lexical_size < 3) return HPCFMT_ERR;
  lexical_size -= 3;
  if(fread(&ctx->lex_type, 1, 1, infs) < 1) return HPCFMT_ERR;
  uint16_t lex_flags;
  HPCFMT_ThrowIfError(hpcfmt_int2_fread(&lex_flags, infs));
  ctx->is_call_srcline = (lex_flags & 0x8) != 0;

  ctx->func_ptr = 0;
  if((lex_flags & 0x1) != 0) {  // has_function
    if(lexical_size < 8) return HPCFMT_ERR;
    lexical_size -= 8;
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->func_ptr, infs));
  }

  ctx->src_fileptr = 0;
  if((lex_flags & 0x2) != 0) {  // has_srcline
    if(lexical_size < 12) return HPCFMT_ERR;
    lexical_size -= 12;
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->src_fileptr, infs));
    HPCFMT_ThrowIfError(hpcfmt_int4_fread(&ctx->src_line, infs));
  }

  ctx->module_ptr = 0;
  if((lex_flags & 0x4) != 0) {  // has_point
    if(lexical_size < 16) return HPCFMT_ERR;
    lexical_size -= 16;
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->module_ptr, infs));
    HPCFMT_ThrowIfError(hpcfmt_int8_fread(&ctx->module_offset, infs));
  }

  if(lexical_size > 0)
    if(fseek(infs, lexical_size, SEEK_CUR) != 0) return HPCFMT_ERR;

  return HPCFMT_OK;
}

int metadb_ctxnode_fprint_long(const metadb_ctxnode_t* ctx, FILE* fs, FILE* infs, const char* pfx) {
  fprintf(fs, "%s[context node:\n", pfx);
  fprintf(fs, "%s  (ctx_id: %"PRIu32")\n", pfx, ctx->ctx_id);
  fprintf(fs, "%s  (block_ptr: 0x%"PRIx64") (block_size: 0x%"PRIx64")\n",
          pfx, ctx->block_ptr, ctx->block_size);
  if(ctx->call_type == HPCMETADB_FMT_CALL_None) {
    fprintf(fs, "%s  [not a callee]\n", pfx);
  } else {
    fprintf(fs, "%s  [callee:\n", pfx);
    fprintf(fs, "%s    (call_type: %"PRIu8" = %s)\n", pfx, ctx->call_type,
            ctx->call_type == HPCMETADB_FMT_CALL_Normal ? "Normal" :
            ctx->call_type == HPCMETADB_FMT_CALL_Inlined ? "Inlined" :
            "???");
    fprintf(fs, "%s    (caller_fileptr: 0x%"PRIx64") (caller_line: %"PRIu32")\n",
            pfx, ctx->caller_fileptr, ctx->caller_line);
    if(ctx->caller_fileptr != 0 && infs != NULL) {
      fseeko(infs, ctx->caller_fileptr, SEEK_SET);
      metadb_srcfile_t file;
      HPCFMT_ThrowIfError(metadb_srcfile_fread(&file, infs));
      char* newpfx = strccat(pfx, "      ");
      if(newpfx == NULL) return HPCFMT_ERR;
      HPCFMT_ThrowIfError(metadb_srcfile_fprint(&file, fs, infs, newpfx));
      free(newpfx);
    }
    fprintf(fs, "%s  ]\n", pfx);
  }
  fprintf(fs, "%s  (lex_type: %"PRIu8" = %s)\n", pfx, ctx->lex_type,
          ctx->lex_type == HPCMETADB_FMT_LEX_Function ? "Function" :
          ctx->lex_type == HPCMETADB_FMT_LEX_Loop ? "Loop" :
          ctx->lex_type == HPCMETADB_FMT_LEX_SrcLine ? "SrcLine" :
          ctx->lex_type == HPCMETADB_FMT_LEX_Point ? "Point" :
          "???");
  fprintf(fs, "%s  (func_ptr: 0x%"PRIx64")\n", pfx, ctx->func_ptr);
  if(ctx->func_ptr != 0 && infs != NULL) {
    fseeko(infs, ctx->func_ptr, SEEK_SET);
    metadb_func_t func;
    HPCFMT_ThrowIfError(metadb_func_fread(&func, infs));
    char* newpfx = strccat(pfx, "    ");
    if(newpfx == NULL) return HPCFMT_ERR;
    HPCFMT_ThrowIfError(metadb_func_fprint(&func, fs, infs, newpfx));
    free(newpfx);
  }
  fprintf(fs, "%s  (src_fileptr: 0x%"PRIx64") (src_line: %"PRIu32")\n", pfx,
          ctx->src_fileptr, ctx->src_line);
  if(ctx->src_fileptr != 0 && infs != NULL) {
    fseeko(infs, ctx->src_fileptr, SEEK_SET);
    metadb_srcfile_t file;
    HPCFMT_ThrowIfError(metadb_srcfile_fread(&file, infs));
    char* newpfx = strccat(pfx, "    ");
    if(newpfx == NULL) return HPCFMT_ERR;
    HPCFMT_ThrowIfError(metadb_srcfile_fprint(&file, fs, infs, newpfx));
    free(newpfx);
  }
  fprintf(fs, "%s  (module_ptr: 0x%"PRIx64") (module_offset: 0x%"PRIx64")\n",
          pfx, ctx->module_ptr, ctx->module_offset);
  if(ctx->module_ptr != 0 && infs != NULL) {
    fseeko(infs, ctx->module_ptr, SEEK_SET);
    metadb_loadmodule_t mod;
    HPCFMT_ThrowIfError(metadb_loadmodule_fread(&mod, infs));
    char* newpfx = strccat(pfx, "    ");
    if(newpfx == NULL) return HPCFMT_ERR;
    HPCFMT_ThrowIfError(metadb_loadmodule_fprint(&mod, fs, infs, newpfx));
    free(newpfx);
  }
  fprintf(fs, "%s]\n", pfx);
  return HPCFMT_OK;
}
