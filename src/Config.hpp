/**
 * ---------------------------------------------------------------------------
 *       _____ _____ _________     _______  _____          _____  ______ 
 *      / ____|_   _|__   __\ \   / / ____|/ ____|   /\   |  __ \|  ____|
 *     | |      | |    | |   \ \_/ / (___ | |       /  \  | |__) | |__   
 *     | |      | |    | |    \   / \___ \| |      / /\ \ |  ___/|  __|  
 *     | |____ _| |_   | |     | |  ____) | |____ / ____ \| |    | |____ 
 *      \_____|_____|  |_|     |_| |_____/ \_____/_/    \_\_|    |______|
 *
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) PC2Lab Development Team
 * All rights reserved.
 *
 * This file is part of free(dom) software -- you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (GPL)as published by the Free Software Foundation, either
 * version 3 (GPL v3), or (at your option) a later version.
 *
 * The software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Miami University and PC2Lab makes no representations or warranties
 * about the suitability of the software, either express or implied,
 * including but not limited to the implied warranties of
 * merchantability, fitness for a particular purpose, or
 * non-infringement.  Miami University and PC2Lab is not be liable for
 * any damages suffered by licensee as a result of using, result of
 * using, modifying or distributing this software or its derivatives.
 *
 * By using or copying this Software, Licensee agrees to abide by the
 * intellectual property laws, and all other applicable laws of the
 * U.S., and the terms of this license.
 *
 * Authors: Stefan Stojanovski      stojansz@miamiOH.edu
 *
 * ---------------------------------------------------------------------------
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <print>

namespace config {

struct Config {
  std::string outputDir{"./cityscape_data"};
  std::string userAgent{"cityscape-automation/1.0 (stojansz@miamioh.edu)"};
  std::string census{"https://www2.census.gov"};
  std::string geofabrik{"https://download.geofabrik.de/north-america/us"};
  std::string pumsSpan{"5-Year"};
  uint16_t tigerYear{2025};
  uint16_t gazYear = {2024};
  uint16_t pumsYear = {2023};
};

/**
 * Anonymous namespace with function specific to only this file.
 */
namespace {

/**
 * Used to hold key value pairs from input config file. Alias used to imporve
 * readability of the function declaration for getConfigFileValues().
 */
using ConfigFileValues = std::unordered_map<std::string, std::string>;

/**
 * Remove any beginning or trailing whitespace in a line.
 *
 * \param[in] s string to be trimmed.
 * \return    string with no whitespaces
 */
std::optional<std::string> trim(const std::string &s) {
  auto startIter = std::find_if_not(s.begin(), s.end(),
                                    [](char c) { return std::isspace(c); });
  auto endIter = std::find_if_not(s.rbegin(), s.rend(),
                                    [](char c) {return std::isspace(c);}).base();

  if (startIter < endIter)
    return std::string(startIter, endIter);

  return std::nullopt;
}

/**
 * Precursor called in loadConfigFile. Does not change Config struct, only
 * parses input config file, validates key value pairs, and inserts them into an
 * unordered_map.
 *
 * \param[in] filePath path to configuration file.
 * \return    unordered_map of key value pairs from configuration file.
 */
std::optional<ConfigFileValues>
getConfigFileValues(const std::string &filePath) {
  std::unordered_map<std::string, std::string> configSettings{};
  std::ifstream configFile(filePath);

  if (!configFile.is_open()) {
    std::println(stderr, "Warning: could not open {}, using default values.", filePath);
    return std::nullopt;
  }

  std::string line{};
  uint8_t lineNumber{0};
  while (std::getline(configFile, line)) {
    ++lineNumber; 
    auto trimmedLine {trim(line)};

    // blank line
    if (!trimmedLine.has_value())
      continue;

    // line is a comment
    if ('#' == trimmedLine->at(0))
      continue;

    // get location of '=' if there is one
    auto equalsIter = trimmedLine->find('=');
    if (equalsIter == std::string::npos) {
      std::println(stderr, "Warning: skipped malformed line {} in {}: {}.", lineNumber, filePath, line);
      return std::nullopt;
    }

    // get chars up to '=' (exlusive), trimming any space between the key and '='
    std::string key{trim(trimmedLine->substr(0, equalsIter)).value()};

    // get chars after '=' to the end of the line, trimming any space between '=' and value
    std::string value{trim(trimmedLine->substr(equalsIter + 1)).value()};

    configSettings[key] = value;
  }

  return configSettings;
}

} // namespace

/**
 * Updates the Config struct's member variable to the values given by the 
 * user through a config file (.txt).
 *
 * \param[in] filePath path to the config file.
 * \return    Config object with updated values.
 */
Config loadConfigFile(const std::string &filePath);

} // namespace config

#endif // !CONFIG_HPP
