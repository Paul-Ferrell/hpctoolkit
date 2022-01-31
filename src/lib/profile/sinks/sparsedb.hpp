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

  // Once that signals when the Contexts/Threads wavefront has passed
  hpctoolkit::util::Once contextWavefront;

  // All the contexts we know about, sorted by identifier.
  std::deque<std::reference_wrapper<const hpctoolkit::Context>> contexts;

  // Total number of Contexts, as seen by Rank 0 (which has all the contexts)
  std::size_t ctxcnt;

//***************************************************************************
// profile.db
//***************************************************************************
#define IDTUPLE_SUMMARY_LENGTH        1
#define IDTUPLE_SUMMARY_PROF_INFO_IDX 0
#define IDTUPLE_SUMMARY_IDX           0  // kind 0 idx 0

  std::optional<hpctoolkit::util::File> pmf;

  static constexpr uint64_t profInfosPtr = PMS_hdr_SIZE;

  // prof info
  hpctoolkit::util::ParallelForEach<std::reference_wrapper<const pms_profile_info_t>>
      forEachProfileInfo;

  // Double-buffered concurrent output for profile data, synchronized across
  // multiple MPI ranks.
  class DoubleBufferedOutput {
  public:
    DoubleBufferedOutput();
    ~DoubleBufferedOutput() = default;

    DoubleBufferedOutput(DoubleBufferedOutput&&) = delete;
    DoubleBufferedOutput(const DoubleBufferedOutput&) = delete;
    DoubleBufferedOutput& operator=(DoubleBufferedOutput&&) = delete;
    DoubleBufferedOutput& operator=(const DoubleBufferedOutput&) = delete;

    // Set the output File and the offset data written this way should start at
    // MT: Externally Synchronized
    void initialize(util::File& outfile, uint64_t startOffset);

    // Allocate some space in the file for the given number of bytes, and return
    // the final offset for this space.
    // MT: Internally Synchronized
    uint64_t allocate(uint64_t size);

    // Write a new blob of profile data into the buffer, obtained by
    // concatinating the two given blobs. The final offset of the blob will be
    // saved to `offset` after flush() has been called.
    // MT: Internally Synchronized
    void write(const std::vector<char>& mvBlob, const std::vector<char>& ciBlob, uint64_t& offset);

    // Flush the buffers and write everything out to the file.
    // MT: Internally Synchronized
    void flush();

  private:
    // Counter for offsets within the file itself
    mpi::SharedAccumulator pos;
    // File everything gets written to in the end
    util::optional_ref<util::File> file;

    // Lock for the top-level internal state
    std::mutex toplock;
    // Index of the current Buffer to write new data to
    int currentBuf = 0;

    struct Buffer {
      static constexpr size_t bufferSize = 64 * 1024 * 1024;  // 64MiB
      Buffer() { blob.reserve(bufferSize); }

      // Lock for per-Buffer state
      std::mutex lowlock;
      // Blob of buffered data to write out on flush
      std::vector<char> blob;
      // Offsets to update once this Buffer is flushed
      std::vector<std::reference_wrapper<uint64_t>> toUpdate;

      // Flush this Buffer's data to the given File.
      // MT: Externally Synchronized (holding lowlock)
      void flush(util::File& file, uint64_t offset);
    };

    // Buffers to rotate between for parallelism
    std::array<Buffer, 2> bufs;
  } profDataOut;

  // help collect cct major data
  std::vector<uint16_t> ctx_nzmids_cnts;

  struct udContext {
    std::atomic<uint64_t> nValues = 0;
  };
  struct udThread {
    pms_profile_info_t info = {
        .metadata_ptr = 0,
        .spare_one = 0,
        .spare_two = 0,
    };
  };
  struct {
    hpctoolkit::Context::ud_t::typed_member_t<udContext> context;
    const auto& operator()(hpctoolkit::Context::ud_t&) const noexcept { return context; }
    hpctoolkit::Thread::ud_t::typed_member_t<udThread> thread;
    const auto& operator()(hpctoolkit::Thread::ud_t&) const noexcept { return thread; }
  } ud;

  // prof_info for the summary profile
  pms_profile_info_t summary_info = {
      .prof_info_idx = 0,
      .metadata_ptr = 0,
      .spare_one = 0,
      .spare_two = 0,
  };

  //***************************************************************************
  // cct.db
  //***************************************************************************
  std::optional<hpctoolkit::util::File> cmf;

  // Byte offset of the start for every Context's data in the resulting cct.db.
  // Indexed by context id. Last element is the total number of bytes in the file.
  std::vector<uint64_t> ctxOffsets;

  // Distribution of contexts into dynamically-allocated groups.
  // Last element is the total number of contexts.
  std::vector<uint32_t> ctxGroups;

  // Shared accumulator to dynamically allocate groups to MPI ranks
  hpctoolkit::mpi::SharedAccumulator groupCounter;

  // Useful data for a profile in the profile.db.
  struct ProfileData {
    // Offset of the data block for this profile in the file
    uint64_t offset;
    // Absolute index of this profile
    uint32_t index;
    // Preparsed ctx_id/idx pairs
    std::vector<std::pair<uint32_t, uint64_t>> ctxPairs;
  };

  // Data for each of the profiles
  std::deque<ProfileData> profiles;

  // Parallel workshares for the various parallel operations
  hpctoolkit::util::ParallelFor forProfilesParse;
  hpctoolkit::util::ResettableParallelFor forProfilesLoad;
  hpctoolkit::util::ResettableParallelForEach<std::pair<uint32_t, uint32_t>> forEachContextRange;
};
}  // namespace hpctoolkit::sinks

#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
