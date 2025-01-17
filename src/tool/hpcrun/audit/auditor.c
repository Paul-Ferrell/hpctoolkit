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


//******************************************************************************
// global includes
//******************************************************************************

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/stat.h>
#include <sys/auxv.h>
#include <sys/wait.h>
#include <link.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>
#include <sys/mman.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "audit-api.h"



//******************************************************************************
// macros
//******************************************************************************

// define the architecture-specific GOT index where the address of loader's 
// resolver function can be found

#if defined(HOST_CPU_x86_64) || defined(HOST_CPU_x86)
#define GOT_resolver_index 2
#elif defined(HOST_CPU_PPC)
#define GOT_resolver_index 0
#elif defined(HOST_CPU_ARM64)
#define GOT_resolver_index 2
#else
#error "GOT resolver index for the host architecture is unknown"
#endif



//******************************************************************************
// type declarations
//******************************************************************************

enum hpcrun_state {
  state_awaiting, state_found, state_attached,
  state_connecting, state_connected, state_disconnected,
};


struct buffered_entry_t {
  struct link_map* map;
  Lmid_t lmid;
  uintptr_t* cookie;

  struct buffered_entry_t* next;
} *buffer = NULL;


enum audit_open_flags {
  AO_NONE = 0x0,
  AO_VDSO = 0x1,
};


struct phdrs_t {
  ElfW(Phdr)* phdrs;
  size_t phnum;
};


//******************************************************************************
// local data
//******************************************************************************

static bool verbose = false;
static char* mainlib = NULL;
static bool disable_plt_call_opt = false;
static ElfW(Addr) dl_runtime_resolver_ptr = 0;

static enum hpcrun_state state = state_awaiting;

static auditor_hooks_t hooks;
static auditor_attach_pfn_t pfn_init = NULL;
static uintptr_t* mainlib_cookie = NULL;

static void mainlib_connected(const char*);

static auditor_exports_t exports = {
  .mainlib_connected = mainlib_connected,
  .pipe = pipe, .close = close, .waitpid = waitpid,
  .clone = clone, .execve = execve
};



//******************************************************************************
// private operations
//******************************************************************************

static struct phdrs_t get_phdrs(struct link_map* map) {
  // Main (non-PIE?) executable
  if(map->l_addr == 0) {
    // This should never happen, if we're loaded we should have the same
    // architecture as the application.
    if(getauxval(AT_PHENT) != sizeof(ElfW(Phdr))) abort();

    return (struct phdrs_t){(void*)getauxval(AT_PHDR), getauxval(AT_PHNUM)};
  }

  // Otherwise l_addr points to an Elf header
  ElfW(Ehdr)* hdr = (void*)map->l_addr;
  return (struct phdrs_t){(void*)map->l_addr + hdr->e_phoff, hdr->e_phnum};
}

static ElfW(Addr) * get_plt_got_start(ElfW(Dyn) * dyn_init) {
  ElfW(Dyn) *dyn;
  for (dyn = dyn_init; dyn->d_tag != DT_NULL; dyn++) {
    if (dyn->d_tag == DT_PLTGOT) {
      return (ElfW(Addr) *) dyn->d_un.d_ptr;
    }
  }
  return NULL;
}


// Helper to call the open hook, when you only have the link_map.
static void hook_open(uintptr_t* cookie, struct link_map* map, enum audit_open_flags ao_flags) {
  // Allocate some space for our extra bits, and fill it.
  auditor_map_entry_t* entry = malloc(sizeof *entry);
  entry->map = map;
  entry->ehdr = NULL;

  // Normally the path is map->l_name, but sometimes that string is empty
  // which indicates the main executable. So we get it the other way.
  entry->path = NULL;
  if(map->l_name[0] == '\0')
    entry->path = realpath((const char*)getauxval(AT_EXECFN), NULL);
  else
    entry->path = realpath(map->l_name, NULL);

  // Find the phdrs for this here binary. Depending on bits it can be tricky.
  struct phdrs_t phdrs = get_phdrs(map);

  // Use the phdrs to calculate the range of executable bits as well as the
  // real base address.
  uintptr_t start = UINTPTR_MAX;
  uintptr_t end = 0;
  for(size_t i = 0; i < phdrs.phnum; i++) {
    if(phdrs.phdrs[i].p_type == PT_LOAD) {
      if(!entry->ehdr)
        entry->ehdr = (void*)(uintptr_t)(phdrs.phdrs[i].p_vaddr - phdrs.phdrs[i].p_offset);
      if((phdrs.phdrs[i].p_flags & PF_X) != 0 && phdrs.phdrs[i].p_memsz > 0) {
        if(phdrs.phdrs[i].p_vaddr < start) start = phdrs.phdrs[i].p_vaddr;
        if(phdrs.phdrs[i].p_vaddr + phdrs.phdrs[i].p_memsz > end)
          end = phdrs.phdrs[i].p_vaddr + phdrs.phdrs[i].p_memsz;
      }
    } else if(phdrs.phdrs[i].p_type == PT_DYNAMIC && map->l_ld == NULL) {
      map->l_ld = (void*)(map->l_addr + phdrs.phdrs[i].p_vaddr);
    }
  }

  // load module.  we must adjust it by start so that it has the proper
  // base address value for the subsequent calculations.
  if (ao_flags & AO_VDSO) map->l_addr -= (intptr_t) start;

  entry->start = (void*)map->l_addr + (intptr_t) start;
  entry->end = (void*)map->l_addr + (intptr_t) end;

  // Since we don't use dl_iterate_phdr, we have to reconsitute its data.
  entry->dl_info.dlpi_addr = (ElfW(Addr))map->l_addr;
  entry->dl_info.dlpi_name = entry->path;
  entry->dl_info.dlpi_phdr = phdrs.phdrs;
  entry->dl_info.dlpi_phnum = phdrs.phnum;
  entry->dl_info.dlpi_adds = 0;
  entry->dl_info.dlpi_subs = 0;
  entry->dl_info.dlpi_tls_modid = 0;
  entry->dl_info.dlpi_tls_data = NULL;
  entry->dl_info_sz = offsetof(struct dl_phdr_info, dlpi_phnum)
                      + sizeof entry->dl_info.dlpi_phnum;

  if(verbose)
    fprintf(stderr, "[audit] Delivering objopen for `%s' [%p, %p)"
	    " dl_info.dlpi_addr = %p\n", entry->path, entry->start, 
	    entry->end, (void*)entry->dl_info.dlpi_addr);

  hooks.open(entry);

  if(cookie)
    *cookie = (uintptr_t)entry;
  else free(entry);
}


// Search a link_map's (dynamic) symbol table until we find the given symbol.
static void* get_symbol(struct link_map* map, const char* name) {
  // Nab the STRTAB and SYMTAB
  const char* strtab = NULL;
  ElfW(Sym)* symtab = NULL;
  for(const ElfW(Dyn)* d = map->l_ld; d->d_tag != DT_NULL; d++) {
    if(d->d_tag == DT_STRTAB) strtab = (const char*)d->d_un.d_ptr;
    else if(d->d_tag == DT_SYMTAB) symtab = (ElfW(Sym)*)d->d_un.d_ptr;
    if(strtab && symtab) break;
  }
  if(!strtab || !symtab) abort();  // This should absolutely never happen

  // Hunt down the given symbol. It must exist, or we run off the end.
  for(ElfW(Sym)* s = symtab; ; s++) {
    if(ELF64_ST_TYPE(s->st_info) != STT_FUNC) continue;
    if(strcmp(&strtab[s->st_name], name) == 0)
      return (void*)(map->l_addr + s->st_value);
  }
}


static void enable_writable_got(struct link_map* map) {
  static uintptr_t pagemask = 0;
  if(pagemask == 0) pagemask = ~(getpagesize()-1);

  struct phdrs_t phdrs = get_phdrs(map);
  for(size_t i = 0; i < phdrs.phnum; i++) {
    // The GOT (and some other stuff) is listed in "a segment which may be made
    // read-only after relocations have been processed."
    // If we don't see this we assume the linker won't make the GOT unwritable.
    if(phdrs.phdrs[i].p_type == PT_GNU_RELRO) {
      // We include PROT_EXEC since the loader may share these pages with
      // other unrelated sections, such as code regions.
      //
      // We also have to open up the entire GOT table since we can't trust
      // the linker to leave the it open for the resolver in the LD_AUDIT case.
      void* start = (void*)(((uintptr_t)map->l_addr + phdrs.phdrs[i].p_vaddr) & pagemask);
      void* end = (void*)map->l_addr + phdrs.phdrs[i].p_vaddr + phdrs.phdrs[i].p_memsz;
      mprotect(start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
  }
}

static void optimize_object_plt(struct link_map* map) {
  ElfW(Addr)* plt_got = get_plt_got_start(map->l_ld);
  if (plt_got != NULL) {
    // If the original entry is already optimized, silently skip
    if(plt_got[GOT_resolver_index] == dl_runtime_resolver_ptr)
      return;

    // If the original entry is NULL, we skip it (obviously something is wrong)
    if(plt_got[GOT_resolver_index] == 0) {
      if(verbose)
        fprintf(stderr, "[audit] Skipping optimization of `%s', original entry %p is NULL\n", map->l_name, &plt_got[GOT_resolver_index]);
      return;
    }

    // Print out some debugging information
    if(verbose) {
      Dl_info info;
      Dl_info info2;
      if(!dladdr((void*)plt_got[GOT_resolver_index], &info))
        info = (Dl_info){NULL, NULL, NULL, NULL};
      if(!dladdr((void*)dl_runtime_resolver_ptr, &info2))
        info = (Dl_info){NULL, NULL, NULL, NULL};
      if(info.dli_fname != NULL && info2.dli_fname != NULL
         && strcmp(info.dli_fname, info2.dli_fname) == 0)
        info2.dli_fname = "...";
      fprintf(stderr, "[audit] Optimizing `%s': %p (%s+%p) -> %p (%s+%p)\n",
              map->l_name,
              (void*)plt_got[GOT_resolver_index], info.dli_fname,
              (void*)plt_got[GOT_resolver_index]-(ptrdiff_t)info.dli_fbase,
              (void*)dl_runtime_resolver_ptr, info2.dli_fname,
              (void*)dl_runtime_resolver_ptr-(ptrdiff_t)info2.dli_fbase);
    }

    // Enable write perms for the GOT, and overwrite the resolver entry
    enable_writable_got(map);
    plt_got[GOT_resolver_index] = dl_runtime_resolver_ptr;
  } else if(verbose) {
    fprintf(stderr, "[audit] Failed to find GOTPLT section in `%s'!\n", map->l_name);
  }
}

uintptr_t la_symbind32(Elf32_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt(state < state_connected ? (struct link_map*)*refcook : ((auditor_map_entry_t*)*refcook)->map);
  return sym->st_value;
}
uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt(state < state_connected ? (struct link_map*)*refcook : ((auditor_map_entry_t*)*refcook)->map);
  return sym->st_value;
}


// Transition to connected, once the mainlib is ready.
static void mainlib_connected(const char* vdso_path) {
  if(state >= state_connected) return;
  if(state < state_attached) {
    fprintf(stderr, "[audit] Attempt to connect before attached!\n");
    abort();
  }

  // Reverse the stack of buffered notifications, so they get reported in order.
  struct buffered_entry_t* queue = NULL;
  while(buffer != NULL) {
   struct buffered_entry_t* next = buffer->next;
   buffer->next = queue;
   queue = buffer;
   buffer = next;
  }

  // Drain the buffer and deliver everything that happened before time begins.
  if(verbose)
    fprintf(stderr, "[audit] Draining buffered objopens\n");
  while(queue != NULL) {
    hook_open(queue->cookie, queue->map, AO_NONE);
    struct buffered_entry_t* next = queue->next;
    free(queue);
    queue = next;
  }

  // Try to get our own linkmap, and let the mainlib know about us.
  // Obviously there is no cookie, we never notify a close from these.
  struct link_map* map;
  Dl_info info;
  dladdr1(la_version, &info, (void**)&map, RTLD_DL_LINKMAP);
  hook_open(NULL, map, AO_NONE);

  // Add an entry for vDSO, because we don't get it otherwise.
  uintptr_t vdso = getauxval(AT_SYSINFO_EHDR);
  if(vdso != 0) {
    struct link_map* mvdso = malloc(sizeof *mvdso);
    mvdso->l_addr = vdso;
    mvdso->l_name = vdso_path ? (char*)vdso_path : "[vdso]";
    mvdso->l_ld = NULL;  // NOTE: Filled by hook_open
    mvdso->l_next = mvdso->l_prev = NULL;
    hook_open(NULL, mvdso, AO_VDSO);
  }

  if(verbose)
    fprintf(stderr, "[audit] Auditor is now connected\n");
  state = state_connected;
}



//******************************************************************************
// interface operations
//******************************************************************************

unsigned int la_version(unsigned int version) {
  if(version < 1) return 0;

  // Read in our arguments
  verbose = getenv("HPCRUN_AUDIT_DEBUG");

  // Check if we need to optimize PLT calls
  disable_plt_call_opt = getenv("HPCRUN_AUDIT_DISABLE_PLT_CALL_OPT");
  if (!disable_plt_call_opt) {
    ElfW(Addr)* plt_got = get_plt_got_start(_DYNAMIC);
    if (plt_got != NULL) {
      dl_runtime_resolver_ptr = plt_got[GOT_resolver_index];
      if(verbose)
        fprintf(stderr, "[audit] PLT optimized resolver: %p\n", (void*)dl_runtime_resolver_ptr);
    } else if(verbose) {
      fprintf(stderr, "[audit] Failed to find PLTGOT section in auditor!\n");
    }
  }

  mainlib = realpath(getenv("HPCRUN_AUDIT_MAIN_LIB"), NULL);
  if(verbose)
    fprintf(stderr, "[audit] Awaiting mainlib `%s'\n", mainlib);

  // Generate the purified environment before the app changes it
  // NOTE: Consider removing only ourselves to allow for Spindle-like optimizations.
  {
    size_t envsz = 0;
    for(char** e = environ; *e != NULL; e++) envsz++;
    exports.pure_environ = malloc((envsz+1)*sizeof exports.pure_environ[0]);
    size_t idx = 0;
    for(char** e = environ; *e != NULL; e++) {
      if(strncmp(*e, "LD_PRELOAD=", 11) == 0) continue;
      if(strncmp(*e, "LD_AUDIT=", 9) == 0) continue;
      exports.pure_environ[idx++] = *e;
    }
    exports.pure_environ[idx] = NULL;
  }

  return 1;
}


unsigned int la_objopen(struct link_map* map, Lmid_t lmid, uintptr_t* cookie) {
  switch(state) {
  case state_awaiting: {
    // If this is libhpcrun.so, nab the initialization bits and transition.
    char* path = realpath(map->l_name, NULL);
    if(path) {
      if(strcmp(path, mainlib) == 0) {
        if(verbose)
          fprintf(stderr, "[audit] Located tracker library.\n");
        free(mainlib);
        mainlib_cookie = cookie;
        pfn_init = get_symbol(map, "hpcrun_auditor_attach");
        if(verbose)
          fprintf(stderr, "[audit] Found init hook: %p\n", pfn_init);
        state = state_found;
      }
      free(path);
    }
    // fallthrough
  }
  case state_found:
  case state_attached:
  case state_connecting: {
    // Buffer operations that happen before the connection
    struct buffered_entry_t* entry = malloc(sizeof *entry);
    if(verbose)
      fprintf(stderr, "[audit] Buffering objopen before connection: `%s'\n", map->l_name);
    *entry = (struct buffered_entry_t){
      .map = map, .lmid = lmid, .cookie = cookie, .next = buffer,
    };
    buffer = entry;
    return LA_FLG_BINDFROM | LA_FLG_BINDTO;
  }
  case state_connected:
    // If we're already connected, just call the hook.
    hook_open(cookie, map, AO_NONE);
    return LA_FLG_BINDFROM | LA_FLG_BINDTO;
  case state_disconnected:
    // We just ignore things that happen after disconnection.
    if(verbose)
      fprintf(stderr, "[audit] objopen after disconnection: `%s'\n", map->l_name);
    return LA_FLG_BINDFROM | LA_FLG_BINDTO;
  }
  abort();  // unreachable
}


void la_activity(uintptr_t* cookie, unsigned int flag) {
  static unsigned int previous = LA_ACT_CONSISTENT;

  if(flag == LA_ACT_CONSISTENT) {
    if(verbose)
      fprintf(stderr, "[audit] la_activity: LA_CONSISTENT\n");

    // If we've hit consistency and know where libhpcrun is, initialize it.
    switch(state) {
    case state_awaiting:
      break;
    case state_found:
      if(verbose)
        fprintf(stderr, "[audit] Attaching to mainlib\n");
      pfn_init(&exports, &hooks);
      state = state_attached;
      break;
#if 0
    // early initialization can cause a SEGV with clang offloading at LLNL
    // using libxlsmp. libxlsmp loads cuda, the auditor intervenes, the auditor
    // tries to start hpctoolkit, which dlopens cuda when "-e gpu=nvidia". 
    // in this circumstance, cuda is not properly initialized before 
    // hpctoolkit tries to use it.
    case state_attached: {
      if(previous == LA_ACT_ADD) {
        if(verbose)
          fprintf(stderr, "[audit] Beginning early initialization\n");
        state = state_connecting;
        hooks.initialize();
      }
      break;
    }
#endif
    case state_connecting:
      // The mainlib is still initializing, we can skip this notification.
      break;
    case state_connected:
      if(verbose)
        fprintf(stderr, "[audit] Notifying stability (additive: %d)\n",
                previous == LA_ACT_ADD);
      hooks.stable(previous == LA_ACT_ADD);
      break;
    case state_disconnected:
      break;
    }
  } else if(verbose) {
    if(flag == LA_ACT_ADD)
      fprintf(stderr, "[audit] la_activity: LA_ADD\n");
    else if(flag == LA_ACT_DELETE)
      fprintf(stderr, "[audit] la_activity: LA_DELETE\n");
    else
      fprintf(stderr, "[audit] la_activity: %d\n", flag);
  }
  previous = flag;
}


void la_preinit(uintptr_t* cookie) {
  if(state == state_attached) {
    if(verbose)
      fprintf(stderr, "[audit] Beginning late initialization\n");
    state = state_connecting;
    hooks.initialize();
  }
}


unsigned int la_objclose(uintptr_t* cookie) {
  switch(state) {
  case state_awaiting:
  case state_found:
  case state_attached:
  case state_connecting:
    // Scan through the buffer for the matching entry, and remove it.
    for(struct buffered_entry_t *e = buffer, *p = NULL; e != NULL;
        p = e, e = e->next) {
      if(e->cookie == cookie) {
        if(verbose)
          fprintf(stderr, "[audit] Buffering objclose before connection: `%s'\n",
                  e->map->l_name);
        if(p != NULL) p->next = e->next;
        else buffer = e->next;
        free(e);
        break;
      }
    }
    break;
  case state_connected:
    if(mainlib_cookie == cookie) state = state_disconnected;
    else if(*cookie != 0) {
      auditor_map_entry_t* entry = *(void**)cookie;
      if(verbose)
        fprintf(stderr, "[audit] Delivering objclose for `%s'\n", entry->path);
      hooks.close(entry);
      free(entry);
    }
    break;
  case state_disconnected:
    // We just ignore things that happen after disconnection.
    break;
  }
  *cookie = 0;
  return 0;
}
