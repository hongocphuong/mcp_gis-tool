#pragma once

#include <vector>
#include "GisTypes.h"
#include "json.hpp"

namespace GeoJsonTraversal {

std::vector<LonLatPoint> CollectCoordinates(const nlohmann::json& geojson);
std::vector<PolygonRings> CollectPolygons(const nlohmann::json& geojson);

} // namespace GeoJsonTraversal
