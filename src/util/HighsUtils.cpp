/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2018 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file util/HighsUtils.cpp
 * @brief Class-independent utilities for HiGHS
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */

#include "HConfig.h"
#include "HConst.h"
#include "HighsUtils.h"

#include <cmath>
#include <vector>
#include <stdio.h>

bool highs_isInfinity(double val) {
  if (val >= HIGHS_CONST_INF) return true;
  return false;
}

#ifdef HiGHSDEV
void util_analyseVectorValues(const char *message, int vecDim, std::vector<double> &vec,
			      bool anVLs) {
  if (vecDim == 0) return;
  double log10 = log(10.0);
  const int nVK = 20;
  int nNz = 0;
  int nPosInfV = 0;
  int nNegInfV = 0;
  std::vector<int> posVK;
  std::vector<int> negVK;
  posVK.resize(nVK + 1, 0);
  negVK.resize(nVK + 1, 0);

  const int VLsMxZ = 10;
  std::vector<int> VLsK;
  std::vector<double> VLsV;
  VLsK.resize(VLsMxZ, 0);
  VLsV.resize(VLsMxZ, 0);
  // Ensure that 1.0 and -1.0 are counted
  const int PlusOneIx = 0;
  const int MinusOneIx = 1;
  bool excessVLsV = false;
  int VLsZ = 2;
  VLsV[PlusOneIx] = 1.0;
  VLsV[MinusOneIx] = -1.0;

  for (int ix = 0; ix < vecDim; ix++) {
    double v = vec[ix];
    double absV = std::fabs(v);
    int log10V;
    if (absV > 0) {
      // Nonzero value
      nNz++;
      if (highs_isInfinity(-v)) {
        //-Inf value
        nNegInfV++;
      } else if (highs_isInfinity(v)) {
        //+Inf value
        nPosInfV++;
      } else {
        // Finite nonzero value
        if (absV == 1) {
          log10V = 0;
        } else if (absV == 10) {
          log10V = 1;
        } else if (absV == 100) {
          log10V = 2;
        } else if (absV == 1000) {
          log10V = 3;
        } else {
          log10V = log(absV) / log10;
        }
        if (log10V >= 0) {
          int k = std::min(log10V, nVK);
          posVK[k]++;
        } else {
          int k = std::min(-log10V, nVK);
          negVK[k]++;
        }
      }
    }
    if (anVLs) {
      if (v == 1.0) {
        VLsK[PlusOneIx]++;
      } else if (v == -1.0) {
        VLsK[MinusOneIx]++;
      } else {
        int fdIx = -1;
        for (int ix = 2; ix < VLsZ; ix++) {
          if (v == VLsV[ix]) {
            fdIx = ix;
            break;
          }
        }
        if (fdIx == -1) {
          // New value
          if (VLsZ < VLsMxZ) {
            fdIx = VLsZ;
            VLsV[fdIx] = v;
            VLsK[fdIx]++;
            VLsZ++;
          } else {
            excessVLsV = true;
          }
        } else {
          // Existing value
          VLsK[fdIx]++;
        }
      }
    }
  }
  printf("%s of dimension %d with %d nonzeros (%3d%%): Analysis\n", message,
         vecDim, nNz, 100 * nNz / vecDim);
  if (nNegInfV > 0) printf("   %7d values are -Inf\n", nNegInfV);
  if (nPosInfV > 0) printf("   %7d values are +Inf\n", nPosInfV);
  int k = nVK;
  int vK = posVK[k];
  if (vK > 0) printf("   %7d values satisfy 10^(%3d) <= v < Inf\n", vK, k);
  for (int k = nVK - 1; k >= 0; k--) {
    int vK = posVK[k];
    if (vK > 0)
      printf("   %7d values satisfy 10^(%3d) <= v < 10^(%3d)\n", vK, k, k + 1);
  }
  for (int k = 1; k <= nVK; k++) {
    int vK = negVK[k];
    if (vK > 0)
      printf("   %7d values satisfy 10^(%3d) <= v < 10^(%3d)\n", vK, -k, 1 - k);
  }
  vK = vecDim - nNz;
  if (vK > 0) printf("   %7d values are zero\n", vK);
  if (anVLs) {
    printf("           Value distribution:");
    if (excessVLsV) printf(" More than %d different values", VLsZ);
    printf("\n           Value    Count\n");
    for (int ix = 0; ix < VLsZ; ix++) {
      int pct = ((100.0 * VLsK[ix]) / vecDim) + 0.5;
      printf("     %11.4g %8d (%3d%%)\n", VLsV[ix], VLsK[ix], pct);
    }
  }
}
#endif

