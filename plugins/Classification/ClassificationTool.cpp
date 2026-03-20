#include "ClassificationTool.h"
#include "ClassificationSchemas.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "json.hpp"
#include "GeoMath/GeoMath.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"

namespace {

using json = nlohmann::json;
using GeoJsonParsingUtils::ParseJsonLikeArgument;
using GeoJsonParsingUtils::ParsePointLike;

ToolResult ClassificationNearestPoint(const json& arguments) {
    const LonLatPoint target = ParsePointLike(ParseJsonLikeArgument(arguments, "target_point"));

    const json pointsNode = ParseJsonLikeArgument(arguments, "points");
    if (!pointsNode.is_object() || !pointsNode.contains("type") || pointsNode["type"] != "FeatureCollection") {
        throw std::invalid_argument("Field 'points' must be a GeoJSON FeatureCollection.");
    }
    if (!pointsNode.contains("features") || !pointsNode["features"].is_array() || pointsNode["features"].empty()) {
        throw std::invalid_argument("Field 'points.features' must be a non-empty array.");
    }

    double minDistanceMeters = std::numeric_limits<double>::infinity();
    json nearestFeature;

    for (const auto& feature : pointsNode["features"]) {
        if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
            throw std::invalid_argument("Each item in 'points.features' must be a GeoJSON Feature.");
        }

        const LonLatPoint candidate = ParsePointLike(feature);

        const double distanceMeters = GeoMath::HaversineDistanceMeters(target.lat, target.lon, candidate.lat, candidate.lon);
        if (distanceMeters < minDistanceMeters) {
            minDistanceMeters = distanceMeters;
            nearestFeature = feature;
        }
    }

    if (nearestFeature.is_null()) {
        throw std::invalid_argument("No valid point feature found in 'points'.");
    }

    if (!nearestFeature.contains("properties") || !nearestFeature["properties"].is_object()) {
        nearestFeature["properties"] = json::object();
    }

    nearestFeature["properties"]["distance"] = minDistanceMeters;

    return {
        "Found nearest point to target point.",
        nearestFeature
    };
}

} // namespace

void RegisterClassificationTools(ToolRegistry& registry) {
    using namespace ClassificationSchemas;
    registry.Register({
        {
            kNearestPointName,
            kNearestPointDescription,
            kNearestPointSchema
        },
        ClassificationNearestPoint
    });
}
