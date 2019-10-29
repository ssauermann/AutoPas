/**
 * @file Option.h
 * @author F. Gratl
 * @date 10/14/19
 */

#pragma once

#include <map>
#include <set>
#include <string>

#include "autopas/utils/StringUtils.h"

namespace autopas {

/**
 * Base class for autopas options.
 * @tparam actualOption Curiously recurring template pattern.
 */
template <typename actualOption>
class Option {
 public:
  /**
   * Constructor.
   */
  constexpr Option() = default;

  /**
   * No cast to bool.
   * @return
   */
  explicit operator bool() = delete;

  /**
   * Provides a way to iterate over the possible options.
   * @return set of all possible values of this option type.
   */
  static std::set<actualOption> getAllOptions() {
    std::set<actualOption> retSet;
    auto mapOptionNames = actualOption::getOptionNames();
    std::for_each(mapOptionNames.begin(), mapOptionNames.end(),
                  [&retSet](auto pairOpStr) { retSet.insert(pairOpStr.first); });
    return retSet;
  };

  /**
   * Converts an Option object to its respective string representation.
   * @return The string representation or "Unknown Option (<IntValue>)".
   */
  std::string to_string() const {
    auto &actualThis = *static_cast<const actualOption *>(this);
    auto mapOptNames = actualOption::getOptionNames();  // <- not copying the map destroys the strings
    auto match = mapOptNames.find(actualThis);
    if (match == mapOptNames.end()) {
      return "Unknown Option (" + std::to_string(actualThis) + ")";
    } else {
      return match->second;
    }
  }

  /**
   * Converts a string of options to a set of enums. For best results, the options are expected to be lower case.
   *
   * Allowed delimiters can be found in autopas::utils::StringUtils::delimiters.
   * Possible options can be found in getAllOptions().
   *
   * This function uses the Needleman-Wunsch algorithm to find the closest matching options.
   * If an option is ambiguous an execption is thrown.
   *
   * @param optionsString String containing traversal options.
   * @return Set of option enums. If no valid option was found the empty set is returned.
   */
  static std::set<actualOption> parseOptions(const std::string &optionsString) {
    std::set<actualOption> optionsSet;

    auto needles = autopas::utils::StringUtils::tokenize(optionsString, autopas::utils::StringUtils::delimiters);

    std::vector<std::string> haystack;

    for (auto &option : actualOption::getAllOptions()) {
      auto s = option.to_string();
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      haystack.push_back(s);
    }

    // assure all string representations are lower case
    std::map<actualOption, std::string> allOptionNames;
    for (auto &pairEnumString : actualOption::getOptionNames()) {
      auto s = pairEnumString.second;
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      allOptionNames.emplace(pairEnumString.first, s);
    }

    for (auto &needle : needles) {
      auto matchingString = autopas::utils::StringUtils::matchStrings(haystack, needle);
      for (auto &pairEnumString : allOptionNames) {
        if (pairEnumString.second == matchingString) {
          optionsSet.insert(pairEnumString.first);
          break;
        }
      }
    }

    return optionsSet;
  }

  /**
   * Converts a string to an enum.
   *
   * This function works faster than parseOptions, however, the given string needs to match exactly an option.
   *
   * @tparam lowercase if set to true all option names are transformed to lower case.
   * @param optionString
   * @return Option enum.
   */
  template <bool lowercase = false>
  static actualOption parseOptionExact(const std::string &optionString) {
    for (auto [optionEnum, optionName] : actualOption::getOptionNames()) {
      if (lowercase) {
        std::transform(std::begin(optionName), std::end(optionName), std::begin(optionName), ::tolower);
      }
      if (optionString == optionName) {
        return optionEnum;
      }
    }

    // the end of the function should not be reached
    utils::ExceptionHandler::exception("Option::parseOptionExact() no match found for: {}", optionString);
    return actualOption();
  }
};
}  // namespace autopas
