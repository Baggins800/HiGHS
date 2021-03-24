/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2021 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HIGHS_PSEUDOCOST_H_
#define HIGHS_PSEUDOCOST_H_

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

class HighsMipSolver;

class HighsPseudocost {
  std::vector<double> pseudocostup;
  std::vector<double> pseudocostdown;
  std::vector<int> nsamplesup;
  std::vector<int> nsamplesdown;
  std::vector<double> inferencesup;
  std::vector<double> inferencesdown;
  std::vector<int> ninferencesup;
  std::vector<int> ninferencesdown;
  std::vector<int> ncutoffsup;
  std::vector<int> ncutoffsdown;

  double cost_total;
  double inferences_total;
  size_t nsamplestotal;
  size_t ninferencestotal;
  size_t ncutoffstotal;
  int minreliable;

 public:
  HighsPseudocost() = default;
  HighsPseudocost(const HighsMipSolver& mipsolver);

  void subtractBase(const HighsPseudocost& base) {
    int ncols = pseudocostup.size();

    for (int i = 0; i != ncols; ++i) {
      pseudocostup[i] -= base.pseudocostup[i];
      pseudocostdown[i] -= base.pseudocostdown[i];
      nsamplesup[i] -= base.nsamplesup[i];
      nsamplesdown[i] -= base.nsamplesdown[i];
    }
  }

  void setMinReliable(int minreliable) { this->minreliable = minreliable; }

  int getMinReliable() const { return minreliable; }

  int getNumObservations(int col) const {
    return nsamplesup[col] + nsamplesdown[col];
  }

  int getNumObservationsUp(int col) const { return nsamplesup[col]; }

  int getNumObservationsDown(int col) const { return nsamplesdown[col]; }

  void addCutoffObservation(int col, bool upbranch) {
    ++ncutoffstotal;
    if (upbranch)
      ncutoffsup[col] += 1;
    else
      ncutoffsdown[col] += 1;
  }

  void addObservation(int col, double delta, double objdelta) {
    assert(delta != 0.0);
    assert(objdelta >= 0.0);
    if (delta > 0.0) {
      double unit_gain = objdelta / delta;
      double d = unit_gain - pseudocostup[col];
      nsamplesup[col] += 1;
      pseudocostup[col] += d / nsamplesup[col];

      d = unit_gain - cost_total;
      ++nsamplestotal;
      cost_total += d / nsamplestotal;
    } else {
      double unit_gain = -objdelta / delta;
      double d = unit_gain - pseudocostdown[col];
      nsamplesdown[col] += 1;
      pseudocostdown[col] += d / nsamplesdown[col];

      d = unit_gain - cost_total;
      ++nsamplestotal;
      cost_total += d / nsamplestotal;
    }
  }

  void addInferenceObservation(int col, int ninferences, bool upbranch) {
    double d = ninferences - inferences_total;
    ++ninferencestotal;
    inferences_total += d / ninferencestotal;
    if (upbranch) {
      d = ninferences - inferencesup[col];
      ninferencesup[col] += 1;
      inferencesup[col] += d / ninferencesup[col];
    } else {
      d = ninferences - inferencesdown[col];
      ninferencesdown[col] += 1;
      inferencesdown[col] += d / ninferencesdown[col];
    }
  }

  bool isReliable(int col) const {
    return std::min(nsamplesup[col], nsamplesdown[col]) >= minreliable;
  }

  bool isReliableUp(int col) const { return nsamplesup[col] >= minreliable; }

  bool isReliableDown(int col) const {
    return nsamplesdown[col] >= minreliable;
  }

  double getAvgPseudocost() const { return cost_total; }

  double getPseudocostUp(int col, double frac, double offset) const {
    double up = std::ceil(frac) - frac;
    double cost;

    if (nsamplesup[col] == 0 || nsamplesup[col] < minreliable) {
      double weightPs = nsamplesup[col] == 0 ? 0
                                             : 0.75 + 0.25 * nsamplesup[col] /
                                                          (double)minreliable;
      cost = weightPs * pseudocostup[col];
      cost += (1.0 - weightPs) * getAvgPseudocost();
    } else
      cost = pseudocostup[col];
    return up * (offset + cost);
  }

  double getPseudocostDown(int col, double frac, double offset) const {
    double down = frac - std::floor(frac);
    double cost;

    if (nsamplesdown[col] == 0 || nsamplesdown[col] < minreliable) {
      double weightPs =
          nsamplesdown[col] == 0
              ? 0
              : 0.75 + 0.25 * nsamplesdown[col] / (double)minreliable;
      cost = weightPs * pseudocostdown[col];
      cost += (1.0 - weightPs) * getAvgPseudocost();
    } else
      cost = pseudocostdown[col];

    return down * (offset + cost);
  }

  double getPseudocostUp(int col, double frac) const {
    double up = std::ceil(frac) - frac;
    if (nsamplesup[col] == 0) return up * cost_total;
    return up * pseudocostup[col];
  }

  double getPseudocostDown(int col, double frac) const {
    double down = frac - std::floor(frac);
    if (nsamplesdown[col] == 0) return down * cost_total;
    return down * pseudocostdown[col];
  }

  double getScore(int col, double upcost, double downcost) const {
    double costScore =
        std::sqrt(upcost * downcost) / std::max(1e-6, cost_total);
    double inferenceScore = std::sqrt(inferencesup[col] * inferencesdown[col]) /
                            (std::max(1e-6, inferences_total));

    double cutoffRateUp =
        ncutoffsup[col] /
        double(std::max(1, ncutoffsup[col] + nsamplesup[col]));
    double cutoffRateDown =
        ncutoffsdown[col] /
        double(std::max(1, ncutoffsdown[col] + nsamplesdown[col]));
    double avgCutoffRate =
        ncutoffstotal /
        double(std::max(size_t{1}, nsamplestotal + ncutoffstotal));

    double cutoffScore = std::sqrt(cutoffRateUp * cutoffRateDown) /
                         std::max(1e-6, avgCutoffRate);

    auto mapScore = [](double score) { return 1.0 - 1.0 / (1.0 + score); };

    return mapScore(costScore) +
           1e-4 * (mapScore(cutoffScore) + mapScore(inferenceScore));
  }

  double getScore(int col, double frac) const {
    double upcost = getPseudocostUp(col, frac);
    double downcost = getPseudocostDown(col, frac);

    return getScore(col, upcost, downcost);
  }
};

#endif
