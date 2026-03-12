#include "tools/GeoJsonTools.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "geojson/GeoJsonTraversal.h"

namespace {

constexpr char kGeoJsonBboxSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {
            "type": "object",
            "description": "A GeoJSON Geometry, Feature, or FeatureCollection object."
        }
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

constexpr char kGeoJsonCentroidSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {
            "type": "object",
            "description": "A GeoJSON Geometry, Feature, or FeatureCollection object."
        }
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

constexpr char kGeoJsonAreaSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {
            "type": "object",
            "description": "A GeoJSON Geometry, Feature, or FeatureCollection object."
        }
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusMeters = 6371008.8;

double DegreesToRadians(double degrees) {
    return degrees * (kPi / 180.0);
}

double RadiansToDegrees(double radians) {
    return radians * (180.0 / kPi);
}

struct BboxAccumulator {
    double minLon = std::numeric_limits<double>::max();
    double minLat = std::numeric_limits<double>::max();
    double maxLon = std::numeric_limits<double>::lowest();
    double maxLat = std::numeric_limits<double>::lowest();
    size_t pointsCount = 0;

    void AddCoordinate(double lon, double lat) {
        minLon = std::min(minLon, lon);
        minLat = std::min(minLat, lat);
        maxLon = std::max(maxLon, lon);
        maxLat = std::max(maxLat, lat);
        ++pointsCount;
    }
};

struct PlanarPoint {
    double x;
    double y;
};

struct SignedAreaCentroid {
    double signedArea;
    double centroidX;
    double centroidY;
};

struct PolygonMetrics {
    double area;
    double centroidX;
    double centroidY;
};

PlanarPoint ToPlanar(const LonLatCoordinate& coordinate, double cosLat0) {
    const double lonRad = DegreesToRadians(coordinate.lon);
    const double latRad = DegreesToRadians(coordinate.lat);
    return {kEarthRadiusMeters * lonRad * cosLat0, kEarthRadiusMeters * latRad};
}

SignedAreaCentroid ComputeRingAreaCentroidPlanar(const std::vector<LonLatCoordinate>& ring, double cosLat0) {
    if (ring.size() < 4) {
        throw std::invalid_argument("Polygon ring must contain at least 4 coordinates including closure.");
    }

    double crossSum = 0.0;
    double centroidXAccumulator = 0.0;
    double centroidYAccumulator = 0.0;

    for (size_t i = 0; i + 1 < ring.size(); ++i) {
        const PlanarPoint p1 = ToPlanar(ring[i], cosLat0);
        const PlanarPoint p2 = ToPlanar(ring[i + 1], cosLat0);
        const double cross = (p1.x * p2.y) - (p2.x * p1.y);

        crossSum += cross;
        centroidXAccumulator += (p1.x + p2.x) * cross;
        centroidYAccumulator += (p1.y + p2.y) * cross;
    }

    const double signedArea = 0.5 * crossSum;
    if (std::abs(signedArea) < 1e-12) {
        throw std::invalid_argument("Polygon ring has zero area.");
    }

    const double centroidX = centroidXAccumulator / (6.0 * signedArea);
    const double centroidY = centroidYAccumulator / (6.0 * signedArea);
    return {signedArea, centroidX, centroidY};
}

PolygonMetrics ComputePolygonMetrics(const PolygonRingsWgs84& polygon) {
    if (polygon.outerRing.empty()) {
        throw std::invalid_argument("Polygon must contain an outer ring.");
    }

    double latSum = 0.0;
    size_t latCount = 0;
    for (const auto& point : polygon.outerRing) {
        latSum += point.lat;
        ++latCount;
    }

    const double lat0Rad = DegreesToRadians(latSum / static_cast<double>(latCount));
    const double cosLat0 = std::cos(lat0Rad);
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("Polygon is too close to poles for planar approximation.");
    }

    const SignedAreaCentroid outer = ComputeRingAreaCentroidPlanar(polygon.outerRing, cosLat0);
    double totalArea = std::abs(outer.signedArea);
    double weightedX = totalArea * outer.centroidX;
    double weightedY = totalArea * outer.centroidY;

    for (const auto& hole : polygon.holes) {
        const SignedAreaCentroid holeMetrics = ComputeRingAreaCentroidPlanar(hole, cosLat0);
        const double holeArea = std::abs(holeMetrics.signedArea);
        totalArea -= holeArea;
        weightedX -= holeArea * holeMetrics.centroidX;
        weightedY -= holeArea * holeMetrics.centroidY;
    }

    if (totalArea <= 0.0) {
        throw std::invalid_argument("Polygon area is zero after subtracting holes.");
    }

    return {
        totalArea,
        weightedX / totalArea,
        weightedY / totalArea
    };
}

ToolResult CalculateGeoJsonBbox(const json& arguments) {
    const auto& geojson = arguments.at("geojson");
    const auto coordinates = CollectWgs84Coordinates(geojson);

    BboxAccumulator bbox;
    for (const auto& coordinate : coordinates) {
        bbox.AddCoordinate(coordinate.lon, coordinate.lat);
    }

    std::ostringstream message;
    message << std::fixed << std::setprecision(6)
            << "GeoJSON bbox: [" << bbox.minLon << ", " << bbox.minLat
            << ", " << bbox.maxLon << ", " << bbox.maxLat << "]";

    return {
        message.str(),
        {
            {"coordinate_system", "WGS84"},
            {"method", "geojson_coordinate_extrema"},
            {"bbox", json::array({bbox.minLon, bbox.minLat, bbox.maxLon, bbox.maxLat})},
            {"min_lon", bbox.minLon},
            {"min_lat", bbox.minLat},
            {"max_lon", bbox.maxLon},
            {"max_lat", bbox.maxLat},
            {"points_count", bbox.pointsCount}
        }
    };
}

ToolResult CalculateGeoJsonCentroid(const json& arguments) {
    const auto& geojson = arguments.at("geojson");
    const auto polygons = CollectPolygonsWgs84(geojson);

    if (!polygons.empty()) {
        double totalArea = 0.0;
        double weightedCentroidX = 0.0;
        double weightedCentroidY = 0.0;

        for (const auto& polygon : polygons) {
            const PolygonMetrics metrics = ComputePolygonMetrics(polygon);
            totalArea += metrics.area;
            weightedCentroidX += metrics.centroidX * metrics.area;
            weightedCentroidY += metrics.centroidY * metrics.area;
        }

        if (totalArea <= 0.0) {
            throw std::invalid_argument("Cannot compute centroid from zero polygon area.");
        }

        double lat0Sum = 0.0;
        size_t lat0Count = 0;
        for (const auto& polygon : polygons) {
            for (const auto& point : polygon.outerRing) {
                lat0Sum += point.lat;
                ++lat0Count;
            }
        }

        const double lat0Rad = DegreesToRadians(lat0Sum / static_cast<double>(lat0Count));
        const double cosLat0 = std::cos(lat0Rad);
        if (std::abs(cosLat0) < 1e-12) {
            throw std::invalid_argument("Polygon centroid conversion failed near poles.");
        }

        const double centroidX = weightedCentroidX / totalArea;
        const double centroidY = weightedCentroidY / totalArea;
        const double centroidLon = RadiansToDegrees(centroidX / (kEarthRadiusMeters * cosLat0));
        const double centroidLat = RadiansToDegrees(centroidY / kEarthRadiusMeters);

        std::ostringstream message;
        message << std::fixed << std::setprecision(6)
                << "GeoJSON centroid: [" << centroidLon << ", " << centroidLat << "]";

        return {
            message.str(),
            {
                {"coordinate_system", "WGS84"},
                {"method", "polygon_area_weighted_centroid_planar"},
                {"centroid", json::array({centroidLon, centroidLat})},
                {"centroid_lon", centroidLon},
                {"centroid_lat", centroidLat},
                {"polygon_count", polygons.size()},
                {"area_weight_m2", totalArea}
            }
        };
    }

    const auto coordinates = CollectWgs84Coordinates(geojson);

    double sumLon = 0.0;
    double sumLat = 0.0;
    for (const auto& coordinate : coordinates) {
        sumLon += coordinate.lon;
        sumLat += coordinate.lat;
    }

    const double centroidLon = sumLon / static_cast<double>(coordinates.size());
    const double centroidLat = sumLat / static_cast<double>(coordinates.size());

    std::ostringstream message;
    message << std::fixed << std::setprecision(6)
            << "GeoJSON centroid: [" << centroidLon << ", " << centroidLat << "]";

    return {
        message.str(),
        {
            {"coordinate_system", "WGS84"},
            {"method", "mean_of_vertices"},
            {"centroid", json::array({centroidLon, centroidLat})},
            {"centroid_lon", centroidLon},
            {"centroid_lat", centroidLat},
            {"points_count", coordinates.size()}
        }
    };
}

ToolResult CalculateGeoJsonArea(const json& arguments) {
    const auto& geojson = arguments.at("geojson");
    const auto polygons = CollectPolygonsWgs84(geojson);
    if (polygons.empty()) {
        throw std::invalid_argument("No Polygon or MultiPolygon geometries found in geojson.");
    }

    double totalArea = 0.0;
    for (const auto& polygon : polygons) {
        totalArea += ComputePolygonMetrics(polygon).area;
    }

    const double totalAreaKm2 = totalArea / 1'000'000.0;
    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "GeoJSON area: " << totalArea << " m^2 ("
            << totalAreaKm2 << " km^2)";

    return {
        message.str(),
        {
            {"coordinate_system", "WGS84"},
            {"method", "polygon_area_planar_equirectangular"},
            {"area_square_meters", totalArea},
            {"area_square_kilometers", totalAreaKm2},
            {"polygon_count", polygons.size()}
        }
    };
}

} // namespace

void RegisterGeoJsonTools(ToolRegistry& registry) {
    registry.Register({
        {"calculate_geojson_bbox", "Calculate WGS84 bounding box for a GeoJSON Geometry, Feature, or FeatureCollection.", kGeoJsonBboxSchema},
        CalculateGeoJsonBbox
    });

    registry.Register({
        {"calculate_geojson_centroid", "Calculate centroid of GeoJSON; uses area-weighted centroid for Polygon/MultiPolygon and mean of vertices otherwise.", kGeoJsonCentroidSchema},
        CalculateGeoJsonCentroid
    });

    registry.Register({
        {"calculate_geojson_area", "Calculate area for Polygon/MultiPolygon geometries in a GeoJSON Geometry, Feature, or FeatureCollection.", kGeoJsonAreaSchema},
        CalculateGeoJsonArea
    });
}
