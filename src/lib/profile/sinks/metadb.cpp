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

#include "lib/prof-lean/metadb.h"

#include <cstdio>
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

static std::string formula(const Expression& e) {
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

void MetaDB::write() {
  auto metadb = dir / "meta.db";
  FILE* of = fopen(metadb.c_str(), "wbm");
  if(!of) util::log::fatal{} << "Failed to open " << metadb.string() << " for writing!";

  // Skip over the header, we'll write it at the end.
  metadb_hdr_t hdr = {
    HPCMETADB_FMT_VersionMajor, HPCMETADB_FMT_VersionMinor,
    HPCMETADB_FMT_NumSec,
    0, 0,
    1, 1,
    2, 2,
    3, 3,
    4, 4,
    5, 5,
    6, 6,
    7, 7
  };
  fseeko(of, align(HPCMETADB_FMT_HeaderLen, 16), SEEK_SET);

  {  // General Properties section
    hdr.general_sec_ptr = ftello(of);
    metadb_general_t gen;
    if(auto n = src.attributes().name())
      gen.title = const_cast<char*>(n->c_str());
    else gen.title = const_cast<char*>("<untitled>");
    gen.description = const_cast<char*>("TODO database description");
    if(metadb_general_fwrite(&gen, of) != HPCFMT_OK)
      util::log::fatal{} << "Error while writing " << metadb.string() << " general properties!";
    hdr.general_sec_size = ftello(of) - hdr.general_sec_ptr;
  }

  {  // Hierarchical Identifier Table section
    const auto& names_map = src.attributes().idtupleNames();
    std::vector<util::optional_ref<const std::string>> names;
    names.resize(names_map.size(), std::nullopt);
    for(const auto& kv: names_map) {
      names.resize(kv.first+1, std::nullopt);
      names[kv.first] = kv.second;
    }

    fseeko(of, hdr.idtable_sec_ptr = align(ftello(of), 16), SEEK_SET);
    if(hpcfmt_int2_fwrite(names.size(), of) != HPCFMT_OK)
      util::log::fatal{} << "Error while writing " << metadb.string() << " id table!";
    for(const auto& n: names) {
      if(hpcfmt_nullstr_fwrite(n ? n->c_str() : "", of) != HPCFMT_OK)
        util::log::fatal{} << "Error while writing " << metadb.string() << " id table!";
    }
    hdr.idtable_sec_size = ftello(of) - hdr.idtable_sec_ptr;
  }

  {  // Performance Metric section
    fseeko(of, hdr.metric_sec_ptr = align(ftello(of), 16), SEEK_SET);
    if(hpcfmt_int4_fwrite(src.metrics().size(), of) != HPCFMT_OK)
      util::log::fatal{} << "Error while writing " << metadb.string() << " metrics!";
    for(const Metric& m: src.metrics().citerate()) {
      const auto& id = m.userdata[src.identifier()];

      metadb_metric_t met = {
        (uint16_t)m.scopes().count(), const_cast<char*>(m.name().c_str())
      };
      if(metadb_metric_fwrite(&met, of) != HPCFMT_OK)
        util::log::fatal{} << "Error while writing " << metadb.string() << " metrics!";

      for(MetricScope ms: m.scopes()) {
        auto msname = ({
          std::ostringstream ss;
          ss << ms;
          ss.str();
        });

        metadb_metric_scope_t scope = {
          (uint16_t)id.getFor(ms), (uint16_t)m.partials().size(),
          const_cast<char*>(msname.c_str())
        };
        if(metadb_metric_scope_fwrite(&scope, of) != HPCFMT_OK)
          util::log::fatal{} << "Error while writing " << metadb.string() << " metrics!";

        for(const auto& part: m.partials()) {
          auto form = formula(part.accumulate());
          metadb_metric_summary_t sum = {
            (uint16_t)id.getFor(part, ms), 0xFF, const_cast<char*>(form.c_str())
          };
          switch(part.combinator()) {
          case Statistic::combination_t::sum:
            sum.combine = HPCMETADB_FMT_COMBINE_SUM; break;
          case Statistic::combination_t::min:
            sum.combine = HPCMETADB_FMT_COMBINE_MIN; break;
          case Statistic::combination_t::max:
            sum.combine = HPCMETADB_FMT_COMBINE_MAX; break;
          }
          if(metadb_metric_summary_fwrite(&sum, of) != HPCFMT_OK)
            util::log::fatal{} << "Error while writing " << metadb.string() << " metrics!";
        }
      }
    }
    hdr.metric_sec_size = ftello(of) - hdr.metric_sec_ptr;
  }

  // Rewind and write the header, then close up
  rewind(of);
  metadb_hdr_fwrite(&hdr, of);
  fclose(of);
}
