#pragma once

#include "GisTypes.h"
#include "json.hpp"

namespace FeatureBuilders {

// Parse a bbox json node (array or JSON string) into a Bbox struct.
// Validates format, numeric types, and minX<=maxX / minY<=maxY.
Bbox ParseBbox(const nlohmann::json& bboxNode);

// Compute the bounding box of any GeoJSON object (Feature, FeatureCollection, Geometry).
Bbox ComputeBbox(const nlohmann::json& geojsonNode);

// Build a GeoJSON Point Feature from lon/lat coordinates.
// options may contain "properties" (object), "id", and "bbox".
nlohmann::json BuildPointFeature(double lon, double lat, const nlohmann::json& options = nlohmann::json::object());

// Build a GeoJSON Polygon Feature whose ring traces the given bounding box.
// options may contain "properties" (object) and "id".
nlohmann::json BboxToPolygonFeature(const Bbox& bbox, const nlohmann::json& options = nlohmann::json::object());

} // namespace FeatureBuilders
