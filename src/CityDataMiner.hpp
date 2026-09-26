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

#ifndef FETCH_CITY_DATA_HPP
#define FETCH_CITY_DATA_HPP

#include "Config.hpp"
#include <cpr/cpr.h>
#include <string>
#include <utility>
#include <vector>

using CityList = std::vector<
    std::pair<std::string /*name*/, std::string /*USPS abbreviation*/>>;

class CityDataMiner {
private:
  CityList inputCities;
  config::Values configuration;

  /**
   * Opens file stream to a file of city names, reads the city names and state
   * abbreviations, and pushes them to a vector of (name, abbreviation) pairs.
   * Comment ('#') lines are skipped.
   *
   * \param[in] cityFilePath path to the file of city names that is read.
   * \return    vectory of pairs where pair.first is the city name and
   *            pair.second is the USPS state abbreviation.
   */
  CityList readCityFile(const std::string &cityFilePath) const;

public:
  /**
   * Constructor - populates member variables from city imput file and
   * configuration file.
   *
   * \param[in] cityFilePath TXT file if city names.
   * \param[in] Configuration Values from configuration file (also TXT).
   */
  CityDataMiner(const std::string &cityFilePath, const config::Values &cfg)
      : inputCities(readCityFile(cityFilePath)), configuration(cfg) {};

  /**
   * Create directories where the automation's outputs land. File tree created:
   *
   * <OUTDIR>/
   *  ├── cache/                shared across cities, persists between runs
   *  │   ├── zips/             every archive downloaded, kept after unpacking
   *  │   ├── gazetteer/
   *  │   ├── tiger/            flat — all states unpack into one directory
   *  │   ├── pums/
   *  │   │   ├── p<st>/        lowercase USPS, e.g. pdc/
   *  │   │   └── h<st>/
   *  │   └── osm/              raw .pbf,
   *  └── cities/
   *      └── <slug>/           one per resolved city: name-designator-st-geoid
   */
  void setupDataFileTree();

  /**
   * Gazetteer is a geographic index that the Census Bureau publishes annually.
   * For each entity, this index gives:
   *
   * |   #   |  Column  |                             What it is                                            |
   * |-------|----------|-----------------------------------------------------------------------------------|
   * | 1     | USPS     | two-letter state code — the script's state filter                                 |
   * | 2     | GEOID    | 7-digit place id, state FIPS + 5-digit place FIPS, **leading zeros significant**  |
   * | 3     | ANSICODE | 8-digit GNIS id; unused here                                                      |
   * | 4     | NAME     | name **with the legal designator appended**: Abbeville city, Abanda CDP           |
   * | 5     | LSAD     | numeric code for that designator                                                  |
   * | 6     | FUNCSTAT | functional status — A active government, S statistical entity                     |
   * | 7–10  | ALAND, AWATER, ALAND_SQMI, AWATER_SQMI |         land/water area, m² then mi²                |
   * | 11–12 | INTPTLAT, INTPTLONG |   internal point (a lat/long guaranteed to fall inside the polygon)    |
   *
   */
  void downloadGazetteerData();
};

#endif // !FETCH_CITY_DATA_HPP
