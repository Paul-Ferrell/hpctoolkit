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

#define HPCTOOLKIT_PROF2MPI_SPARSE_H
#ifdef HPCTOOLKIT_PROF2MPI_SPARSE_H

#include "lib/prof-lean/hpcrun-fmt.h"
#include "lib/prof-lean/id-tuple.h"
#include "lib/prof/cms-format.h"
#include "lib/prof/pms-format.h"
#include "lib/profile/mpi/all.hpp"
#include "lib/profile/sink.hpp"
#include "lib/profile/stdshim/filesystem.hpp"
#include "lib/profile/util/file.hpp"
#include "lib/profile/util/locked_unordered.hpp"
#include "lib/profile/util/once.hpp"

#include <vector>

namespace hpctoolkit::sinks {

class SparseDB : public hpctoolkit::ProfileSink {
public:
  SparseDB(hpctoolkit::stdshim::filesystem::path);
  ~SparseDB() = default;

  void write() override;

  hpctoolkit::DataClass accepts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::threads | DataClass::contexts | DataClass::metrics;
  }

  hpctoolkit::DataClass wavefronts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::contexts + DataClass::threads;
  }

  hpctoolkit::ExtensionClass requires() const noexcept override {
    using namespace hpctoolkit;
    return ExtensionClass::identifier;
  }

  hpctoolkit::util::WorkshareResult help() override;
  void notifyPipeline() noexcept override;

  void notifyWavefront(hpctoolkit::DataClass) noexcept override;
  void notifyThreadFinal(const hpctoolkit::PerThreadTemporary&) override;

  void cctdbSetUp();
  void writeCCTDB();

private:
//***************************************************************************
// general
//***************************************************************************
#define MULTIPLE_8(v) ((v + 7) & ~7)

  hpctoolkit::stdshim::filesystem::path dir;

  std::size_t ctxcnt;
  std::vector<std::reference_wrapper<const hpctoolkit::Context>> contexts;
  hpctoolkit::util::Once contextWavefront;

//***************************************************************************
// profile.db
//***************************************************************************
#define IDTUPLE_SUMMARY_LENGTH        1
#define IDTUPLE_SUMMARY_PROF_INFO_IDX 0
#define IDTUPLE_SUMMARY_IDX           0  // kind 0 idx 0

  std::optional<hpctoolkit::util::File> pmf;

  // hdr
  uint64_t id_tuples_sec_size;
  uint64_t id_tuples_sec_ptr;
  uint64_t prof_info_sec_size;
  uint64_t prof_info_sec_ptr;

  void writePMSHdr(const uint32_t total_num_prof, const hpctoolkit::util::File& fh);

  // id tuples
  std::vector<char> convertTuple2Bytes(const id_tuple_t& tuple);
  void writeIdTuples(std::vector<id_tuple_t>& id_tuples, uint64_t my_offset);
  void workIdTuplesSection();

  // prof info
  std::vector<uint64_t> id_tuple_ptrs;
  uint32_t min_prof_info_idx;
  std::vector<pms_profile_info_t> prof_infos;
  hpctoolkit::util::ParallelForEach<pms_profile_info_t> parForPi;

  void setMinProfInfoIdx(const int total_num_prof);
  void handleItemPi(pms_profile_info_t& pi);
  void writeProfInfos();

  // help write profiles in notifyWavefront, notifyThreadFinal, write
  hpctoolkit::mpi::SharedAccumulator accFpos;

  struct OutBuffer {
    std::vector<char> buf;
    size_t cur_pos;
    std::vector<uint32_t> buffered_pidxs;
    std::mutex mtx;
  };
  std::vector<OutBuffer> obuffers;  // profiles in binary form waiting to be written
  int cur_obuf_idx;
  std::mutex outputs_l;

  // help collect cct major data
  std::vector<uint64_t> ctx_nzval_cnts;
  std::vector<uint16_t> ctx_nzmids_cnts;

  class udContext {
  public:
    udContext(const hpctoolkit::Context&, SparseDB&) : cnt(0){};
    ~udContext() = default;

    std::atomic<uint64_t> cnt;
  };

  struct {
    hpctoolkit::Context::ud_t::typed_member_t<udContext> context;
    const auto& operator()(hpctoolkit::Context::ud_t&) const noexcept { return context; }
  } ud;

  // write profiles
  std::vector<char> profBytes(hpcrun_fmt_sparse_metrics_t* sm);
  uint64_t filePosFetchOp(uint64_t val);
  void flushOutBuffer(uint64_t wrt_off, OutBuffer& ob);
  uint64_t writeProf(const std::vector<char>& prof_bytes, uint32_t prof_info_idx);

  //***************************************************************************
  // cct.db
  //***************************************************************************
  std::optional<hpctoolkit::util::File> cmf;

  // ctx offsets
  std::vector<uint64_t> ctx_off;

  // helper - gather prof infos
  std::vector<pms_profile_info_t> prof_info_list;

  // helper - gather ctx id idx pairs
  hpctoolkit::util::ParallelFor parForCiip;
  std::vector<std::vector<std::pair<uint32_t, uint64_t>>> all_prof_ctx_pairs;

  hpctoolkit::util::ResettableParallelFor parForPd;

  std::vector<uint32_t> ctx_group_list;  // each number represents the starting ctx id for this
                                         // group
  hpctoolkit::mpi::SharedAccumulator accCtxGrp;
  hpctoolkit::util::ResettableParallelForEach<std::pair<uint32_t, uint32_t>> parForCtxs;

  void rwOneCtxGroup(uint32_t first_ctx, uint32_t last_ctx);
  void rwAllCtxGroup();
};
}  // namespace hpctoolkit::sinks

#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
