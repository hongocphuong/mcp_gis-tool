#include "AggregationCommon.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "GeoAlgorithms/GeoAlgorithms.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"
#include "GeoMath/GeoMath.h"
#include "geojson/GeoJsonTraversal.h"

namespace AggregationCommon {

using json = nlohmann::json;
using GeoJsonParsingUtils::ParsePointLike;
void ValidatePolygonFeatureCollection(const json& polygons) {
    if (!polygons.contains("features") || !polygons["features"].is_array()) {
        throw std::invalid_argument("Field 'polygons.features' must be an array.");
    }

    for (const auto& feature : polygons["features"]) {
        if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
            throw std::invalid_argument("Field 'polygons.features' must contain GeoJSON Feature objects.");
        }

        if (!feature.contains("geometry") || feature["geometry"].is_null()) {
            throw std::invalid_argument("Each polygon feature must contain a non-null geometry.");
        }

        const auto& geometry = feature["geometry"];
        if (!geometry.is_object() || !geometry.contains("type") || !geometry["type"].is_string()) {
            throw std::invalid_argument("Each polygon feature geometry must contain a string type.");
        }

        const std::string type = geometry["type"].get<std::string>();
        if (type != "Polygon" && type != "MultiPolygon") {
            throw std::invalid_argument("Field 'polygons' must contain only Polygon or MultiPolygon features.");
        }
    }
}

std::vector<PointFeatureData> ParsePointFeatures(const json& pointsCollection) {
    if (!pointsCollection.contains("features") || !pointsCollection["features"].is_array()) {
        throw std::invalid_argument("Field 'points.features' must be an array.");
    }

    std::vector<PointFeatureData> pointFeatures;
    pointFeatures.reserve(pointsCollection["features"].size());

    for (const auto& feature : pointsCollection["features"]) {
        if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
            throw std::invalid_argument("Field 'points.features' must contain GeoJSON Feature objects.");
        }

        pointFeatures.push_back({ParsePointLike(feature), feature});
    }

    return pointFeatures;
}

bool FeatureContainsPoint(const json& polygonFeature, const LonLatPoint& point) {
    const auto polygons = GeoJsonTraversal::CollectPolygons(polygonFeature);
    for (const auto& polygon : polygons) {
        if (GeoAlgorithms::PointInPolygon(point.lon, point.lat, polygon)) {
            return true;
        }
    }
    return false;
}

std::vector<std::vector<int>> BuildNeighborhoods(const std::vector<PointFeatureData>& points, double epsilonKm) {
    const int n = static_cast<int>(points.size());
    if (n > 10000) {
        throw std::invalid_argument(
            "clustersDbscan supports at most 10000 points (got " +
            std::to_string(n) + "). Split the input into smaller batches.");
    }
    std::vector<std::vector<int>> neighbors(n);

    for (int i = 0; i < n; ++i) {
        neighbors[i].push_back(i);
    }

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const double meters = GeoMath::HaversineDistanceMeters(
                points[i].point.lat,
                points[i].point.lon,
                points[j].point.lat,
                points[j].point.lon
            );

            if ((meters / 1000.0) <= epsilonKm) {
                neighbors[i].push_back(j);
                neighbors[j].push_back(i);
            }
        }
    }

    return neighbors;
}

void EnsureFeatureProperties(json& feature) {
    if (!feature.contains("properties") || !feature["properties"].is_object()) {
        feature["properties"] = json::object();
    }
}

void SetClusterLabels(json& featureCollection, const std::vector<int>& labels) {
    if (!featureCollection.contains("features") || !featureCollection["features"].is_array()) {
        throw std::invalid_argument("FeatureCollection must contain a features array.");
    }
    if (featureCollection["features"].size() != labels.size()) {
        throw std::invalid_argument("labels size must match features size.");
    }

    for (size_t i = 0; i < labels.size(); ++i) {
        auto& feature = featureCollection["features"][i];
        EnsureFeatureProperties(feature);
        feature["properties"]["cluster"] = labels[i];
    }
}

} // namespace AggregationCommon
