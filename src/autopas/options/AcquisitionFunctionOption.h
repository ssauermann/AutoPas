/**
 * @file AcquisitionFunctionOption.h
 * @author Jan Nguyen
 * @date 26.06.19
 */

#pragma once

#include <set>

#include "autopas/options/Option.h"

namespace autopas {

/**
 * Class representing the acquisition function choices for the Bayesian search.
 */
class AcquisitionFunctionOption : public Option<AcquisitionFunctionOption> {
 public:
  /**
   * Different acquisition functions
   */
  enum Value {
    upperConfidenceBound,
    lowerConfidenceBound,
    mean,
    variance,
    probabilityOfDecrease,
    expectedDecrease,
  };

  /**
   * Constructor.
   */
  AcquisitionFunctionOption() = default;

  /**
   * Constructor from value.
   * @param option
   */
  constexpr AcquisitionFunctionOption(Value option) : _value(option) {}

  /**
   * Cast to value.
   * @return
   */
  constexpr operator Value() const { return _value; }

  /**
   * Provides a way to iterate over the possible choices of AcquisitionFunction.
   * @return map option -> string representation
   */
  static std::map<AcquisitionFunctionOption, std::string> getOptionNames() {
    return {
        {AcquisitionFunctionOption::upperConfidenceBound, "upper-confidence-bound"},
        {AcquisitionFunctionOption::lowerConfidenceBound, "lower-confidence-bound"},
        {AcquisitionFunctionOption::mean, "mean"},
        {AcquisitionFunctionOption::variance, "variance"},
        {AcquisitionFunctionOption::probabilityOfDecrease, "probability-of-decrease"},
        {AcquisitionFunctionOption::expectedDecrease, "expected-decrease"},
    };
  };

 private:
  Value _value{Value(-1)};
};
}  // namespace autopas
