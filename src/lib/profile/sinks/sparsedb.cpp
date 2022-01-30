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

#include "sparsedb.hpp"

#include "../mpi/all.hpp"
#include "../util/log.hpp"
#include "lib/prof-lean/hpcrun-fmt.h"
#include "lib/prof-lean/id-tuple.h"
#include "lib/prof/cms-format.h"
#include "lib/prof/pms-format.h"
#include "lib/profile/sinks/FORMATS.md.inc"

#include <cassert>
#include <cmath>
#include <fstream>
#include <omp.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

using namespace hpctoolkit;
using namespace hpctoolkit::sinks;

static uint32_t readAsByte4(util::File::Instance& fh, uint64_t off) {
  uint32_t v = 0;
  int shift = 0, num_reads = 0;
  char input[4];

  fh.readat(off, 4, input);

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}

static uint16_t interpretByte2(const char* input) {
  uint16_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    v |= ((uint16_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}
static uint32_t interpretByte4(const char* input) {
  uint32_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    v |= ((uint32_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}
static uint64_t interpretByte8(const char* input) {
  uint64_t v = 0;
  int shift = 0, num_reads = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    v |= ((uint64_t)(input[num_reads] & 0xff) << shift);
    num_reads++;
  }

  return v;
}

static char* insertByte2(char* bytes, uint16_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 8; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 2;
}
static char* insertByte4(char* bytes, uint32_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 24; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 4;
}
static char* insertByte8(char* bytes, uint64_t val) {
  int shift = 0, num_writes = 0;

  for (shift = 56; shift >= 0; shift -= 8) {
    bytes[num_writes] = (val >> shift) & 0xff;
    num_writes++;
  }
  return bytes + 8;
}

static std::vector<char> convertToByte2(uint16_t val) {
  std::vector<char> bytes(2);
  insertByte2(bytes.data(), val);
  return bytes;
}
static std::vector<char> convertToByte4(uint32_t val) {
  std::vector<char> bytes(4);
  insertByte4(bytes.data(), val);
  return bytes;
}
static std::vector<char> convertToByte8(uint64_t val) {
  std::vector<char> bytes(8);
  insertByte8(bytes.data(), val);
  return bytes;
}

static std::array<char, PMS_real_hdr_SIZE> composeProfHdr(
    uint32_t nProf, uint64_t profInfoPtr, uint64_t profInfoSize, uint64_t idTuplesPtr,
    uint64_t idTuplesSize) {
  std::array<char, PMS_real_hdr_SIZE> out;
  char* cur = out.data();
  cur = std::copy(
      HPCPROFILESPARSE_FMT_Magic, HPCPROFILESPARSE_FMT_Magic + HPCPROFILESPARSE_FMT_MagicLen, cur);
  *(cur++) = HPCPROFILESPARSE_FMT_VersionMajor;
  *(cur++) = HPCPROFILESPARSE_FMT_VersionMinor;

  cur = insertByte4(cur, nProf);
  cur = insertByte2(cur, HPCPROFILESPARSE_FMT_NumSec);
  cur = insertByte8(cur, profInfoSize);
  cur = insertByte8(cur, profInfoPtr);
  cur = insertByte8(cur, idTuplesSize);
  cur = insertByte8(cur, idTuplesPtr);
  return out;
}

static size_t sizeofIdTuple(const id_tuple_t& tuple) {
  return PMS_id_tuple_len_SIZE + tuple.length * PMS_id_SIZE;
}

static char* insertIdTuple(char* cur, const id_tuple_t& tuple) {
  cur = insertByte2(cur, (uint16_t)tuple.length);
  for (unsigned int i = 0; i < tuple.length; i++) {
    const auto& id = tuple.ids[i];
    cur = insertByte2(cur, id.kind);
    cur = insertByte8(cur, id.physical_index);
    cur = insertByte8(cur, id.logical_index);
  }
  return cur;
}

static std::vector<char> composeIdTuples(const std::vector<id_tuple_t>& tuples) {
  std::vector<char> buf;
  for (const auto& tuple : tuples) {
    auto oldsz = buf.size();
    buf.resize(oldsz + sizeofIdTuple(tuple), 0);
    insertIdTuple(&buf[oldsz], tuple);
  }
  return buf;
}

static pms_profile_info_t parseProfInfo(const char* input) {
  pms_profile_info_t pi;
  pi.id_tuple_ptr = interpretByte8(input);
  pi.metadata_ptr = interpretByte8(input + PMS_id_tuple_ptr_SIZE);
  pi.spare_one = interpretByte8(input + PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE);
  pi.spare_two =
      interpretByte8(input + PMS_id_tuple_ptr_SIZE + PMS_metadata_ptr_SIZE + PMS_spare_one_SIZE);
  pi.num_vals = interpretByte8(input + PMS_ptrs_SIZE);
  pi.num_nzctxs = interpretByte4(input + PMS_ptrs_SIZE + PMS_num_val_SIZE);
  pi.offset = interpretByte8(input + PMS_ptrs_SIZE + PMS_num_val_SIZE + PMS_num_nzctx_SIZE);
  return pi;
}

static std::array<char, CMS_real_hdr_SIZE> composeCtxHdr(uint32_t ctxcnt) {
  std::array<char, CMS_real_hdr_SIZE> out;
  char* cur = out.data();
  cur = std::copy(HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_Magic + HPCCCTSPARSE_FMT_MagicLen, cur);
  *(cur++) = HPCCCTSPARSE_FMT_VersionMajor;
  *(cur++) = HPCCCTSPARSE_FMT_VersionMinor;

  cur = insertByte4(cur, ctxcnt);                      // num_ctx
  cur = insertByte2(cur, HPCCCTSPARSE_FMT_NumSec);     // num_sec
  cur = insertByte8(cur, ctxcnt * CMS_ctx_info_SIZE);  // ci_size
  cur = insertByte8(cur, CMS_hdr_SIZE);                // ci_ptr
  return out;
}

static char* insertCtxInfo(char* cur, cms_ctx_info_t ci) {
  cur = insertByte4(cur, ci.ctx_id);
  cur = insertByte8(cur, ci.num_vals);
  cur = insertByte2(cur, ci.num_nzmids);
  cur = insertByte8(cur, ci.offset);
  return cur;
}

SparseDB::SparseDB(stdshim::filesystem::path p)
    : dir(std::move(p)), accFpos(mpi::Tag::SparseDB_1), groupCounter(mpi::Tag::SparseDB_2) {
  if (dir.empty())
    util::log::fatal{} << "SparseDB doesn't allow for dry runs!";
  else
    stdshim::filesystem::create_directory(dir);
}

util::WorkshareResult SparseDB::help() {
  auto res = parForPi.contribute();
  if (!res.completed)
    return res;
  res = forProfilesParse.contribute();
  if (!res.completed)
    return res;
  return forProfilesLoad.contribute() + forEachContextRange.contribute();
}

void SparseDB::notifyPipeline() noexcept {
  src.registerOrderedWavefront();
  src.registerOrderedWrite();
  auto& ss = src.structs();
  ud = ss.context.add_default<udContext>();
}

void SparseDB::notifyWavefront(DataClass d) noexcept {
  if (!d.hasContexts() || !d.hasThreads())
    return;
  auto mpiSem = src.enterOrderedWavefront();
  auto sig = contextWavefront.signal();

  // Fill contexts with the sorted list of Contexts
  assert(contexts.empty());
  src.contexts().citerate([&](const Context& c) { contexts.push_back(c); }, nullptr);
  std::sort(contexts.begin(), contexts.end(), [this](const Context& a, const Context& b) -> bool {
    return a.userdata[src.identifier()] < b.userdata[src.identifier()];
  });
  assert(
      std::adjacent_find(
          contexts.begin(), contexts.end(),
          [this](const Context& a, const Context& b) -> bool {
            return a.userdata[src.identifier()] == b.userdata[src.identifier()];
          })
      == contexts.end());

  // Pop open profile.db and synchronize across the ranks.
  pmf = util::File(dir / "profile.db", true);
  pmf->synchronize();

  // Count the total number of profiles across all ranks
  size_t myNProf = src.threads().size();
  if (mpi::World::rank() == 0)
    myNProf++;  // Counting the summary profile
  auto nProf = mpi::allreduce(myNProf, mpi::Op::sum());

  // Lay out some bits of the profile.db
  prof_info_sec_ptr = PMS_hdr_SIZE;
  prof_info_sec_size = nProf * PMS_prof_info_SIZE;
  id_tuples_sec_ptr = prof_info_sec_ptr + MULTIPLE_8(prof_info_sec_size);

  // Figure out the absolute index of our first profile.
  if (mpi::World::rank() == 0) {
    // Rank 0 always starts at 0 since it handles the summary profile.
    min_prof_info_idx = 0;
  } else {
    min_prof_info_idx = std::numeric_limits<uint32_t>::max();
    for (const auto& t : src.threads().citerate()) {
      min_prof_info_idx = std::min<uint32_t>(min_prof_info_idx, t->userdata[src.identifier()] + 1);
    }
  }

  workIdTuplesSection();  // write id_tuples, set id_tuples_sec_size for hdr

  // Rank 0 writes out the file header, now that everything has been laid out
  if (mpi::World::rank() == 0) {
    auto buf = composeProfHdr(
        nProf, prof_info_sec_ptr, prof_info_sec_size, id_tuples_sec_ptr, id_tuples_sec_size);
    pmf->open(true, false).writeat(0, buf.size(), buf.data());
  }

  // prep for profiles writing
  prof_infos.resize(myNProf);  // prepare for prof infos

  obuffers = std::vector<OutBuffer>(2);  // prepare for prof data
  obuffers[0].cur_pos = 0;
  obuffers[1].cur_pos = 0;
  cur_obuf_idx = 0;

  accFpos.initialize(
      id_tuples_sec_ptr
      + MULTIPLE_8(id_tuples_sec_size));  // start the window to keep track of the real file cursor
}

void SparseDB::notifyThreadFinal(const PerThreadTemporary& tt) {
  const auto& t = tt.thread();
  contextWavefront.wait();

  // Allocate the blobs needed for the final output
  std::vector<hpcrun_metricVal_t> values;
  std::vector<uint16_t> mids;
  std::vector<uint32_t> cids;
  std::vector<uint64_t> coffsets;
  coffsets.reserve(contexts.size() + 1);
  uint64_t pre_val_size;

  // Now stitch together each Context's results
  for (const Context& c : contexts) {
    if (auto accums = tt.accumulatorsFor(c)) {
      cids.push_back(c.userdata[src.identifier()]);
      coffsets.push_back(values.size());
      pre_val_size = values.size();
      for (const auto& mx : accums->citerate()) {
        const Metric& m = mx.first;
        const auto& vv = mx.second;
        if (!m.scopes().has(MetricScope::function) || !m.scopes().has(MetricScope::execution))
          util::log::fatal{} << "Metric isn't function/execution!";
        const auto& id = m.userdata[src.identifier()];
        hpcrun_metricVal_t v;
        if (auto vex = vv.get(MetricScope::function)) {
          v.r = *vex;
          mids.push_back(id.getFor(MetricScope::function));
          values.push_back(v);
          // HACK conditional to work around experiment.xml. Line Scopes are
          // emitted as leaves (<S>), so they should have no extra inclusive cost.
          if (c.scope().flat().type() == Scope::Type::line) {
            mids.push_back(id.getFor(MetricScope::execution));
            values.push_back(v);
          }
        }
        // HACK conditional to work around experiment.xml. Line Scopes are
        // emitted as leaves (<S>), so they should have no extra inclusive cost.
        if (c.scope().flat().type() != Scope::Type::line) {
          if (auto vinc = vv.get(MetricScope::execution)) {
            v.r = *vinc;
            mids.push_back(id.getFor(MetricScope::execution));
            values.push_back(v);
          }
        }
      }
      c.userdata[ud].cnt += (values.size() - pre_val_size);
      // HACK conditional to support the above HACKs. Its now possible (although
      // hopefully rare) for a Context to have no metric values.
      if (pre_val_size == values.size()) {
        cids.pop_back();
        coffsets.pop_back();
      }
    }
  }

  // Add the extra ctx id and offset pair, to mark the end of ctx
  cids.push_back(LastNodeEnd);
  coffsets.push_back(values.size());

  // Put together the sparse_metrics structure
  hpcrun_fmt_sparse_metrics_t sm;
  sm.id_tuple.length = 0;  // always 0 here
  sm.num_vals = values.size();
  sm.num_cct_nodes = contexts.size();
  sm.num_nz_cct_nodes = coffsets.size() - 1;  // since there is an extra end node
  sm.values = values.data();
  sm.mids = mids.data();
  sm.cct_node_ids = cids.data();
  sm.cct_node_idxs = coffsets.data();

  // Convert the sparse_metrics structure to binary form
  auto sparse_metrics_bytes = profBytes(&sm);
  assert(
      sparse_metrics_bytes.size()
      == (values.size() * PMS_vm_pair_SIZE + coffsets.size() * PMS_ctx_pair_SIZE));

  // Build prof_info
  pms_profile_info_t pi;
  pi.prof_info_idx = t.userdata[src.identifier()] + 1;
  pi.num_vals = values.size();
  pi.num_nzctxs = coffsets.size() - 1;
  pi.id_tuple_ptr = id_tuple_ptrs[pi.prof_info_idx - min_prof_info_idx];
  pi.metadata_ptr = 0;
  pi.spare_one = 0;
  pi.spare_two = 0;

  pi.offset = writeProf(sparse_metrics_bytes, pi.prof_info_idx);
  prof_infos[pi.prof_info_idx - min_prof_info_idx] = std::move(pi);
}

void SparseDB::write() {
  auto mpiSem = src.enterOrderedWrite();

  // flush out the remaining buffer besides summary
  OutBuffer& ob = obuffers[cur_obuf_idx];
  uint64_t wrt_off = 0;

  if (ob.buf.size() != 0) {
    wrt_off = accFpos.fetch_add(ob.buf.size());
    flushOutBuffer(wrt_off, ob);
  }

  // write prof_infos, no summary yet
  writeProfInfos();

  // gather cct major data
  ctxcnt = mpi::bcast(contexts.size(), 0);
  ctx_nzmids_cnts.resize(ctxcnt, 1);  // one for LastNodeEnd
  ctx_nzval_cnts.resize(ctxcnt, 0);
  for (const Context& c : contexts)
    ctx_nzval_cnts[c.userdata[src.identifier()]] =
        c.userdata[ud].cnt.load(std::memory_order_relaxed);

  if (mpi::World::rank() == 0) {
    // Allocate the blobs needed for the final output
    std::vector<hpcrun_metricVal_t> values;
    std::vector<uint16_t> mids;
    std::vector<uint32_t> cids;
    std::vector<uint64_t> coffsets;
    coffsets.reserve(contexts.size() + 1);

    // Now stitch together each Context's results
    for (const Context& c : contexts) {
      const auto& stats = c.data().statistics();
      if (stats.size() > 0) {
        cids.push_back(c.userdata[src.identifier()]);
        coffsets.push_back(values.size());
      }
      for (const auto& mx : stats.citerate()) {
        bool hasEx = false;
        bool hasInc = false;
        const Metric& m = mx.first;
        if (!m.scopes().has(MetricScope::function) || !m.scopes().has(MetricScope::execution))
          util::log::fatal{} << "Metric isn't function/execution!";
        const auto& id = m.userdata[src.identifier()];
        const auto& vv = mx.second;
        for (const auto& sp : m.partials()) {
          hpcrun_metricVal_t v;
          if (auto vex = vv.get(sp).get(MetricScope::function)) {
            v.r = *vex;
            mids.push_back(id.getFor(sp, MetricScope::function));
            values.push_back(v);
            hasEx = true;
            // HACK conditional to work around experiment.xml. Line Scopes are
            // emitted as leaves (<S>), so they should have no extra inclusive cost.
            if (c.scope().flat().type() == Scope::Type::line) {
              mids.push_back(id.getFor(sp, MetricScope::execution));
              values.push_back(v);
              hasInc = true;
            }
          }
          // HACK conditional to work around experiment.xml. Line Scopes are
          // emitted as leaves (<S>), so they should have no extra inclusive cost.
          if (c.scope().flat().type() != Scope::Type::line) {
            if (auto vinc = vv.get(sp).get(MetricScope::execution)) {
              v.r = *vinc;
              mids.push_back(id.getFor(sp, MetricScope::execution));
              values.push_back(v);
              hasInc = true;
            }
          }
        }
        if (hasEx)
          ctx_nzmids_cnts[c.userdata[src.identifier()]]++;
        if (hasInc)
          ctx_nzmids_cnts[c.userdata[src.identifier()]]++;
      }
    }

    // prepare for cct.db, initiate some worker threads
    cctdbSetUp();

    // SUMMARY
    // Add the extra ctx id and offset pair, to mark the end of ctx
    cids.push_back(LastNodeEnd);
    coffsets.push_back(values.size());

    // Put together the sparse_metrics structure
    hpcrun_fmt_sparse_metrics_t sm;
    // sm.tid = 0;
    sm.id_tuple.length = 0;
    sm.num_vals = values.size();
    sm.num_cct_nodes = contexts.size();
    sm.num_nz_cct_nodes = coffsets.size() - 1;  // since there is an extra end node
    sm.values = values.data();
    sm.mids = mids.data();
    sm.cct_node_ids = cids.data();
    sm.cct_node_idxs = coffsets.data();

    // Build prof_info
    pms_profile_info_t pi;
    pi.prof_info_idx = 0;
    pi.num_vals = values.size();
    pi.num_nzctxs = coffsets.size() - 1;
    pi.id_tuple_ptr = id_tuple_ptrs[0];
    pi.metadata_ptr = 0;
    pi.spare_one = 0;
    pi.spare_two = 0;

    auto sparse_metrics_bytes = profBytes(&sm);
    assert(
        sparse_metrics_bytes.size()
        == (values.size() * PMS_vm_pair_SIZE + coffsets.size() * PMS_ctx_pair_SIZE));

    // write summary data into the current buffer
    wrt_off = accFpos.fetch_add(sparse_metrics_bytes.size());
    auto pmfi = pmf->open(true, true);
    pmfi.writeat(wrt_off, sparse_metrics_bytes.size(), sparse_metrics_bytes.data());
    pi.offset = wrt_off;

    // write prof_info for summary
    handleItemPi(pi);

    // footer to show completeness
    // auto pmfi = pmf->open(true, false);
    auto footer_val = PROFDBft;
    uint64_t footer_off = accFpos.fetch_add(sizeof(footer_val));
    pmfi.writeat(footer_off, sizeof(footer_val), &footer_val);
  } else {
    // prepare for cct.db, initiate some worker threads
    cctdbSetUp();
  }

  // do the actual main reads and writes of cct.db
  writeCCTDB();

  // Dump the FORMATS.md file
  try {
    std::ofstream(dir / "FORMATS.md") << FORMATS_md;
  } catch (std::exception& e) { util::log::warning{} << "Error while writing out FORMATS.md file"; }
}

//***************************************************************************
// profile.db  - YUMENG
//
/// EXAMPLE
/// HPCPROF-tmsdb_____
///[hdr:
///   (version: 1.0)
///   (num_prof: 73)
///   (num_sec: 2)
///   (prof_info_sec_size: 3796)
///   (prof_info_sec_ptr: 128)
///   (id_tuples_sec_size: 1596)
///   (id_tuples_sec_ptr: 3928)
///]
///[Profile informations for 72 profiles
///   0[(id_tuple_ptr: 3928) (metadata_ptr: 0) (spare_one: 0) (spare_two: 0) (num_vals: 182)
///   (num_nzctxs: 120) (starting location: 70594))] 1[(id_tuple_ptr: 3940) (metadata_ptr: 0)
///   (spare_one: 0) (spare_two: 0) (num_vals: 52) (num_nzctxs: 32) (starting location: 59170)]
///   ...
///]
///[Id tuples for 121 profiles
///   0[(SUMMARY: 0) ]
///   1[(NODE: 713053824) (THREAD: 5) ]
///   2[(NODE: 713053824) (THREAD: 53) ]
///   ...
///]
///[thread 39
///   [metrics:
///   (NOTES: printed in file order, help checking if the file is correct)
///     (value: 2.8167, metric id: 1)
///     (value: 2.8167, metric id: 1)
///     ...
///   ]
///   [ctx indices:
///     (ctx id: 1, index: 0)
///     (ctx id: 7, index: 1)
///     ...
///   ]
///]
///...
/// PROFILEDB FOOTER CORRECT, FILE COMPLETE
//***************************************************************************

//
// id tuples
//
void SparseDB::workIdTuplesSection() {
  int local_num_prof = src.threads().size();
  if (mpi::World::rank() == 0)
    local_num_prof++;

  std::vector<id_tuple_t> id_tuples(local_num_prof);
  id_tuple_ptrs.resize(local_num_prof);
  uint64_t local_tuples_size = 0;

  // fill the id_tuples and id_tuple_ptrs
  for (const auto& t : src.threads().iterate()) {
    // get the idx in the id_tuples and id_tuple_ptrs
    uint32_t idx = t->userdata[src.identifier()] + 1 - min_prof_info_idx;

    // build the id_tuple
    id_tuple_t idt;
    idt.length = t->attributes.idTuple().size();
    idt.ids = (pms_id_t*)malloc(idt.length * sizeof(pms_id_t));
    for (uint i = 0; i < idt.length; i++)
      idt.ids[i] = t->attributes.idTuple()[i];

    id_tuples[idx] = std::move(idt);
    id_tuple_ptrs[idx] = PMS_id_tuple_len_SIZE + idt.length * PMS_id_SIZE;
    local_tuples_size += id_tuple_ptrs[idx];
  }

  // don't forget the summary id_tuple
  if (mpi::World::rank() == 0) {
    id_tuple_t idt;
    idt.length = IDTUPLE_SUMMARY_LENGTH;
    idt.ids = (pms_id_t*)malloc(idt.length * sizeof(pms_id_t));
    idt.ids[0].kind = IDTUPLE_SUMMARY;
    idt.ids[0].physical_index = IDTUPLE_SUMMARY_IDX;
    idt.ids[0].logical_index = IDTUPLE_SUMMARY_IDX;

    id_tuples[0] = std::move(idt);
    id_tuple_ptrs[0] = PMS_id_tuple_len_SIZE + idt.length * PMS_id_SIZE;
    local_tuples_size += id_tuple_ptrs[0];
  }

  // find where to write as a rank
  uint64_t my_offset = 0;
  my_offset = mpi::exscan(local_tuples_size, mpi::Op::sum()).value_or(0);

  // write out id_tuples
  {
    auto buf = composeIdTuples(id_tuples);
    auto fhi = pmf->open(true, false);
    fhi.writeat(id_tuples_sec_ptr + my_offset, buf.size(), buf.data());
  }

  // set class variable, will be output in hdr
  id_tuples_sec_size = mpi::allreduce(local_tuples_size, mpi::Op::sum());

  // id_tuple_ptrs now store the number of bytes for each idtuple, exscan to get ptr
  std::exclusive_scan(id_tuple_ptrs.begin(), id_tuple_ptrs.end(), id_tuple_ptrs.begin(), 0);
  for (auto& ptr : id_tuple_ptrs) {
    ptr += (my_offset + id_tuples_sec_ptr);
  }

  // free all the tuples
  for (auto tuple : id_tuples) {
    free(tuple.ids);
    tuple.ids = NULL;
  }
}

void SparseDB::handleItemPi(pms_profile_info_t& pi) {
  std::vector<char> info_bytes;

  auto b = convertToByte8(pi.id_tuple_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.metadata_ptr);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.spare_one);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.spare_two);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.num_vals);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte4(pi.num_nzctxs);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  b = convertToByte8(pi.offset);
  info_bytes.insert(info_bytes.end(), b.begin(), b.end());

  auto fhi = pmf->open(true, false);
  fhi.writeat(
      prof_info_sec_ptr + pi.prof_info_idx * PMS_prof_info_SIZE, PMS_prof_info_SIZE,
      info_bytes.data());
}

void SparseDB::writeProfInfos() {
  parForPi.fill(std::move(prof_infos), [&](pms_profile_info_t& item) { handleItemPi(item); });
  parForPi.contribute(parForPi.wait());
}

//
// write profiles
//
std::vector<char> SparseDB::profBytes(hpcrun_fmt_sparse_metrics_t* sm) {
  std::vector<char> out;
  std::vector<char> b;

  for (uint i = 0; i < sm->num_vals; ++i) {
    b = convertToByte8(sm->values[i].bits);
    out.insert(out.end(), b.begin(), b.end());
    b = convertToByte2(sm->mids[i]);
    out.insert(out.end(), b.begin(), b.end());
  }

  for (uint i = 0; i < sm->num_nz_cct_nodes + 1; ++i) {
    b = convertToByte4(sm->cct_node_ids[i]);
    out.insert(out.end(), b.begin(), b.end());
    b = convertToByte8(sm->cct_node_idxs[i]);
    out.insert(out.end(), b.begin(), b.end());
  }

  return out;
}

void SparseDB::flushOutBuffer(uint64_t wrt_off, OutBuffer& ob) {
  auto pmfi = pmf->open(true, true);
  pmfi.writeat(wrt_off, ob.buf.size(), ob.buf.data());
  for (auto pi : ob.buffered_pidxs)
    prof_infos[pi - min_prof_info_idx].offset += wrt_off;
  ob.cur_pos = 0;
  ob.buf.clear();
  ob.buffered_pidxs.clear();
}

uint64_t SparseDB::writeProf(const std::vector<char>& prof_bytes, uint32_t prof_info_idx) {
  uint64_t wrt_off = 0;

  std::unique_lock<std::mutex> olck(outputs_l);
  OutBuffer& ob = obuffers[cur_obuf_idx];

  std::unique_lock<std::mutex> lck(ob.mtx);

  bool flush = false;
  if ((prof_bytes.size() + ob.cur_pos) >= (64 * 1024 * 1024)) {
    cur_obuf_idx = 1 - cur_obuf_idx;
    flush = true;

    size_t my_size = ob.buf.size() + prof_bytes.size();
    wrt_off = accFpos.fetch_add(my_size);
  }
  olck.unlock();

  // add bytes to the current buffer
  ob.buf.insert(ob.buf.end(), prof_bytes.begin(), prof_bytes.end());

  // record the prof_info_idx of the profile being added to the buffer
  ob.buffered_pidxs.emplace_back(prof_info_idx);

  // update current position
  uint64_t rel_off = ob.cur_pos;
  ob.cur_pos += prof_bytes.size();

  if (flush)
    flushOutBuffer(wrt_off, ob);

  return rel_off + wrt_off;
}

//***************************************************************************
// cct.db
//
/// EXAMPLE
/// HPCPROF-cmsdb___
///[hdr:
///   (version: 1.0)
///   (num_ctx: 137)
///   (num_sec: 1)
///  (ctx_info_sec_size: 3014)
///   (ctx_info_sec_ptr: 128)
///]
///[Context informations for 220 Contexts
///   [(context id: 1) (num_vals: 72) (num_nzmids: 1) (starting location: 4844)]
///   [(context id: 3) (num_vals: 0) (num_nzmids: 0) (starting location: 5728)]
///   ...
///]
///[context 1
///   [metrics easy grep version:
///   (NOTES: printed in file order, help checking if the file is correct)
///     (value: 2.64331, thread id: 0)
///     (value: 2.62104, thread id: 1)
///     ...
///   ]
///   [metric indices:
///     (metric id: 1, index: 0)
///     (metric id: END, index: 72)
///   ]
///]
///...same [sparse metrics] for all rest ctxs
/// CCTDB FOOTER CORRECT, FILE COMPLETE
//***************************************************************************
static std::vector<std::pair<uint32_t, uint64_t>>
readProfileCtxPairs(const util::File& pmf, const pms_profile_info_t& pi) {
  if (pi.num_nzctxs == 0) {
    // No data in this profile, skip
    return {};
  }

  // Read the whole chunk of ctx_id/idx pairs
  auto pmfi = pmf.open(false, false);
  std::vector<char> buf((pi.num_nzctxs + 1) * PMS_ctx_pair_SIZE);
  pmfi.readat(pi.offset + pi.num_vals * PMS_vm_pair_SIZE, buf.size(), buf.data());

  // Parse and save in the output
  std::vector<std::pair<uint32_t, uint64_t>> prof_ctx_pairs;
  prof_ctx_pairs.reserve(pi.num_nzctxs + 1);
  for (uint32_t i = 0; i < pi.num_nzctxs + 1; i++) {
    const char* cur = &buf[i * PMS_ctx_pair_SIZE];
    prof_ctx_pairs.emplace_back(interpretByte4(cur), interpretByte8(cur + PMS_ctx_id_SIZE));
  }
  return prof_ctx_pairs;
}

void SparseDB::cctdbSetUp() {
  // Lay out the cct.db metric data section, last is total size.
  ctxOffsets = std::vector<uint64_t>(ctxcnt + 1, 0);
  for (size_t i = 0; i < ctxcnt; i++) {
    ctxOffsets[i] = ctx_nzval_cnts[i] * CMS_val_prof_idx_pair_SIZE;
    // ????
    if (mpi::World::rank() == 0 && ctx_nzmids_cnts[i] > 1)
      ctxOffsets[i] += ctx_nzmids_cnts[i] * CMS_m_pair_SIZE;
  }
  std::exclusive_scan(
      ctxOffsets.begin(), ctxOffsets.end(), ctxOffsets.begin(),
      mpi::World::rank() == 0 ? (MULTIPLE_8(ctxcnt * CMS_ctx_info_SIZE)) + CMS_hdr_SIZE : 0);
  ctxOffsets = mpi::allreduce(ctxOffsets, mpi::Op::sum());

  // Divide the contexts into groups of easily distributable sizes
  ctxGroups.clear();
  {
    ctxGroups.push_back(0);
    const uint64_t limit =
        std::min<uint64_t>(1024ULL * 1024 * 1024 * 3, ctxOffsets.back() / (3 * mpi::World::size()));
    uint64_t cursize = 0;
    for (size_t i = 0; i < ctxcnt; i++) {
      const uint64_t size = ctxOffsets[i + 1] - ctxOffsets[i];
      if (cursize + size > limit) {
        ctxGroups.push_back(i);
        cursize = 0;
      }
      cursize += size;
    }
    ctxGroups.push_back(ctxcnt);
  }

  // Initialize the ctx group counter, for dynamic distribution
  // All ranks other than rank 0 get a pre-allocated ctx group
  groupCounter.initialize(mpi::World::size() - 1);

  // Pop open the cct.db file, and synchronize.
  cmf = util::File(dir / "cct.db", true);
  cmf->synchronize();

  // Read and parse the Profile Info section of the final profile.db
  {
    auto fi = pmf->open(false, false);
    uint32_t nProf =
        readAsByte4(fi, HPCPROFILESPARSE_FMT_MagicLen + HPCPROFILESPARSE_FMT_VersionLen);

    // Read the whole section in, skipping over the index 0 summary profile
    std::vector<char> buf((nProf - 1) * PMS_prof_info_SIZE);
    fi.readat(PMS_hdr_SIZE + PMS_prof_info_SIZE, buf.size(), buf.data());

    // Load the data we need from the profile.db, in parallel
    profiles = std::deque<ProfileData>(nProf - 1);
    forProfilesParse.fill(profiles.size(), [this, &buf](size_t i) {
      auto pi = parseProfInfo(&buf[i * PMS_prof_info_SIZE]);
      profiles[i] = {
          .offset = pi.offset,
          .index = (uint32_t)(i + 1),
          .ctxPairs = readProfileCtxPairs(*pmf, pi),
      };
    });
    forProfilesParse.contribute(forProfilesParse.wait());
  }
}

namespace {
struct LoadedProfile {
  // First loaded ctx_id/idx pair
  std::vector<std::pair<uint32_t, uint64_t>>::const_iterator first;
  // Last (one-after-end) loaded ctx_id/idx pair
  std::vector<std::pair<uint32_t, uint64_t>>::const_iterator last;
  // Absolute index of the profile
  uint32_t index;
  // Loaded metric/value blob for the profile, in the [first, last) ctx range
  std::vector<char> mvBlob;

  LoadedProfile() = default;

  // Load a profile's data from the given File and data
  LoadedProfile(
      uint32_t firstCtx, uint32_t lastCtx, const util::File& pmf, uint64_t offset, uint32_t index,
      const std::vector<std::pair<uint32_t, uint64_t>>& ctxPairs);
};
}  // namespace

LoadedProfile::LoadedProfile(
    uint32_t firstCtx, uint32_t lastCtx, const util::File& pmf, const uint64_t offset,
    const uint32_t index, const std::vector<std::pair<uint32_t, uint64_t>>& ctxPairs)
    : first(ctxPairs.begin()), last(ctxPairs.begin()), index(index) {
  if (ctxPairs.size() <= 1 || firstCtx >= lastCtx) {
    // Empty range, we don't have any data to add.
    return;
  }

  // Compare a range of ctx_ids [first, second) to a ctx_id/idx pair.
  // ctx_ids within [first, second) compare "equal".
  using cipair = std::pair<uint32_t, uint64_t>;
  using range = std::pair<uint32_t, uint32_t>;
  static_assert(!std::is_same_v<cipair, range>, "ADL will fail here!");
  struct compare {
    bool operator()(const cipair& a, const range& b) { return a.first < b.first; }
    bool operator()(const range& a, const cipair& b) { return a.second <= b.first; }
  };

  // Binary search for the [first, last) range among this profile's pairs.
  // Skip the last pair, which is always LastNodeEnd.
  auto ctxRange =
      std::equal_range(ctxPairs.begin(), --ctxPairs.end(), range(firstCtx, lastCtx), compare{});
  first = ctxRange.first;
  last = ctxRange.second;

  // Read the blob of data containing all our pairs
  mvBlob.resize((last->second - first->second) * PMS_vm_pair_SIZE);
  assert(!mvBlob.empty());

  auto pmfi = pmf.open(false, false);
  pmfi.readat(offset + first->second * PMS_vm_pair_SIZE, mvBlob.size(), mvBlob.data());
}

static void writeContexts(
    uint32_t firstCtx, uint32_t lastCtx, const util::File& cmf,
    const std::deque<LoadedProfile>& loadedProfiles, const std::vector<uint64_t>& ctxOffsets) {
  assert(firstCtx < lastCtx && "Empty ctxRange?");

  // Set up a heap with cursors into each profile's data blob
  std::vector<std::pair<
      std::vector<std::pair<uint32_t, uint64_t>>::const_iterator,  // ctx_id/idx pair in a profile
      std::reference_wrapper<const LoadedProfile>                  // Profile to get data from
      >>
      heap;
  const auto heap_comp = [](const auto& a, const auto& b) {
    // The "largest" heap entry has the smallest ctx id or profile index
    return a.first->first != b.first->first ? a.first->first > b.first->first
                                            : a.second.get().index > b.second.get().index;
  };
  heap.reserve(loadedProfiles.size());
  for (const auto& lp : loadedProfiles) {
    if (lp.first == lp.last)
      continue;  // Profile is empty, skip

    auto start = std::lower_bound(
        lp.first, lp.last, std::make_pair(firstCtx, 0),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    if (start->first >= lastCtx)
      continue;  // Profile has no data for us, skip

    heap.push_back({start, lp});
  }
  heap.shrink_to_fit();
  std::make_heap(heap.begin(), heap.end(), heap_comp);
  if (heap.empty())
    return;  // No data for us!

  // Start copying data over, one context at a time. The heap efficiently sorts
  // our search so we can jump straight to the next context we want.
  const auto firstCtxId = heap.front().first->first;
  std::vector<char> buf;
  while (!heap.empty() && heap.front().first->first < lastCtx) {
    const uint32_t ctx_id = heap.front().first->first;
    std::map<uint16_t, std::vector<char>> valuebufs;
    uint64_t allpvs = 0;

    // Pull the data out for one context and save it to cmb
    while (heap.front().first->first == ctx_id) {
      // Pop the top entry from the heap, and increment it
      std::pop_heap(heap.begin(), heap.end(), heap_comp);
      const auto& curPair = *(heap.back().first++);

      // Fill cmb with metric/value pairs for this context, from the top profile
      const LoadedProfile& lp = heap.back().second;
      const char* cur = &lp.mvBlob[(curPair.second - lp.first->second) * PMS_vm_pair_SIZE];
      for (uint64_t i = 0, e = heap.back().first->second - curPair.second; i < e;
           i++, cur += PMS_vm_pair_SIZE) {
        allpvs++;
        const uint16_t mid = interpretByte2(cur + PMS_val_SIZE);
        auto& subbuf = valuebufs.try_emplace(mid).first->second;

        // Write in this prof_idx/value pair, in bytes.
        // Values are represented the same so they can just be copied.
        subbuf.reserve(subbuf.size() + CMS_val_prof_idx_pair_SIZE);
        subbuf.insert(subbuf.end(), cur, cur + PMS_val_SIZE);
        insertByte4(&*subbuf.insert(subbuf.end(), CMS_prof_idx_SIZE, 0), lp.index);
      }

      // If the updated entry is still in range, push it back into the heap.
      // Otherwise pop the entry from the vector completely.
      if (heap.back().first->first < lastCtx)
        std::push_heap(heap.begin(), heap.end(), heap_comp);
      else
        heap.pop_back();
    }

    // Allocate enough space in buf for all the bits we want.
    const auto newsz =
        allpvs * CMS_val_prof_idx_pair_SIZE + (valuebufs.size() + 1) * CMS_m_pair_SIZE;
    assert(
        ctxOffsets[ctx_id] + newsz == ctxOffsets[ctx_id + 1]
        && "Final layout doesn't match precalculated ctx_off!");
    buf.reserve(buf.size() + newsz);

    // Concatinate the prof_idx/value pairs, in bytes form, in metric order
    for (const auto& [mid, pvbuf] : valuebufs)
      buf.insert(buf.end(), pvbuf.begin(), pvbuf.end());

    // Construct the metric_id/idx pairs for this context, in bytes
    {
      char* cur = &*buf.insert(buf.end(), (valuebufs.size() + 1) * CMS_m_pair_SIZE, 0);
      uint64_t pvs = 0;
      for (const auto& [mid, pvbuf] : valuebufs) {
        cur = insertByte2(cur, mid);
        cur = insertByte8(cur, pvs);
        pvs += pvbuf.size() / CMS_val_prof_idx_pair_SIZE;
      }
      assert(pvs == allpvs);

      // Last entry is always LastMidEnd
      cur = insertByte2(cur, LastMidEnd);
      cur = insertByte8(cur, pvs);
    }
  }

  // Write out the whole blob of data where it belongs in the file
  if (buf.empty())
    return;
  assert(firstCtxId != LastNodeEnd);
  auto cmfi = cmf.open(true, true);
  cmfi.writeat(ctxOffsets[firstCtxId], buf.size(), buf.data());
}

void SparseDB::writeCCTDB() {
  if (mpi::World::rank() == 0) {
    // Rank 0 is in charge of writing the header and context info sections
    auto cmfi = cmf->open(true, true);

    auto hdr = composeCtxHdr(ctxcnt);
    cmfi.writeat(0, hdr.size(), hdr.data());

    std::vector<char> buf(ctxcnt * CMS_ctx_info_SIZE);
    char* cur = buf.data();
    for (uint32_t i = 0; i < ctxcnt; i++) {
      uint16_t nMetrics = ctx_nzmids_cnts[i] - 1;
      uint64_t nVals = nMetrics == 0
                         ? 0
                         : (ctxOffsets[i + 1] - ctxOffsets[i] - (nMetrics + 1) * CMS_m_pair_SIZE)
                               / CMS_val_prof_idx_pair_SIZE;
      assert(nVals == ctx_nzval_cnts[i]);
      cur = insertCtxInfo(
          cur, {
                   .ctx_id = i,
                   .num_vals = nVals,
                   .num_nzmids = nMetrics,
                   .offset = ctxOffsets[i],
               });
    }
    cmfi.writeat(CMS_hdr_SIZE, buf.size(), buf.data());
  }

  // Transpose and copy context data until we're done
  uint32_t idx = mpi::World::rank() > 0 ? mpi::World::rank() - 1  // Pre-allocation
                                        : groupCounter.fetch_add(1);
  while (idx < ctxGroups.size() - 1) {
    // Process the next range of contexts allocated to us
    auto firstCtx = ctxGroups[idx];
    auto lastCtx = ctxGroups[idx + 1];
    if (firstCtx < lastCtx) {
      // Read the blob of data we need from each profile, in parallel
      std::deque<LoadedProfile> loadedProfiles(profiles.size());
      forProfilesLoad.fill(
          loadedProfiles.size(), [this, &loadedProfiles, firstCtx, lastCtx](size_t i) {
            const auto& p = profiles[i];
            loadedProfiles[i] =
                LoadedProfile(firstCtx, lastCtx, *pmf, p.offset, p.index, p.ctxPairs);
          });
      forProfilesLoad.reset();  // Also waits for work to complete

      // Divide up this ctx group into ranges suitable for distributing to threads.
      std::vector<std::pair<uint32_t, uint32_t>> ctxRanges;
      {
        ctxRanges.reserve(src.teamSize());
        const size_t target = (ctxOffsets[lastCtx] - ctxOffsets[firstCtx]) / src.teamSize();
        size_t cursize = 0;
        for (uint32_t id = firstCtx; id < lastCtx; id++) {
          // Stop allocating once we have nearly enough ranges
          if (ctxRanges.size() + 1 == src.teamSize())
            break;

          cursize += ctxOffsets[id + 1] - ctxOffsets[id];
          if (cursize > target) {
            ctxRanges.push_back({!ctxRanges.empty() ? ctxRanges.back().second : firstCtx, id + 1});
            cursize = 0;
          }
        }
        if (ctxRanges.empty() || ctxRanges.back().second < lastCtx) {
          // Last range takes whatever remains
          ctxRanges.push_back({!ctxRanges.empty() ? ctxRanges.back().second : firstCtx, lastCtx});
        }
      }

      // Handle the individual ctx copies
      forEachContextRange.fill(std::move(ctxRanges), [this, &loadedProfiles](const auto& range) {
        writeContexts(range.first, range.second, *cmf, loadedProfiles, ctxOffsets);
      });
      forEachContextRange.reset();
    }

    // Fetch the next available workitem from the group
    idx = groupCounter.fetch_add(1);
  }

  // Notify our helper threads that the workshares are complete now
  forProfilesLoad.fill({});
  forProfilesLoad.complete();
  forEachContextRange.fill({});
  forEachContextRange.complete();

  // The last rank is in charge of writing the final footer, AFTER all other
  // writes have completed. If the footer isn't there, the file isn't complete.
  mpi::barrier();
  if (mpi::World::rank() + 1 == mpi::World::size()) {
    const uint64_t footer = CCTDBftr;
    auto cmfi = cmf->open(true, false);
    cmfi.writeat(ctxOffsets.back(), sizeof footer, &footer);
  }
}
