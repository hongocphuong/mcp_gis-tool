#pragma once

#include <string>
#include <vector>

#include "GisTypes.h"
#include "GeoUnits/GeoUnits.h"
#include "json.hpp"

namespace AggregationCommon {

void ValidatePolygonFeatureCollection(const nlohmann::json& polygons);

std::vector<PointFeatureData> ParsePointFeatures(const nlohmann::json& pointsCollection);

bool FeatureContainsPoint(const nlohmann::json& polygonFeature, const LonLatPoint& point);

// Convenience function: converts a distance value to kilometers
inline double ConvertDistanceToKilometers(double distance, const std::string& units) {
    return GeoUnits::FromMeters(GeoUnits::ToMeters(distance, units), "kilometers");
}

std::vector<std::vector<int>> BuildNeighborhoods(const std::vector<PointFeatureData>& points, double epsilonKm);

void EnsureFeatureProperties(nlohmann::json& feature);

void SetClusterLabels(nlohmann::json& featureCollection, const std::vector<int>& labels);

} // namespace AggregationCommon
