/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "GlobalTypeAnalysisPass.h"

#include "DexUtil.h"
#include "GlobalTypeAnalyzer.h"
#include "TypeAnalysisTransform.h"
#include "Walkers.h"

using namespace type_analyzer;

void GlobalTypeAnalysisPass::run_pass(DexStoresVector& stores,
                                      ConfigFiles& config,
                                      PassManager& mgr) {
  if (m_config.insert_runtime_asserts) {
    m_config.runtime_assert =
        RuntimeAssertTransform::Config(config.get_proguard_map());
  }

  type_analyzer::Transform::NullAssertionSet null_assertion_set;
  Transform::setup(null_assertion_set);
  Scope scope = build_class_scope(stores);
  global::GlobalTypeAnalysis analysis(m_config.max_global_analysis_iteration);
  auto gta = analysis.analyze(scope);
  optimize(scope, *gta, null_assertion_set, mgr);
}

void GlobalTypeAnalysisPass::optimize(
    const Scope& scope,
    const type_analyzer::global::GlobalTypeAnalyzer& gta,
    const type_analyzer::Transform::NullAssertionSet& null_assertion_set,
    PassManager& mgr) {
  auto stats = walk::parallel::methods<Stats>(scope, [&](DexMethod* method) {
    if (method->get_code() == nullptr) {
      return Stats();
    }
    auto code = method->get_code();
    auto lta = gta.get_local_analysis(method);

    if (m_config.insert_runtime_asserts) {
      RuntimeAssertTransform rat(m_config.runtime_assert);
      Stats ra_stats;
      ra_stats.assert_stats =
          rat.apply(*lta, gta.get_whole_program_state(), method);
      return ra_stats;
    }

    Transform tf(m_config.transform);
    Stats tr_stats;
    tr_stats.transform_stats = tf.apply(*lta, code, null_assertion_set);
    return tr_stats;
  });
  stats.report(mgr);
}

static GlobalTypeAnalysisPass s_pass;
