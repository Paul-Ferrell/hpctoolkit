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
// Copyright ((c)) 2022, Rice University
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

#include "metadb.hpp"

#include "../util/log.hpp"

#include "lib/prof-lean/formats/metadb.h"

#include <fstream>
#include <stack>

using namespace hpctoolkit;
using namespace sinks;

MetaDB::MetaDB(stdshim::filesystem::path dir, bool copySources)
  : dir(std::move(dir)), copySources(copySources) {};

void MetaDB::notifyPipeline() noexcept {
  // TODO
}

template<class I>
static I align(I v, uint8_t a) {
  return (v + a - 1) / a * a;
}

std::string MetaDB::accumulateFormulaString(const Expression& e) {
  std::ostringstream ss;
  std::stack<bool, std::vector<bool>> first;
  first.push(true);
  std::stack<std::string, std::vector<std::string>> infix;
  infix.push("!!!");
  e.citerate_all(
    [&](double v){
      if(!first.top()) ss << infix.top();
      first.top() = false;
      ss << v;
    },
    [&](Expression::uservalue_t){
      if(!first.top()) ss << infix.top();
      first.top() = false;
      ss << "$$";
    },
    [&](const Expression& e) {
      if(!first.top()) ss << infix.top();
      first.top() = false;
      std::string fix = "!!!";
      switch(e.kind()) {
      case Expression::Kind::constant:
      case Expression::Kind::subexpression:
      case Expression::Kind::variable:
        std::abort();
      case Expression::Kind::op_sum:  ss << '('; fix = "+"; break;
      case Expression::Kind::op_sub:  ss << '('; fix = "-"; break;
      case Expression::Kind::op_neg:  ss << "-("; break;
      case Expression::Kind::op_prod: ss << '('; fix = "*"; break;
      case Expression::Kind::op_div:  ss << '('; fix = "/"; break;
      case Expression::Kind::op_pow:  ss << '('; fix = "^"; break;
      case Expression::Kind::op_sqrt: ss << "sqrt("; break;
      case Expression::Kind::op_log:  ss << "log("; fix = ","; break;
      case Expression::Kind::op_ln:   ss << "ln(";break;
      case Expression::Kind::op_min:  ss << "min("; fix = ","; break;
      case Expression::Kind::op_max:  ss << "max("; fix = ","; break;
      case Expression::Kind::op_floor: ss << "floor("; break;
      case Expression::Kind::op_ceil: ss << "ceil("; break;
      }
      first.push(true);
      infix.push(std::move(fix));
    },
    [&](const Expression&) {
      first.pop();
      infix.pop();
      ss << ')';
    }
  );
  return ss.str();
}

void MetaDB::write() try {
  stdshim::filesystem::create_directory(dir);
  std::ofstream f(dir / "meta.db", std::ios_base::out
      | std::ios_base::trunc | std::ios_base::binary);

  fmt_metadb_fHdr_t filehdr = {
    0, 0,
    1, 1,
    2, 2,
    3, 3,
    4, 4,
    5, 5,
    6, 6,
    7, 7
  };

  { // File header
    char buf[FMT_METADB_SZ_FHdr];
    fmt_metadb_fHdr_write(buf, &filehdr);
    f.seekp(0, std::ios_base::beg);
    f.write(buf, sizeof buf);
  }
} catch(const std::exception& e) {
  util::log::fatal{} << "Error while writing meta.db: " << e.what();
}
