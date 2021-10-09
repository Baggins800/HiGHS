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
/**@file simplex/HFactorUtils.cpp
 * @brief Types of solution classes
 */
#include "simplex/HFactor.h"

void HFactor::invalidAMatrixAction() {
  this->a_matrix_valid = false;
  refactor_info_.clear();
}

void HFactor::reportLu(const HighsInt l_u_or_both, const bool full) const {
  if (l_u_or_both < kReportLuJustL || l_u_or_both > kReportLuBoth) return;
  if (l_u_or_both & 1) {
    printf("L");
    if (full) printf(" - full");
    printf(":\n");

    if (full) reportIntVector("LpivotLookup", LpivotLookup);
    if (full) reportIntVector("LpivotIndex", LpivotIndex);
    reportIntVector("Lstart", Lstart);
    reportIntVector("Lindex", Lindex);
    reportD0ubleVector("Lvalue", Lvalue);
    if (full) {
      reportIntVector("LRstart", LRstart);
      reportIntVector("LRindex", LRindex);
      reportD0ubleVector("LRvalue", LRvalue);
    }
  }
  if (l_u_or_both & 2) {
    printf("U");
    if (full) printf(" - full");
    printf(":\n");
    if (full) reportIntVector("UpivotLookup", UpivotLookup);
    reportIntVector("UpivotIndex", UpivotIndex);
    reportD0ubleVector("UpivotValue", UpivotValue);
    reportIntVector("Ustart", Ustart);
    if (full) reportIntVector("Ulastp", Ulastp);
    reportIntVector("Uindex", Uindex);
    reportD0ubleVector("Uvalue", Uvalue);
    if (full) {
      reportIntVector("URstart", URstart);
      reportIntVector("URlastp", URlastp);
      reportIntVector("URspace", URspace);
      for (HighsInt iRow = 0; iRow < URstart.size(); iRow++) {
        const HighsInt start = URstart[iRow];
        const HighsInt end = URlastp[iRow];
        if (start >= end) continue;
        printf("UR    Row %2d: ", (int)iRow);
        for (HighsInt iEl = start; iEl < end; iEl++)
          printf("%11d ", (int)URindex[iEl]);
        printf("\n              ");
        for (HighsInt iEl = start; iEl < end; iEl++)
          printf("%11.4g ", (double)URvalue[iEl]);
        printf("\n");
      }
      //      reportIntVector("URindex", URindex);
      //      reportD0ubleVector("URvalue", URvalue);
    }
  }
  if (l_u_or_both == 3 && full) {
    reportD0ubleVector("PFpivotValue", PFpivotValue);
    reportIntVector("PFpivotIndex", PFpivotIndex);
    reportIntVector("PFstart", PFstart);
    reportIntVector("PFindex", PFindex);
    reportD0ubleVector("PFvalue", PFvalue);
  }
}

void HFactor::reportIntVector(const std::string name,
                              const vector<HighsInt> entry) const {
  const HighsInt num_en = entry.size();
  printf("%-12s: siz %4d; cap %4d: ", name.c_str(), (int)num_en,
         (int)entry.capacity());
  for (HighsInt iEn = 0; iEn < num_en; iEn++) {
    if (iEn > 0 && iEn % 10 == 0)
      printf("\n                                  ");
    printf("%11d ", (int)entry[iEn]);
  }
  printf("\n");
}
void HFactor::reportD0ubleVector(const std::string name,
                                 const vector<HighsFloat> entry) const {
  const HighsInt num_en = entry.size();
  printf("%-12s: siz %4d; cap %4d: ", name.c_str(), (int)num_en,
         (int)entry.capacity());
  for (HighsInt iEn = 0; iEn < num_en; iEn++) {
    if (iEn > 0 && iEn % 10 == 0)
      printf("\n                                  ");
    printf("%11.4g ", (double)entry[iEn]);
  }
  printf("\n");
}
