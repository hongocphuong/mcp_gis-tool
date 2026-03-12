#include "tools/Wgs84Tools.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusMeters = 6371008.8;

constexpr char kDistanceSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "lat1": { "type": "number", "description": "Latitude of point A in decimal degrees (-90 to 90)." },
        "lon1": { "type": "number", "description": "Longitude of point A in decimal degrees (-180 to 180)." },
        "lat2": { "type": "number", "description": "Latitude of point B in decimal degrees (-90 to 90)." },
        "lon2": { "type": "number", "description": "Longitude of point B in decimal degrees (-180 to 180)." }
    },
    "required": ["lat1", "lon1", "lat2", "lon2"],
    "additionalProperties": false
})";

constexpr char kPolylineSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {
            "type": "array",
            "minItems": 2,
            "items": {
                "type": "object",
                "properties": {
                    "lat": { "type": "number", "description": "Latitude in decimal degrees (-90 to 90)." },
                    "lon": { "type": "number", "description": "Longitude in decimal degrees (-180 to 180)." }
                },
                "required": ["lat", "lon"],
                "additionalProperties": false
            },
            "description": "Ordered polyline vertices in WGS84."
        }
    },
    "required": ["points"],
    "additionalProperties": false
})";

constexpr char kAzimuthSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "lat1": { "type": "number", "description": "Latitude of start point in decimal degrees (-90 to 90)." },
        "lon1": { "type": "number", "description": "Longitude of start point in decimal degrees (-180 to 180)." },
        "lat2": { "type": "number", "description": "Latitude of end point in decimal degrees (-90 to 90)." },
        "lon2": { "type": "number", "description": "Longitude of end point in decimal degrees (-180 to 180)." },
        "return_direction": { "type": "boolean", "description": "When true, include cardinal direction text (N, NE, E, ...)." },
        "direction_sectors": { "type": "integer", "enum": [8, 16], "description": "Compass sectors for direction text. Supported values: 8 or 16." }
    },
    "required": ["lat1", "lon1", "lat2", "lon2"],
    "additionalProperties": false
})";

constexpr char kReverseAzimuthSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "lat1": { "type": "number", "description": "Latitude of start point in decimal degrees (-90 to 90)." },
        "lon1": { "type": "number", "description": "Longitude of start point in decimal degrees (-180 to 180)." },
        "lat2": { "type": "number", "description": "Latitude of end point in decimal degrees (-90 to 90)." },
        "lon2": { "type": "number", "description": "Longitude of end point in decimal degrees (-180 to 180)." },
        "return_direction": { "type": "boolean", "description": "When true, include cardinal direction text (N, NE, E, ...)." },
        "direction_sectors": { "type": "integer", "enum": [8, 16], "description": "Compass sectors for direction text. Supported values: 8 or 16." }
    },
    "required": ["lat1", "lon1", "lat2", "lon2"],
    "additionalProperties": false
})";

double DegreesToRadians(double degrees) {
    return degrees * (kPi / 180.0);
}

double RadiansToDegrees(double radians) {
    return radians * (180.0 / kPi);
}

double NormalizeDegrees360(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

bool IsValidWgs84Coordinate(double latitude, double longitude) {
    return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

void ValidateCoordinate(double latitude, double longitude) {
    if (!IsValidWgs84Coordinate(latitude, longitude)) {
        throw std::invalid_argument("WGS84 coordinates out of range. Latitude must be [-90, 90], longitude must be [-180, 180].");
    }
}

int ParseDirectionSectors(const json& arguments) {
    int sectors = 8;
    if (arguments.contains("direction_sectors")) {
        sectors = arguments.at("direction_sectors").get<int>();
        if (sectors != 8 && sectors != 16) {
            throw std::invalid_argument("direction_sectors must be 8 or 16.");
        }
    }
    return sectors;
}

std::string CardinalDirectionFromBearing(double bearingDegrees, int sectors) {
    static const char* kDirections8[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    static const char* kDirections16[] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };

    const double step = 360.0 / static_cast<double>(sectors);
    const int index = static_cast<int>(std::floor((NormalizeDegrees360(bearingDegrees) + (step / 2.0)) / step)) % sectors;

    if (sectors == 16) {
        return kDirections16[index];
    }

    return kDirections8[index];
}

double InitialBearingDegrees(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLonRad = DegreesToRadians(lon2 - lon1);

    const double y = std::sin(dLonRad) * std::cos(lat2Rad);
    const double x = (std::cos(lat1Rad) * std::sin(lat2Rad)) -
                     (std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(dLonRad));

    return NormalizeDegrees360(RadiansToDegrees(std::atan2(y, x)));
}

double HaversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLat = DegreesToRadians(lat2 - lat1);
    const double dLon = DegreesToRadians(lon2 - lon1);

    const double sinHalfDLat = std::sin(dLat / 2.0);
    const double sinHalfDLon = std::sin(dLon / 2.0);
    const double a = (sinHalfDLat * sinHalfDLat) +
                     std::cos(lat1Rad) * std::cos(lat2Rad) *
                     (sinHalfDLon * sinHalfDLon);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return kEarthRadiusMeters * c;
}

ToolResult CalculateWgs84Distance(const json& arguments) {
    const double lat1 = arguments.at("lat1").get<double>();
    const double lon1 = arguments.at("lon1").get<double>();
    const double lat2 = arguments.at("lat2").get<double>();
    const double lon2 = arguments.at("lon2").get<double>();

    ValidateCoordinate(lat1, lon1);
    ValidateCoordinate(lat2, lon2);

    const double distanceMeters = HaversineDistanceMeters(lat1, lon1, lat2, lon2);
    const double distanceKilometers = distanceMeters / 1000.0;

    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "WGS84 distance: " << distanceMeters << " meters ("
            << distanceKilometers << " km)";

    return {
        message.str(),
        {
            {"coordinate_system", "WGS84"},
            {"formula", "haversine"},
            {"distance_meters", distanceMeters},
            {"distance_kilometers", distanceKilometers},
            {"input", {
                {"point_a", {{"lat", lat1}, {"lon", lon1}}},
                {"point_b", {{"lat", lat2}, {"lon", lon2}}}
            }}
        }
    };
}

ToolResult CalculateWgs84PolylineLength(const json& arguments) {
    const auto points = arguments.at("points");
    if (!points.is_array() || points.size() < 2) {
        throw std::invalid_argument("points must be an array with at least 2 coordinates.");
    }

    double totalMeters = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const double lat = points[i].at("lat").get<double>();
        const double lon = points[i].at("lon").get<double>();
        ValidateCoordinate(lat, lon);

        if (i == 0) {
            continue;
        }

        const double prevLat = points[i - 1].at("lat").get<double>();
        const double prevLon = points[i - 1].at("lon").get<double>();
        totalMeters += HaversineDistanceMeters(prevLat, prevLon, lat, lon);
    }

    const double totalKilometers = totalMeters / 1000.0;

    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "WGS84 polyline length: " << totalMeters << " meters ("
            << totalKilometers << " km), segments: " << (points.size() - 1);

    return {
        message.str(),
        {
            {"coordinate_system", "WGS84"},
            {"formula", "haversine_segment_sum"},
            {"point_count", points.size()},
            {"segment_count", points.size() - 1},
            {"length_meters", totalMeters},
            {"length_kilometers", totalKilometers}
        }
    };
}

ToolResult CalculateWgs84Azimuth(const json& arguments) {
    const double lat1 = arguments.at("lat1").get<double>();
    const double lon1 = arguments.at("lon1").get<double>();
    const double lat2 = arguments.at("lat2").get<double>();
    const double lon2 = arguments.at("lon2").get<double>();

    ValidateCoordinate(lat1, lon1);
    ValidateCoordinate(lat2, lon2);

    if (lat1 == lat2 && lon1 == lon2) {
        throw std::invalid_argument("Cannot calculate azimuth for identical start and end coordinates.");
    }

    const bool returnDirection = arguments.contains("return_direction") ? arguments.at("return_direction").get<bool>() : false;
    const int directionSectors = ParseDirectionSectors(arguments);
    const double azimuthDegrees = InitialBearingDegrees(lat1, lon1, lat2, lon2);

    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "WGS84 azimuth: " << azimuthDegrees << " degrees";

    json data = {
        {"coordinate_system", "WGS84"},
        {"formula", "initial_bearing"},
        {"azimuth_degrees", azimuthDegrees},
        {"input", {
            {"point_a", {{"lat", lat1}, {"lon", lon1}}},
            {"point_b", {{"lat", lat2}, {"lon", lon2}}}
        }}
    };

    if (returnDirection) {
        const std::string direction = CardinalDirectionFromBearing(azimuthDegrees, directionSectors);
        message << " (" << direction << ")";
        data["direction"] = direction;
        data["direction_sectors"] = directionSectors;
    }

    return {
        message.str(),
        data
    };
}

ToolResult CalculateWgs84ReverseAzimuth(const json& arguments) {
    const double lat1 = arguments.at("lat1").get<double>();
    const double lon1 = arguments.at("lon1").get<double>();
    const double lat2 = arguments.at("lat2").get<double>();
    const double lon2 = arguments.at("lon2").get<double>();

    ValidateCoordinate(lat1, lon1);
    ValidateCoordinate(lat2, lon2);

    if (lat1 == lat2 && lon1 == lon2) {
        throw std::invalid_argument("Cannot calculate reverse azimuth for identical start and end coordinates.");
    }

    const bool returnDirection = arguments.contains("return_direction") ? arguments.at("return_direction").get<bool>() : false;
    const int directionSectors = ParseDirectionSectors(arguments);

    // Back bearing from B to A.
    const double reverseAzimuthDegrees = InitialBearingDegrees(lat2, lon2, lat1, lon1);

    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "WGS84 reverse azimuth: " << reverseAzimuthDegrees << " degrees";

    json data = {
        {"coordinate_system", "WGS84"},
        {"formula", "initial_bearing_reverse"},
        {"reverse_azimuth_degrees", reverseAzimuthDegrees},
        {"input", {
            {"point_a", {{"lat", lat1}, {"lon", lon1}}},
            {"point_b", {{"lat", lat2}, {"lon", lon2}}}
        }}
    };

    if (returnDirection) {
        const std::string direction = CardinalDirectionFromBearing(reverseAzimuthDegrees, directionSectors);
        message << " (" << direction << ")";
        data["direction"] = direction;
        data["direction_sectors"] = directionSectors;
    }

    return {
        message.str(),
        data
    };
}

} // namespace

void RegisterWgs84Tools(ToolRegistry& registry) {
    registry.Register({
        {"calculate_wgs84_distance", "Calculate geodesic distance between two WGS84 coordinates using Haversine formula.", kDistanceSchema},
        CalculateWgs84Distance
    });

    registry.Register({
        {"calculate_wgs84_polyline_length", "Calculate total length of a WGS84 polyline by summing Haversine distance of each segment.", kPolylineSchema},
        CalculateWgs84PolylineLength
    });

    registry.Register({
        {"calculate_wgs84_azimuth", "Calculate initial azimuth (bearing) from point A to point B in WGS84.", kAzimuthSchema},
        CalculateWgs84Azimuth
    });

    registry.Register({
        {"calculate_wgs84_reverse_azimuth", "Calculate reverse azimuth (back bearing) from point B to point A in WGS84.", kReverseAzimuthSchema},
        CalculateWgs84ReverseAzimuth
    });
}
