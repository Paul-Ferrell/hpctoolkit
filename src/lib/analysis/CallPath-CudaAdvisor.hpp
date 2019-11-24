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
// Copyright ((c)) 2002-2018, Rice University
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
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#ifndef Analysis_CallPath_CallPath_CudaAdvisor_hpp 
#define Analysis_CallPath_CallPath_CudaAdvisor_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <map>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <lib/cuda/AnalyzeInstruction.hpp>

#include "CCTGraph.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {

struct Bottleneck {
  Prof::LoadMap::LMId_t lm_id;
  size_t function_id;
  size_t block_id;

  // BLAME latency->pcs
  std::map<std::string, std::vector<std::pair<VMA, double> > > hotspots;
};


class CudaAdvisor {
 public:
  explicit CudaAdvisor(Prof::CallPath::Profile *prof) : _prof(prof) {}

  void init();

  void blame(Prof::LoadMap::LMId_t lm_id, std::vector<CudaParse::InstructionStat *> &inst_stats);

  void advise(Prof::LoadMap::LMId_t lm_id);

  void save(const std::string &file_name);
 
 private:
  typedef std::map<VMA, Prof::CCT::ANode *> VMAProfMap;

  typedef std::map<VMA, Prof::Struct::ANode *> VMAStructMap;

  typedef std::map<VMA, CudaParse::InstructionStat *> VMAInstMap;

 private:
  void constructVMAProfMap(Prof::LoadMap::LMId_t lm_id,
    std::vector<CudaParse::InstructionStat *> &inst_stats,
    std::map<VMA, Prof::CCT::ANode *> &vma_prof_map);

  void constructVMAStructMap(std::vector<CudaParse::InstructionStat *> &inst_stats,
    std::map<VMA, Prof::Struct::ANode *> &vma_struct_map);

  void initStructGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
    CCTGraph<Prof::Struct::ANode *> &struct_dep_graph, VMAStructMap &vma_struct_map);
  
  void initCCTGraph(std::vector<CudaParse::InstructionStat *> &inst_stats,
    CCTGraph<Prof::CCT::ANode *> &cct_dep_graph, VMAProfMap &vma_prof_map);

  void updateCCTGraph(CCTGraph<Prof::CCT::ANode *> &cct_dep_graph,
    CCTGraph<Prof::Struct::ANode *> &struct_dep_graph, VMAProfMap &vma_prof_map,
    VMAStructMap &vma_struct_map);

  void blameCCTGraph(CCTGraph<Prof::CCT::ANode *> &cct_dep_graph, VMAInstMap &vma_inst_map);

 private:
  std::string _inst_metric;
  std::string _issue_metric;

  std::string _invalid_stall_metric;
  std::string _tex_stall_metric;
  std::string _ifetch_stall_metric;
  std::string _pipe_bsy_stall_metric;
  std::string _mem_thr_stall_metric;
  std::string _nosel_stall_metric;
  std::string _other_stall_metric;
  std::string _sleep_stall_metric;

  std::string _exec_dep_stall_metric;
  std::string _mem_dep_stall_metric;
  std::string _cmem_dep_stall_metric;
  std::string _sync_stall_metric;

  std::set<std::string> _inst_stall_metrics;
  std::set<std::string> _dep_stall_metrics;
  Prof::CallPath::Profile *_prof;
  std::vector<std::map<std::string, std::pair<size_t, size_t> > > _metric_name_prof_maps;
};


}  // CallPath

}  // Analysis

#endif  // Analysis_CallPath_CallPath_CudaAdvisor_hpp