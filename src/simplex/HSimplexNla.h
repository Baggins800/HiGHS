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
/**@file simplex/HSimplexNla.h
 *
 * @brief Interface to HFactor allowing non-HFactor updates, NLA-only
 * scaling and shifting of NLA analysis below simplex level.
 */
#ifndef HSIMPLEXNLA_H_
#define HSIMPLEXNLA_H_

#include "lp_data/HighsDebug.h"
#include "simplex/HFactor.h"
#include "simplex/HighsSimplexAnalysis.h"
#include "simplex/SimplexStruct.h"

struct ProductFormUpdate {
  bool valid_ = false;
  HighsInt num_row_;
  HighsInt update_count_;
  vector<HighsInt> pivot_index_;
  vector<HighsFloat> pivot_value_;
  vector<HighsInt> start_;
  vector<HighsInt> index_;
  vector<HighsFloat> value_;
  void clear();
  void setup(const HighsInt num_row, const HighsFloat expected_density);
  HighsInt update(HVector* aq, HighsInt* iRow);
  void btran(HVector& rhs) const;
  void ftran(HVector& rhs) const;
};

struct FrozenBasis {
  bool valid_ = false;
  HighsInt prev_;
  HighsInt next_;
  ProductFormUpdate update_;
  SimplexBasis basis_;
  void clear();
};

class HSimplexNla {
 private:
  void setup(const HighsLp* lp, HighsInt* base_index,
             const HighsOptions* options, HighsTimer* timer,
             HighsSimplexAnalysis* analysis,
             const HighsSparseMatrix* factor_a_matrix,
             const HighsFloat factor_pivot_threshold);

  void setLpAndScalePointers(const HighsLp* lp);
  void setBasicIndexPointers(HighsInt* basic_index);
  void setPointers(const HighsLp* for_lp,
                   const HighsSparseMatrix* factor_a_matrix = NULL,
                   HighsInt* base_index = NULL,
                   const HighsOptions* options = NULL, HighsTimer* timer = NULL,
                   HighsSimplexAnalysis* analysis = NULL);
  void clear();
  HighsInt invert();
  void btran(HVector& rhs, const HighsFloat expected_density,
             HighsTimerClock* factor_timer_clock_pointer = NULL) const;
  void ftran(HVector& rhs, const HighsFloat expected_density,
             HighsTimerClock* factor_timer_clock_pointer = NULL) const;
  void frozenBtran(HVector& rhs) const;
  void frozenFtran(HVector& rhs) const;
  void update(HVector* aq, HVector* ep, HighsInt* iRow, HighsInt* hint);

  void frozenBasisClearAllData();
  void frozenBasisClearAllUpdate();
  bool frozenBasisAllDataClear();
  bool frozenBasisIdValid(const HighsInt frozen_basis_id) const;
  bool frozenBasisHasInvert(const HighsInt frozen_basis_id) const;
  HighsInt freeze(const SimplexBasis& basis, const HighsFloat col_aq_density);
  void unfreeze(const HighsInt unfreeze_basis_id, SimplexBasis& basis);

  void transformForUpdate(HVector* column, HVector* row_ep,
                          const HighsInt variable_in, const HighsInt row_out);

  void setPivotThreshold(const HighsFloat new_pivot_threshold);

  void passLpPointer(const HighsLp* lp);
  void passScalePointer(const HighsScale* scale);
  void applyBasisMatrixColScale(HVector& rhs) const;
  void applyBasisMatrixRowScale(HVector& rhs) const;
  void addCols(const HighsLp* updated_lp);
  void addRows(const HighsLp* updated_lp, HighsInt* base_index,
               const HighsSparseMatrix* scaled_ar_matrix);
  bool sparseLoopStyle(const HighsInt count, const HighsInt dim,
                       HighsInt& to_entry) const;
  void reportArray(const std::string message, const HVector* vector,
                   const bool force = false) const;
  void reportArraySparse(const std::string message, const HVector* vector,
                         const bool force = false) const;
  void reportPackValue(const std::string message, const HVector* vector,
                       const bool force = false) const;
  // Debug methods
  HighsDebugStatus debugCheckData(const std::string message = "") const;
  HighsDebugStatus debugCheckInvert(const std::string message,
                                    const HighsInt alt_debug_level = -1) const;
  HighsFloat debugInvertResidualError(const bool transposed,
                                  const HVector& solution,
                                  HVector& residual) const;
  HighsDebugStatus debugReportInvertSolutionError(const bool transposed,
                                                  const HVector& true_solution,
                                                  const HVector& solution,
                                                  HVector& residual,
                                                  const bool force) const;
  HighsDebugStatus debugReportInvertSolutionError(
      const std::string source, const bool transposed,
      const HighsFloat solve_error_norm, const HighsFloat residual_error_norm,
      const bool force) const;

  // References:
  //
  // Pointers:

  // Class data members
  const HighsLp* lp_;
  const HighsScale* scale_;
  HighsInt* base_index_;
  const HighsOptions* options_;
  HighsTimer* timer_;
  HighsSimplexAnalysis* analysis_;

  HFactor factor_;

  bool report_;
  HighsFloat build_synthetic_tick_;

  // Frozen basis data
  HighsInt first_frozen_basis_id_ = kNoLink;
  HighsInt last_frozen_basis_id_ = kNoLink;
  vector<FrozenBasis> frozen_basis_;
  ProductFormUpdate update_;

  friend class HEkk;
  friend class HEkkPrimal;
  friend class HEkkDual;
};

#endif /* HSIMPLEXNLA_H_ */
