#include "FeatureBuilders.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "geojson/GeoJsonTraversal.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"

namespace FeatureBuilders {

using json = nlohmann::json;
using GeoJsonTraversal::CollectCoordinates;

Bbox ParseBbox(const json& bboxNode) {
    json bbox = bboxNode;
    if (bbox.is_string()) {
        bbox = GeoJsonParsingUtils::ParseJsonString(bbox.get<std::string>(), "bbox");
    }

    if (!bbox.is_array() || bbox.size() != 4) {
        throw std::invalid_argument("bbox must be an array [minX, minY, maxX, maxY].");
    }
    for (int i = 0; i < 4; ++i) {
        if (!bbox[i].is_number()) {
            throw std::invalid_argument("bbox coordinates must be numbers.");
        }
    }

    const Bbox result{
        bbox[0].get<double>(),
        bbox[1].get<double>(),
        bbox[2].get<double>(),
        bbox[3].get<double>()
    };

    if (result.minX > result.maxX || result.minY > result.maxY) {
        throw std::invalid_argument("bbox must satisfy minX <= maxX and minY <= maxY.");
    }

    return result;
}

Bbox ComputeBbox(const json& geojsonNode) {
    const auto coords = CollectCoordinates(geojsonNode);
    if (coords.empty()) {
        throw std::invalid_argument("Cannot compute bbox: GeoJSON contains no coordinates.");
    }

    Bbox bbox{
        coords.front().lon,
        coords.front().lat,
        coords.front().lon,
        coords.front().lat
    };

    for (const auto& c : coords) {
        bbox.minX = std::min(bbox.minX, c.lon);
        bbox.minY = std::min(bbox.minY, c.lat);
        bbox.maxX = std::max(bbox.maxX, c.lon);
        bbox.maxY = std::max(bbox.maxY, c.lat);
    }

    return bbox;
}

json BuildPointFeature(double lon, double lat, const json& options) {
    json properties = json::object();
    if (options.contains("properties") && options["properties"].is_object()) {
        properties = options["properties"];
    }

    json feature = {
        {"type", "Feature"},
        {"properties", properties},
        {"geometry", {
            {"type", "Point"},
            {"coordinates", json::array({lon, lat})}
        }}
    };

    if (options.contains("id")) {
        feature["id"] = options["id"];
    }
    if (options.contains("bbox") && options["bbox"].is_array()) {
        feature["bbox"] = options["bbox"];
    }

    return feature;
}

json BboxToPolygonFeature(const Bbox& bbox, const json& options) {
    const json ring = json::array({
        json::array({bbox.minX, bbox.minY}),
        json::array({bbox.maxX, bbox.minY}),
        json::array({bbox.maxX, bbox.maxY}),
        json::array({bbox.minX, bbox.maxY}),
        json::array({bbox.minX, bbox.minY})
    });

    json feature = {
        {"type", "Feature"},
        {"properties", json::object()},
        {"geometry", {
            {"type", "Polygon"},
            {"coordinates", json::array({ring})}
        }}
    };

    if (options.contains("properties") && options["properties"].is_object()) {
        feature["properties"] = options["properties"];
    }
    if (options.contains("id")) {
        feature["id"] = options["id"];
    }

    return feature;
}

} // namespace FeatureBuilders
