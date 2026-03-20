#pragma once

#include "json.hpp"

namespace TransformationGeometryOps {

nlohmann::json ClipFeatureToBbox(const nlohmann::json& feature, const nlohmann::json& bbox);

nlohmann::json PolygonUnion(const nlohmann::json& featureCollection);
nlohmann::json PolygonIntersect(const nlohmann::json& featureCollection);
nlohmann::json PolygonDifference(const nlohmann::json& featureCollection);
nlohmann::json PolygonDissolve(const nlohmann::json& featureCollection, const nlohmann::json& options);

nlohmann::json BuildVoronoi(const nlohmann::json& pointsFeatureCollection, const nlohmann::json& options);

} // namespace TransformationGeometryOps
