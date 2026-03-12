#pragma once

#include <vector>

#include "json.hpp"

using json = nlohmann::json;

struct LonLatCoordinate {
    double lon;
    double lat;
};

struct PolygonRingsWgs84 {
    std::vector<LonLatCoordinate> outerRing;
    std::vector<std::vector<LonLatCoordinate>> holes;
};

std::vector<LonLatCoordinate> CollectWgs84Coordinates(const json& geojson);
std::vector<PolygonRingsWgs84> CollectPolygonsWgs84(const json& geojson);
