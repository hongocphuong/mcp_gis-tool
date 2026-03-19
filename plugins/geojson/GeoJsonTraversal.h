#pragma once

#include <vector>
#include "GisTypes.h"

#include "json.hpp"

using json = nlohmann::json;



std::vector<LonLatPoint> CollectCoordinates(const json& geojson);
std::vector<PolygonRings> CollectPolygons(const json& geojson);
