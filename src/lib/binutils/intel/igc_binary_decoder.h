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

//==============================================================
// Copyright © 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =============================================================

#ifndef PTI_SAMPLES_UTILS_IGC_BINARY_DECODER_H_
#define PTI_SAMPLES_UTILS_IGC_BINARY_DECODER_H_

#include <memory.h>

#ifdef ENABLE_IGC
#include "gen_binary_decoder.h"

#include <igc/ocl_igc_shared/executable_format/patch_list.h>
#include <metrics_discovery_internal_api.h>

using namespace iOpenCL;

class IgcBinaryDecoder {
public:
  IgcBinaryDecoder(const std::vector<uint8_t>& binary) : binary_(binary) {}

  InstructionList Disassemble(const std::string& kernel_name) {
    if (!IsValidHeader()) {
      return InstructionList();
    }

    const SProgramBinaryHeader* header =
        reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    iga_gen_t arch = GetArch(header->Device);
    if (arch == IGA_GEN_INVALID) {
      return InstructionList();
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(header) + sizeof(SProgramBinaryHeader)
                       + header->PatchListSize;
    for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
      const SKernelBinaryHeaderCommon* kernel_header =
          reinterpret_cast<const SKernelBinaryHeaderCommon*>(ptr);

      ptr += sizeof(SKernelBinaryHeaderCommon);
      const char* name = (const char*)ptr;

      ptr += kernel_header->KernelNameSize;
      if (kernel_name == name) {
        std::vector<uint8_t> raw_binary(kernel_header->KernelHeapSize);
        memcpy(raw_binary.data(), ptr, kernel_header->KernelHeapSize * sizeof(uint8_t));
        GenBinaryDecoder decoder(raw_binary, arch);
        return decoder.Disassemble();
      }

      ptr += kernel_header->PatchListSize + kernel_header->KernelHeapSize
           + kernel_header->GeneralStateHeapSize + kernel_header->DynamicStateHeapSize
           + kernel_header->SurfaceStateHeapSize;
    }

    return InstructionList();
  }

private:
  bool IsValidHeader() {
    if (binary_.size() < sizeof(SProgramBinaryHeader)) {
      return false;
    }

    const SProgramBinaryHeader* header =
        reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    if (header->Magic != MAGIC_CL) {
      return false;
    }

    return true;
  }

  static iga_gen_t GetArch(uint32_t device) {
    switch (1 << device) {
    case MetricsDiscovery::PLATFORM_BDW: return IGA_GEN8;
    case MetricsDiscovery::PLATFORM_SKL: return IGA_GEN9;
    case MetricsDiscovery::PLATFORM_KBL: return IGA_GEN9p5;
    case MetricsDiscovery::PLATFORM_ICL: return IGA_GEN11;
    case 1 << 18:  // TGL (?)
      return IGA_GEN12p1;
    default: break;
    }
    return IGA_GEN_INVALID;
  }

private:
  std::vector<uint8_t> binary_;
};
#endif  // ENABLE_IGC

#endif
