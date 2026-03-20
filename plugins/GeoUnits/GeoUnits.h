#pragma once

#include <string>

#include "json.hpp"

namespace GeoUnits {

// Conversion factors: each unit value equals how many meters
double MetersPerUnit(const std::string& units);

// Convert value from a unit to meters
double ToMeters(double value, const std::string& units);

// Convert value from meters to a unit
double FromMeters(double meters, const std::string& units);

// Read units from options object, with default
std::string ReadUnits(const nlohmann::json& options, const std::string& defaultUnits = "kilometers");

// Supported units: meters, kilometers, miles, nauticalmiles, degrees, radians

} // namespace GeoUnits
