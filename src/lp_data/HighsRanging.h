/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2021 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/*    Authors: Julian Hall, Ivet Galabova, Qi Huangfu, Leona Gottwald    */
/*    and Michael Feldmeier                                              */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file lp_data/HighsRanging.h
 * @brief
 */
#ifndef LP_DATA_HIGHS_RANGING_H_
#define LP_DATA_HIGHS_RANGING_H_

#include <vector>

#include "lp_data/HighsLpSolverObject.h"

struct HighsRangingRecord {
  std::vector<HighsFloat> value_;
  std::vector<HighsFloat> objective_;
  std::vector<HighsInt> in_var_;
  std::vector<HighsInt> ou_var_;
};

struct HighsRanging {
  HighsRangingRecord col_cost_up;
  HighsRangingRecord col_cost_dn;
  HighsRangingRecord col_bound_up;
  HighsRangingRecord col_bound_dn;
  HighsRangingRecord row_bound_up;
  HighsRangingRecord row_bound_dn;
};

HighsStatus getRangingData(HighsRanging& ranging,
                           HighsLpSolverObject& solver_object);
void writeRanging(const HighsRanging& ranging,
                  const HighsLpSolverObject& solver_object);
#endif
