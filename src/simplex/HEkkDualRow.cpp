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
/**@file simplex/HEkkDualRow.cpp
 * @brief
 */
#include "simplex/HEkkDualRow.h"

#include <cassert>
#include <iostream>

#include "pdqsort/pdqsort.h"
#include "simplex/HSimplexDebug.h"
#include "simplex/SimplexTimer.h"
#include "util/HighsSort.h"

using std::make_pair;
using std::pair;
using std::set;

void HEkkDualRow::setupSlice(HighsInt size) {
  workSize = size;
  workMove = &ekk_instance_.basis_.nonbasicMove_[0];
  workDual = &ekk_instance_.info_.workDual_[0];
  workRange = &ekk_instance_.info_.workRange_[0];
  work_devex_index = &ekk_instance_.info_.devex_index_[0];

  // Allocate spaces
  packCount = 0;
  packIndex.resize(workSize);
  packValue.resize(workSize);

  workCount = 0;
  workData.resize(workSize);
  analysis = &ekk_instance_.analysis_;
}

void HEkkDualRow::setup() {
  // Setup common vectors
  const HighsInt numTot =
      ekk_instance_.lp_.num_col_ + ekk_instance_.lp_.num_row_;
  setupSlice(numTot);
  workNumTotPermutation = &ekk_instance_.info_.numTotPermutation_[0];

  // deleteFreelist() is being called in Phase 1 and Phase 2 since
  // it's in updatePivots(), but create_Freelist() is only called in
  // Phase 2. Hence freeList is not initialised when freeList.empty()
  // is used in deleteFreelist(), clear freeList now.
  freeList.clear();
}

void HEkkDualRow::clear() {
  packCount = 0;
  workCount = 0;
}

void HEkkDualRow::chooseMakepack(const HVector* row, const HighsInt offset) {
  /**
   * Pack the indices and values for the row
   *
   * Offset of numCol is used when packing row_ep
   */
  const HighsInt rowCount = row->count;
  const HighsInt* rowIndex = &row->index[0];
  const HighsFloat* rowArray = &row->array[0];

  for (HighsInt i = 0; i < rowCount; i++) {
    const HighsInt index = rowIndex[i];
    const HighsFloat value = rowArray[index];
    packIndex[packCount] = index + offset;
    packValue[packCount++] = value;
  }
}

void HEkkDualRow::choosePossible() {
  /**
   * Determine the possible variables - candidates for CHUZC
   * TODO: Check with Qi what this is doing
   */
  const HighsFloat Ta = ekk_instance_.info_.update_count < 10
                        ? 1e-9
                        : ekk_instance_.info_.update_count < 20 ? 3e-8 : 1e-6;
  const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
  const HighsInt move_out = workDelta < 0 ? -1 : 1;
  workTheta = kHighsInf;
  workCount = 0;
  for (HighsInt i = 0; i < packCount; i++) {
    const HighsInt iCol = packIndex[i];
    const HighsInt move = workMove[iCol];
    const HighsFloat alpha = packValue[i] * move_out * move;
    if (alpha > Ta) {
      workData[workCount++] = make_pair(iCol, alpha);
      const HighsFloat relax = workDual[iCol] * move + Td;
      if (workTheta * alpha > relax) workTheta = relax / alpha;
    }
  }
}

void HEkkDualRow::chooseJoinpack(const HEkkDualRow* otherRow) {
  /**
   * Join pack of possible candidates in this row with possible
   * candidates in otherRow
   */
  const HighsInt otherCount = otherRow->workCount;
  const pair<HighsInt, HighsFloat>* otherData = &otherRow->workData[0];
  copy(otherData, otherData + otherCount, &workData[workCount]);
  workCount = workCount + otherCount;
  workTheta = min(workTheta, otherRow->workTheta);
}

HighsInt HEkkDualRow::chooseFinal() {
  /**
   * Chooses the entering variable via BFRT and EXPAND
   *
   * It will
   * (1) reduce the candidates as a small collection
   * (2) choose by BFRT by going over break points
   * (3) choose final by alpha
   * (4) determine final flip variables
   */

  // 1. Reduce by large step BFRT
  analysis->simplexTimerStart(Chuzc2Clock);
  HighsInt fullCount = workCount;
  workCount = 0;
  HighsFloat totalChange = 0;
  const HighsFloat totalDelta = fabs(workDelta);
  HighsFloat selectTheta = 10 * workTheta + 1e-7;
  for (;;) {
    for (HighsInt i = workCount; i < fullCount; i++) {
      HighsInt iCol = workData[i].first;
      HighsFloat alpha = workData[i].second;
      HighsFloat tight = workMove[iCol] * workDual[iCol];
      if (alpha * selectTheta >= tight) {
        swap(workData[workCount++], workData[i]);
        totalChange += workRange[iCol] * alpha;
      }
    }
    selectTheta *= 10;
    if (totalChange >= totalDelta || workCount == fullCount) break;
  }
  analysis->simplexTimerStop(Chuzc2Clock);
  // 2. Choose by small step BFRT

  bool use_quad_sort = false;
  bool use_heap_sort = false;
  // Use the quadratic cost sort for smaller values of workCount,
  // otherwise use the heap-based sort
  use_quad_sort = true;  // workCount < 100;
  use_heap_sort = !use_quad_sort;
  assert(use_heap_sort || use_quad_sort);
  if (workCount < 100) {
    analysis->num_quad_chuzc++;
  } else {
    analysis->num_heap_chuzc++;
    analysis->sum_heap_chuzc_size += workCount;
    analysis->max_heap_chuzc_size =
        max(workCount, analysis->max_heap_chuzc_size);
  }

  if (use_heap_sort) {
    printf("CHUZC: Using heap sort\n");
    // Take a copy of workData and workCount for the independent
    // heap-based code
    original_workData = workData;
    alt_workCount = workCount;
  }
  analysis->simplexTimerStart(Chuzc3Clock);
  bool choose_ok;
  if (use_quad_sort) {
    // Use the O(n^2) quadratic sort for the candidates
    analysis->simplexTimerStart(Chuzc3a0Clock);
    choose_ok = chooseFinalWorkGroupQuad();
    analysis->simplexTimerStop(Chuzc3a0Clock);
  }
  if (use_heap_sort) {
    // Use the O(n log n) heap sort for the candidates
    analysis->simplexTimerStart(Chuzc3a1Clock);
    choose_ok = chooseFinalWorkGroupHeap();
    analysis->simplexTimerStop(Chuzc3a1Clock);
  }
  if (!choose_ok) {
    analysis->simplexTimerStop(Chuzc3Clock);
    return -1;
  }
  // Make sure that there is at least one group according to sorting procedure
  if (use_quad_sort) assert((HighsInt)workGroup.size() > 1);
  if (use_heap_sort) assert((HighsInt)alt_workGroup.size() > 1);

  // 3. Choose large alpha
  analysis->simplexTimerStart(Chuzc3bClock);
  HighsInt breakIndex;
  HighsInt breakGroup;
  HighsInt alt_breakIndex;
  HighsInt alt_breakGroup;
  if (use_quad_sort)
    chooseFinalLargeAlpha(breakIndex, breakGroup, workCount, workData,
                          workGroup);
  if (use_heap_sort)
    chooseFinalLargeAlpha(alt_breakIndex, alt_breakGroup, alt_workCount,
                          sorted_workData, alt_workGroup);
  analysis->simplexTimerStop(Chuzc3bClock);

  if (!use_quad_sort) {
    // If the quadratic sort is not being used, revert to the heap
    // sort results
    breakIndex = alt_breakIndex;
    breakGroup = alt_breakGroup;
  }
  analysis->simplexTimerStart(Chuzc3cClock);

  const HighsInt move_out = workDelta < 0 ? -1 : 1;
  assert(breakIndex >= 0);
  if (use_quad_sort) {
    workPivot = workData[breakIndex].first;
    workAlpha = workData[breakIndex].second * move_out * workMove[workPivot];
  } else {
    workPivot = sorted_workData[breakIndex].first;
    workAlpha =
        sorted_workData[breakIndex].second * move_out * workMove[workPivot];
  }
  if (workDual[workPivot] * workMove[workPivot] > 0) {
    workTheta = workDual[workPivot] / workAlpha;
  } else {
    workTheta = 0;
  }
  analysis->simplexTimerStop(Chuzc3cClock);

  analysis->simplexTimerStart(Chuzc3dClock);

  // 4. Determine BFRT flip index: flip all
  fullCount = breakIndex;  // Not used
  workCount = 0;
  const bool report = true;  // ekk_instance_.iteration_count_ == check_iter;
  if (use_quad_sort) {
    for (HighsInt i = 0; i < workGroup[breakGroup]; i++) {
      const HighsInt iCol = workData[i].first;
      const HighsInt move = workMove[iCol];
      workData[workCount++] = make_pair(iCol, move * workRange[iCol]);
    }
  } else {
    printf("DebugHeapSortCHUZC: Pivot = %4d; alpha = %11.4g; theta = %11.4g\n",
           (int)workPivot, workAlpha, workTheta);
    debugReportBfrtVar(-1, sorted_workData);
    for (HighsInt i = 0; i < alt_workGroup[breakGroup]; i++) {
      const HighsInt iCol = sorted_workData[i].first;
      const HighsInt move = workMove[iCol];
      debugReportBfrtVar(i, sorted_workData);
      workData[workCount++] = make_pair(iCol, move * workRange[iCol]);
    }
    // Look at all entries of final group to see what dual
    // infeasibilities might be created
    assert(breakGroup + 1 < (int)alt_workGroup.size());
    const HighsInt to_i = alt_workGroup[breakGroup + 1];
    assert(to_i <= (int)sorted_workData.size());
    //    HighsInt num_infeasibility = 0;
    const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
    for (HighsInt i = alt_workGroup[breakGroup]; i < to_i; i++) {
      debugReportBfrtVar(i, sorted_workData);
      const HighsInt iCol = sorted_workData[i].first;
      const HighsFloat value = sorted_workData[i].second;
      const HighsInt move = workMove[iCol];
      const HighsFloat dual = workDual[iCol];
      const HighsFloat new_dual = dual - move_out * move * workTheta * value;
      const HighsFloat new_dual_infeasibility = move * new_dual;
      const bool infeasible = new_dual_infeasibility < -Td;
      if (infeasible) {
        //	num_infeasibility++;
        workData[workCount++] = make_pair(iCol, move * workRange[iCol]);
        assert(workRange[iCol] < kHighsInf);
      }
    }
  }
  if (workTheta == 0) workCount = 0;
  analysis->simplexTimerStop(Chuzc3dClock);

  analysis->simplexTimerStart(Chuzc3eClock);
  // Sort workData by .first (iCol) so that columns of A are accessed in order?
  pdqsort(workData.begin(), workData.begin() + workCount);
  analysis->simplexTimerStop(Chuzc3eClock);
  analysis->simplexTimerStop(Chuzc3Clock);

  HighsInt num_infeasibility = debugChooseColumnInfeasibilities();
  if (num_infeasibility) {
    highsLogDev(ekk_instance_.options_->log_options, HighsLogType::kError,
                "Heap-based chooseFinal would create %d dual infeasibilities\n",
                (int)num_infeasibility);
    analysis->simplexTimerStop(Chuzc3dClock);
    return -1;
  }
  return 0;
}

bool HEkkDualRow::chooseFinalWorkGroupQuad() {
  const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
  HighsInt fullCount = workCount;
  workCount = 0;
  HighsFloat totalChange = kInitialTotalChange;
  HighsFloat selectTheta = workTheta;
  const HighsFloat totalDelta = fabs(workDelta);
  workGroup.clear();
  workGroup.push_back(0);
  HighsInt prev_workCount = workCount;
  HighsFloat prev_remainTheta = kInitialRemainTheta;
  HighsFloat prev_selectTheta = selectTheta;
  HighsInt debug_num_loop = 0;

  while (selectTheta < kMaxSelectTheta) {
    HighsFloat remainTheta = kInitialRemainTheta;
    debug_num_loop++;
    HighsInt debug_loop_ln = 0;
    for (HighsInt i = workCount; i < fullCount; i++) {
      HighsInt iCol = workData[i].first;
      HighsFloat value = workData[i].second;
      HighsFloat dual = workMove[iCol] * workDual[iCol];
      // Tight satisfy
      if (dual <= selectTheta * value) {
        swap(workData[workCount++], workData[i]);
        totalChange += value * (workRange[iCol]);
      } else if (dual + Td < remainTheta * value) {
        remainTheta = (dual + Td) / value;
      }
      debug_loop_ln++;
    }
    workGroup.push_back(workCount);

    // Update selectTheta with the value of remainTheta;
    selectTheta = remainTheta;
    // Check for no change in this loop - to prevent infinite loop
    if ((workCount == prev_workCount) && (prev_selectTheta == selectTheta) &&
        (prev_remainTheta == remainTheta)) {
      HighsInt num_var =
          ekk_instance_.lp_.num_col_ + ekk_instance_.lp_.num_row_;
      debugDualChuzcFailQuad0(*ekk_instance_.options_, workCount, workData,
                              num_var, workDual, selectTheta, remainTheta,
                              true);
      return false;
    }
    // Record the initial values of workCount, remainTheta and selectTheta for
    // the next pass through the loop - to check for infinite loop condition
    prev_workCount = workCount;
    prev_remainTheta = remainTheta;
    prev_selectTheta = selectTheta;
    if (totalChange >= totalDelta || workCount == fullCount) break;
  }
  // Check that at least one group has been identified
  if ((HighsInt)workGroup.size() <= 1) {
    HighsInt num_var = ekk_instance_.lp_.num_col_ + ekk_instance_.lp_.num_row_;
    debugDualChuzcFailQuad1(*ekk_instance_.options_, workCount, workData,
                            num_var, workDual, selectTheta, true);
    return false;
  }
  return true;
}

bool HEkkDualRow::chooseFinalWorkGroupHeap() {
  const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
  HighsInt fullCount = alt_workCount;
  HighsFloat totalChange = kInitialTotalChange;
  HighsFloat selectTheta = workTheta;
  const HighsFloat totalDelta = fabs(workDelta);
  HighsInt heap_num_en = 0;
  std::vector<HighsInt> heap_i;
  std::vector<HighsFloat> heap_v;
  heap_i.resize(fullCount + 1);
  heap_v.resize(fullCount + 1);
  for (HighsInt i = 0; i < fullCount; i++) {
    HighsInt iCol = original_workData[i].first;
    HighsFloat value = original_workData[i].second;
    HighsFloat dual = workMove[iCol] * workDual[iCol];
    HighsFloat ratio = dual / value;
    if (ratio < kMaxSelectTheta) {
      heap_num_en++;
      heap_i[heap_num_en] = i;
      heap_v[heap_num_en] = ratio;
    }
  }
  maxheapsort(&heap_v[0], &heap_i[0], heap_num_en);

  alt_workCount = 0;
  alt_workGroup.clear();
  alt_workGroup.push_back(alt_workCount);
  if (heap_num_en <= 0) {
    HighsInt num_var = ekk_instance_.lp_.num_col_ + ekk_instance_.lp_.num_row_;
    // No entries in heap = > failure
    debugDualChuzcFailHeap(*ekk_instance_.options_, alt_workCount,
                           original_workData, num_var, workDual, selectTheta,
                           true);
    return false;
  }
  HighsInt this_group_first_entry = alt_workCount;
  sorted_workData.resize(heap_num_en);
  for (HighsInt en = 1; en <= heap_num_en; en++) {
    HighsInt i = heap_i[en];
    HighsInt iCol = original_workData[i].first;
    HighsFloat value = original_workData[i].second;
    HighsFloat dual = workMove[iCol] * workDual[iCol];
    if (dual > selectTheta * value) {
      // Breakpoint is in the next group, so record the pointer to its
      // first entry
      alt_workGroup.push_back(alt_workCount);
      this_group_first_entry = alt_workCount;
      HighsInt alt_workGroup_size = alt_workGroup.size();
      selectTheta = (dual + Td) / value;
      // End loop if all permitted groups have been identified
      if (totalChange >= totalDelta) break;
    }
    // Store the breakpoint
    sorted_workData[alt_workCount].first = iCol;
    sorted_workData[alt_workCount].second = value;
    totalChange += value * (workRange[iCol]);
    alt_workCount++;
  }
  if (alt_workCount > this_group_first_entry)
    alt_workGroup.push_back(alt_workCount);
  return true;
}

void HEkkDualRow::chooseFinalLargeAlpha(
    HighsInt& breakIndex, HighsInt& breakGroup, HighsInt pass_workCount,
    const std::vector<std::pair<HighsInt, HighsFloat>>& pass_workData,
    const std::vector<HighsInt>& pass_workGroup) {
  HighsFloat finalCompare = 0;
  for (HighsInt i = 0; i < pass_workCount; i++)
    finalCompare = max(finalCompare, pass_workData[i].second);
  finalCompare = min(0.1 * finalCompare, 1.0);
  HighsInt countGroup = pass_workGroup.size() - 1;
  breakGroup = -1;
  breakIndex = -1;
  for (HighsInt iGroup = countGroup - 1; iGroup >= 0; iGroup--) {
    HighsFloat dMaxFinal = 0;
    HighsInt iMaxFinal = -1;
    for (HighsInt i = pass_workGroup[iGroup]; i < pass_workGroup[iGroup + 1];
         i++) {
      if (dMaxFinal < pass_workData[i].second) {
        dMaxFinal = pass_workData[i].second;
        iMaxFinal = i;
      } else if (dMaxFinal == pass_workData[i].second) {
        HighsInt jCol = pass_workData[iMaxFinal].first;
        HighsInt iCol = pass_workData[i].first;
        if (workNumTotPermutation[iCol] < workNumTotPermutation[jCol]) {
          iMaxFinal = i;
        }
      }
    }

    if (pass_workData[iMaxFinal].second > finalCompare) {
      breakIndex = iMaxFinal;
      breakGroup = iGroup;
      break;
    }
  }
}

void HEkkDualRow::updateFlip(HVector* bfrtColumn) {
  HighsFloat* workDual = &ekk_instance_.info_.workDual_[0];
  HighsFloat dual_objective_value_change = 0;
  bfrtColumn->clear();
  for (HighsInt i = 0; i < workCount; i++) {
    const HighsInt iCol = workData[i].first;
    const HighsFloat change = workData[i].second;
    HighsFloat local_dual_objective_change = change * workDual[iCol];
    local_dual_objective_change *= ekk_instance_.cost_scale_;
    dual_objective_value_change += local_dual_objective_change;
    ekk_instance_.flipBound(iCol);
    ekk_instance_.lp_.a_matrix_.collectAj(*bfrtColumn, iCol, change);
  }
  ekk_instance_.info_.updated_dual_objective_value +=
      dual_objective_value_change;
}

void HEkkDualRow::updateDual(HighsFloat theta) {
  analysis->simplexTimerStart(UpdateDualClock);
  HighsFloat* workDual = &ekk_instance_.info_.workDual_[0];
  HighsFloat dual_objective_value_change = 0;
  for (HighsInt i = 0; i < packCount; i++) {
    workDual[packIndex[i]] -= theta * packValue[i];
    // Identify the change to the dual objective
    HighsInt iCol = packIndex[i];
    const HighsFloat delta_dual = theta * packValue[i];
    const HighsFloat local_value = ekk_instance_.info_.workValue_[iCol];
    HighsFloat local_dual_objective_change =
        ekk_instance_.basis_.nonbasicFlag_[iCol] * (-local_value * delta_dual);
    local_dual_objective_change *= ekk_instance_.cost_scale_;
    dual_objective_value_change += local_dual_objective_change;
  }
  ekk_instance_.info_.updated_dual_objective_value +=
      dual_objective_value_change;
  analysis->simplexTimerStop(UpdateDualClock);
}

void HEkkDualRow::createFreelist() {
  freeList.clear();
  for (HighsInt i = 0;
       i < ekk_instance_.lp_.num_col_ + ekk_instance_.lp_.num_row_; i++) {
    if (ekk_instance_.basis_.nonbasicFlag_[i] &&
        highs_isInfinity(-ekk_instance_.info_.workLower_[i]) &&
        highs_isInfinity(ekk_instance_.info_.workUpper_[i]))
      freeList.insert(i);
  }
  //  debugFreeListNumEntries(ekk_instance_, freeList);
}

void HEkkDualRow::createFreemove(HVector* row_ep) {
  // TODO: Check with Qi what this is doing and why it's expensive
  if (!freeList.empty()) {
    HighsFloat Ta = ekk_instance_.info_.update_count < 10
                    ? 1e-9
                    : ekk_instance_.info_.update_count < 20 ? 3e-8 : 1e-6;
    HighsInt move_out = workDelta < 0 ? -1 : 1;
    set<HighsInt>::iterator sit;
    for (sit = freeList.begin(); sit != freeList.end(); sit++) {
      HighsInt iCol = *sit;
      assert(iCol < ekk_instance_.lp_.num_col_);
      HighsFloat alpha = ekk_instance_.lp_.a_matrix_.computeDot(*row_ep, iCol);
      if (fabs(alpha) > Ta) {
        if (alpha * move_out > 0)
          ekk_instance_.basis_.nonbasicMove_[iCol] = 1;
        else
          ekk_instance_.basis_.nonbasicMove_[iCol] = -1;
      }
    }
  }
}
void HEkkDualRow::deleteFreemove() {
  if (!freeList.empty()) {
    set<HighsInt>::iterator sit;
    for (sit = freeList.begin(); sit != freeList.end(); sit++) {
      HighsInt iCol = *sit;
      assert(iCol < ekk_instance_.lp_.num_col_);
      ekk_instance_.basis_.nonbasicMove_[iCol] = 0;
    }
  }
}

void HEkkDualRow::deleteFreelist(HighsInt iColumn) {
  if (!freeList.empty()) {
    if (freeList.count(iColumn)) freeList.erase(iColumn);
  }
}

void HEkkDualRow::computeDevexWeight(const HighsInt slice) {
  const bool rp_computed_edge_weight = false;
  computed_edge_weight = 0;
  for (HighsInt el_n = 0; el_n < packCount; el_n++) {
    HighsInt vr_n = packIndex[el_n];
    if (!ekk_instance_.basis_.nonbasicFlag_[vr_n]) continue;
    HighsFloat pv = work_devex_index[vr_n] * packValue[el_n];
    if (pv) {
      computed_edge_weight += pv * pv;
    }
  }
  if (rp_computed_edge_weight) {
    if (slice >= 0)
      printf("HEkkDualRow::computeDevexWeight: Slice %1" HIGHSINT_FORMAT
             "; computed_edge_weight = "
             "%11.4g\n",
             slice, computed_edge_weight);
  }
}

HighsInt HEkkDualRow::debugFindInWorkData(
    const HighsInt iCol, const HighsInt count,
    const std::vector<std::pair<HighsInt, HighsFloat>>& workData_) {
  for (HighsInt Ix = 0; Ix < count; Ix++)
    if (workData_[Ix].first == iCol) return Ix;
  return -1;
}

HighsInt HEkkDualRow::debugChooseColumnInfeasibilities() const {
  HighsInt num_infeasibility = 0;
  if (ekk_instance_.options_->highs_debug_level < kHighsDebugLevelCheap)
    return num_infeasibility;
  const HighsInt move_out = workDelta < 0 ? -1 : 1;
  std::vector<HighsFloat> unpack_value;
  HighsLp& lp = ekk_instance_.lp_;
  unpack_value.resize(lp.num_col_ + lp.num_row_);
  for (HighsInt ix = 0; ix < packCount; ix++)
    unpack_value[packIndex[ix]] = packValue[ix];
  const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
  for (HighsInt i = 0; i < workCount; i++) {
    const HighsInt iCol = workData[i].first;
    const HighsFloat delta = workData[i].second;
    const HighsFloat value = unpack_value[iCol];
    const HighsInt move = workMove[iCol];
    const HighsFloat dual = workDual[iCol];
    const HighsFloat delta_dual = fabs(workTheta * value);
    const HighsFloat new_dual = dual - workTheta * value;
    const HighsFloat infeasibility_after_flip = -move * new_dual;
    const bool infeasible = infeasibility_after_flip < -Td;
    if (infeasible) {
      printf(
          "%3d: iCol = %4d; dual = %11.4g; value = %11.4g; move = %2d; delta = "
          "%11.4g; new_dual = %11.4g; infeasibility = %11.4g: %d\n",
          (int)i, (int)iCol, dual, value, (int)move, delta_dual, new_dual,
          infeasibility_after_flip, infeasible);

      num_infeasibility++;
    }
  }
  assert(!num_infeasibility);
  return num_infeasibility;
}
void HEkkDualRow::debugReportBfrtVar(
    const HighsInt ix,
    const std::vector<std::pair<HighsInt, HighsFloat>>& pass_workData) const {
  const HighsInt move_out = workDelta < 0 ? -1 : 1;
  const HighsFloat Td = ekk_instance_.options_->dual_feasibility_tolerance;
  const HighsInt iCol = sorted_workData[ix].first;
  const HighsFloat value = sorted_workData[ix].second;
  const HighsInt move = workMove[iCol];
  const HighsFloat dual = workDual[iCol];
  const HighsFloat new_dual = dual - move_out * move * workTheta * value;
  const HighsFloat new_dual_infeasibility = move * new_dual;
  const bool infeasible = new_dual_infeasibility < -Td;
  if (ix < 0) {
    printf(
        "Ix iCol Mv       Lower      Primal       Upper       Value        "
        "Dual       Ratio      NwDual Ifs\n");
    return;
  }
  printf("%2d %4d %2d %11.4g %11.4g %11.4g %11.4g %11.4g %11.4g %11.4g %3d\n",
         (int)ix, (int)iCol, (int)move, ekk_instance_.info_.workLower_[iCol],
         ekk_instance_.info_.workValue_[iCol],
         ekk_instance_.info_.workUpper_[iCol], value, dual, fabs(dual / value),
         new_dual, infeasible);
}
