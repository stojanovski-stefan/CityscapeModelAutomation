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

#ifndef STATES_HPP
#define STATES_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

/**
 * This namespace provides a Federal Information Processing Standard (FIPS) 
 * number to Geofabrik slug translation. Needed to extract OSM XML files,
 * where the Geofabrik URL is spelled out in words 
 * (.../north-america/us/ohio-latest.osm.pbf). This is the only place where 
 * the program derives "ohio" from 39 or  OH.
 *
 * Usage:
 *
 * #include "States.hpp"
 *
 * auto ohio = states::byCode(39);  // get ohio in O(1) time complexity
 *
 * // iterate through states table
 * for (const auto &s : states::STATE_TABLE) { ... }
 *
 */
namespace states {

struct State {
  int fips;                            // two-digit code, e.g. "06"
  std::string_view postalAbbreviation; // e.g. "CA"
  std::string_view name;               // slug name, e.g. "california"

  constexpr bool valid() const { return !postalAbbreviation.empty(); }
};

/**
 * Source data in ascending FIPS order. Used in the construction of a State
 * "Table" - an array of States where the array index equals the fips number
 * allowing O(1) state search. Covers the 53 US subregions Geofabrik 
 * publishes (50 states + DC + PR + VI).
 */
inline constexpr std::array LIST = {
    State{1, "AL", "alabama"},
    State{2, "AK", "alaska"},
    State{4, "AZ", "arizona"},
    State{5, "AR", "arkansas"},
    State{6, "CA", "california"},
    State{8, "CO", "colorado"},
    State{9, "CT", "connecticut"},
    State{10, "DE", "delaware"},
    State{11, "DC", "district-of-columbia"},
    State{12, "FL", "florida"},
    State{13, "GA", "georgia"},
    State{15, "HI", "hawaii"},
    State{16, "ID", "idaho"},
    State{17, "IL", "illinois"},
    State{18, "IN", "indiana"},
    State{19, "IA", "iowa"},
    State{20, "KS", "kansas"},
    State{21, "KY", "kentucky"},
    State{22, "LA", "louisiana"},
    State{23, "ME", "maine"},
    State{24, "MD", "maryland"},
    State{25, "MA", "massachusetts"},
    State{26, "MI", "michigan"},
    State{27, "MN", "minnesota"},
    State{28, "MS", "mississippi"},
    State{29, "MO", "missouri"},
    State{30, "MT", "montana"},
    State{31, "NE", "nebraska"},
    State{32, "NV", "nevada"},
    State{33, "NH", "new-hampshire"},
    State{34, "NJ", "new-jersey"},
    State{35, "NM", "new-mexico"},
    State{36, "NY", "new-york"},
    State{37, "NC", "north-carolina"},
    State{38, "ND", "north-dakota"},
    State{39, "OH", "ohio"},
    State{40, "OK", "oklahoma"},
    State{41, "OR", "oregon"},
    State{42, "PA", "pennsylvania"},
    State{44, "RI", "rhode-island"},
    State{45, "SC", "south-carolina"},
    State{46, "SD", "south-dakota"},
    State{47, "TN", "tennessee"},
    State{48, "TX", "texas"},
    State{49, "UT", "utah"},
    State{50, "VT", "vermont"},
    State{51, "VA", "virginia"},
    State{53, "WA", "washington"},
    State{54, "WV", "west-virginia"},
    State{55, "WI", "wisconsin"},
    State{56, "WY", "wyoming"},
    State{72, "PR", "puerto-rico"},
    State{78, "VI", "us-virgin-islands"},
};

/**
 * Largest FIPS number in LIST becomes the final state table size.
 */
inline constexpr std::size_t TABLE_SIZE = static_cast<std::size_t>(LIST.back().fips) + 1u;

/**
 * Builds array of States where index == fips. 
 *
 * Example:
 * State{1, "AL", "alabama"},
 * State{4, "AZ", "arizona"},
 * State{6, "CA", "california"},
 *
 * index:   0      1      2      3      4      5      6
 *        [empty][ AL ][empty][empty][ AZ ][ empty ][ CA ]
 *
 * \return std::array of States of size TABLE_SIZE
 */
constexpr std::array<State, TABLE_SIZE> buildTable() {
  std::array<State, TABLE_SIZE> table{}; // empty slots have empty strings
  for (const State &s : LIST) {
    table[s.fips] = s;
  }
  return table;
}
inline constexpr std::array<State, TABLE_SIZE> STATE_TABLE = buildTable();

// Lookup by numeric FIPS code, e.g. byCode(6). Returns nullptr if unknown.
/**
 * Lookup state in table with numeric FIPS code.
 * 
 * \param[in] code FIPS number for lookup
 * 
 * \return optional State, if a non-fips index is accessed, return nullopt
 */ 
constexpr std::optional<State> byCode(int code) {
  if (code < 0 || static_cast<std::size_t>(code) >= STATE_TABLE.size())
    return std::nullopt;

  const State &s = STATE_TABLE[static_cast<std::size_t>(code)];
  if (s.valid())
    return s;

  return std::nullopt;
}

// TODO: delete these, may not need
// // Lookup by postal abbreviation, e.g. byAbbrev("CA"). Case-sensitive.
// constexpr const std::optional<State> byAbbrev(std::string_view abbrev) {
//   for (const auto &s : LIST)
//     if (s.postalAbbreviation == abbrev)
//       return s;
//   return std::nullopt;
// }
//
// // Lookup by slug name, e.g. byName("new-york"). Case-sensitive.
// constexpr const std::optional<State> byName(std::string_view name) {
//   for (const auto &s : LIST)
//     if (s.name == name)
//       return s;
//   return std::nullopt;
// }

} // namespace states

#endif // !STATES_HPP
