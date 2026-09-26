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

#include "CityDataMiner.hpp"
#include <cstdint>
#include <fstream>
#include <print>
#include <string_view>

/**
 * Removes any leading and trailing whitespace from a *temporary* string. Safe
 * for use with str.substr. Only visible inside this translation unit.
 *
 * \param[in] view of string to be trimmed.
 *
 * \return view of trimmed string or empty view if an empty string is given.
 */
static std::string_view trim(std::string_view s) {
  auto isSpace = [](unsigned char c) { return std::isspace(c); };

  auto start = std::ranges::find_if_not(s, isSpace);
  auto end = std::ranges::find_if_not(s | std::views::reverse, isSpace).base();

  if (start < end)
    return std::string_view(start, end);

  return std::string_view{};
}

/**
 * Opens file stream to a file of city names, reads the city names and state
 * abbreviations, and pushes them to a vector of (name, abbreviation) pairs. 
 * Comment ('#') lines are skipped.
 *
 * \param[in] cityFilePath path to the file of city names that is read.
 * \return    vectory of pairs where pair.first is the city name and 
 *            pair.second is the USPS state abbreviation.
 */
CityList CityDataMiner::readCityFile(const std::string &cityFilePath) const {
  CityList cities{};
  std::ifstream cityFile(cityFilePath);

  // return empty list
  if (!cityFile.good()) {
    std::println(stderr, "Could not open file {}", cityFilePath);
    return cities;
  }

  uint8_t lineNumber{0};
  std::string line{};
  while (std::getline(cityFile, line)) {
    ++lineNumber;

    // line is a comment or is blank
    if ('#' == line[0] || line.empty())
      continue;

    // get location of ',' delimiter
    auto commaIter = line.find(',');
    if (commaIter == std::string::npos) {
      std::println(stderr, "Warning: skipping malformed line {} in {}: {}",
                   lineNumber, cityFilePath, line);
      continue;
    }

    // get city/state from line - keep spaces in center of line for cities
    // like "San Francisco"
    std::string cityName{trim(line.substr(0, commaIter))};
    std::string stateAbbreviation{trim(line.substr(commaIter + 1))};

    cities.push_back({cityName, stateAbbreviation});
  }

  cityFile.close();
  return cities;
}

