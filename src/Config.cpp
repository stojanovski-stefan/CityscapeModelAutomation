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

#include "Config.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>
#include <string>
#include <unordered_map>

/**
 * Updates the Config struct's member variable to the values given by the
 * user through a config file (.txt).
 *
 * \param[in] filePath path to the config file.
 * \return    Config object with updated values.
 */
config::Config config::loadConfigFile(const std::string &filePath) {
  // at this point in the program, we assume a config file was given through
  // command line args
  assert(!filePath.empty());

  Config cfg{}; // already has default values
  auto configSetting = getConfigFileValues(filePath);

  // could not open given file path
  if (!configSetting.has_value())
    return cfg;

  // get ref to the map now that we confirmed it exists
  const std::unordered_map<std::string, std::string> &configMap{
      configSetting.value()};

  /**
   * Lambda function that updates a single string field in the Config struct.
   *
   * \param[in]  key expected to be in the config file.
   * \param[out] field reference to member variable in Config struct to be
   *             updated.
   * \return nothing
   */
  auto getStr = [&](const std::string &key, std::string &field) {
    auto iter = configMap.find(key);
    if (iter != configMap.end())
      field = configMap.at(key);
    else
      // user may have only wanted to change a subset of config fields
      std::println(stderr, "Warning: no value provided for {}, keeping default",
                   key);
  };

  /**
   * Lambda function that updates a single integer field in the Config struct.
   *
   * \param[in]  key expected to be in the config file.
   * \param[out] field reference to member variable in Config struct to be
   *             updated.
   * \return nothing
   */
  auto getInt = [&](const std::string &key, uint16_t &field) {
    auto iter = configMap.find(key);
    if (iter != configMap.end()) {
      try {
        // track how much was consumed so trailing junk ("2025x") is rejected
        std::size_t parsedChars{};
        const int value{std::stoi(iter->second, &parsedChars)};

        if (parsedChars != iter->second.size())
          std::println(stderr,
                       "Warning: invalid integer for {}: {}. Keeping default.",
                       key, iter->second);
        else if (value < 0 || value > UINT16_MAX)
          std::println(
              stderr,
              "Warning: out of range integer for {}: {}. Keeping default.", key,
              iter->second);
        else
          field = static_cast<uint16_t>(value);
      } catch (const std::exception &) {
        std::println(stderr,
                     "Warning: invalid integer for {}: {}. Keeping default.",
                     key, iter->second);
      }
    } else {
      // user may have only wanted to change a subset of config fields
      std::println(stderr, "Warning: no value provided for {}, keeping default",
                   key);
    }
  };

  getInt("tiger_year", cfg.tigerYear);
  getInt("gaz_year", cfg.gazYear);
  getInt("pums_year", cfg.pumsYear);
  getStr("pums_span", cfg.pumsSpan);
  getStr("output_dir", cfg.outputDir);
  getStr("user_agent", cfg.userAgent);
  getStr("census", cfg.census);
  getStr("geofabrik", cfg.geofabrik);

  if (cfg.pumsSpan != "5-Year" && cfg.pumsSpan != "1-Year") {
    std::println(
        stderr,
        "Ivalid pums_span value in config: {} (expected 5-Year or 1-Year)",
        cfg.pumsSpan);
    std::exit(EXIT_FAILURE);
  }

  return cfg;
}
