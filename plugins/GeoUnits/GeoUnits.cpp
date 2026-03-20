#include "GeoUnits.h"

#include <stdexcept>

#include "../GeoMath/GeoMath.h"
#include "json.hpp"

namespace GeoUnits {

using GeoMath::kPi;
using GeoMath::kEarthRadiusMeters;

double MetersPerUnit(const std::string& units) {
    if (units == "meters" || units == "meter") {
        return 1.0;
    }
    if (units == "kilometers" || units == "kilometer") {
        return 1000.0;
    }
    if (units == "miles" || units == "mile") {
        return 1609.344;
    }
    if (units == "nauticalmiles" || units == "nauticalmile") {
        return 1852.0;
    }
    if (units == "radians" || units == "radian") {
        return kEarthRadiusMeters;
    }
    if (units == "degrees" || units == "degree") {
        return kEarthRadiusMeters * (kPi / 180.0);
    }

    throw std::invalid_argument("Unsupported units: " + units + ". Supported: meters, kilometers, miles, nauticalmiles, degrees, radians.");
}

double ToMeters(double value, const std::string& units) {
    return value * MetersPerUnit(units);
}

double FromMeters(double meters, const std::string& units) {
    return meters / MetersPerUnit(units);
}

std::string ReadUnits(const nlohmann::json& options, const std::string& defaultUnits) {
    if (!options.contains("units")) {
        return defaultUnits;
    }

    const std::string units = options.at("units").get<std::string>();
    return units;
}

} // namespace GeoUnits
