/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2021 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file PresolveComponent.cpp
 * @brief The HiGHS class
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */

#include "presolve/PresolveComponent.h"

#include "presolve/HPresolve.h"

HighsStatus PresolveComponent::init(const HighsLp& lp, HighsTimer& timer,
                                    bool mip) {
  data_.postSolveStack.initializeIndexMaps(lp.numRow_, lp.numCol_);
  data_.reduced_lp_ = lp;
  return HighsStatus::OK;
}

HighsStatus PresolveComponent::setOptions(const HighsOptions& options) {
  options_ = &options;

  return HighsStatus::OK;
}

void PresolveComponent::negateReducedLpColDuals(bool reduced) {
  for (HighsInt col = 0; col < data_.reduced_lp_.numCol_; col++)
    data_.recovered_solution_.col_dual[col] =
        -data_.recovered_solution_.col_dual[col];
  return;
}

void PresolveComponent::negateReducedLpCost() { return; }

HighsPresolveStatus PresolveComponent::run(HighsBasis* startBasis) {
  presolve::HPresolve presolve;
  presolve.setInput(data_.reduced_lp_, *options_);

  HighsModelStatus status = presolve.run(data_.postSolveStack, startBasis);

  switch (status) {
    case HighsModelStatus::PRIMAL_INFEASIBLE:
      return HighsPresolveStatus::Infeasible;
    case HighsModelStatus::DUAL_INFEASIBLE:
      return HighsPresolveStatus::Unbounded;
    case HighsModelStatus::OPTIMAL:
      return HighsPresolveStatus::ReducedToEmpty;
    default:
      return HighsPresolveStatus::Reduced;
  }
}

void PresolveComponent::clear() {
  has_run_ = false;
  data_.clear();
}
namespace presolve {

bool checkOptions(const PresolveComponentOptions& options) {
  // todo: check options in a smart way
  if (options.dev) std::cout << "Checking presolve options... ";

  if (!(options.iteration_strategy == "smart" ||
        options.iteration_strategy == "off" ||
        options.iteration_strategy == "num_limit")) {
    if (options.dev)
      std::cout << "error: iteration strategy unknown: "
                << options.iteration_strategy << "." << std::endl;
    return false;
  }

  if (options.iteration_strategy == "num_limit" && options.max_iterations < 0) {
    if (options.dev)
      std::cout << "warning: negative iteration limit: "
                << options.max_iterations
                << ". Presolve will be run with no limit on iterations."
                << std::endl;
    return false;
  }

  return true;
}

}  // namespace presolve
