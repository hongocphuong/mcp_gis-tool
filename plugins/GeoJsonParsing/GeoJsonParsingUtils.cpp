#include "GeoJsonParsingUtils.h"

#include <stdexcept>

namespace GeoJsonParsingUtils {

namespace {

bool IsValidCoordinate(double latitude, double longitude) {
    return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

void ValidateCoordinate(double latitude, double longitude) {
    if (!IsValidCoordinate(latitude, longitude)) {
        throw std::invalid_argument("WGS84 coordinates out of range. Latitude must be [-90, 90], longitude must be [-180, 180].");
    }
}

} // namespace

nlohmann::json ParseJsonString(const std::string& value, const std::string& fieldName) {
    const nlohmann::json parsed = nlohmann::json::parse(value, nullptr, false);
    if (parsed.is_discarded()) {
        throw std::invalid_argument("Field '" + fieldName + "' must contain valid JSON text.");
    }
    return parsed;
}

nlohmann::json ParseJsonLikeArgument(const nlohmann::json& arguments, const std::string& fieldName) {
    if (!arguments.contains(fieldName)) {
        throw std::invalid_argument("Missing required field '" + fieldName + "'.");
    }

    const auto& value = arguments.at(fieldName);
    if (value.is_string()) {
        return ParseJsonString(value.get<std::string>(), fieldName);
    }

    return value;
}

nlohmann::json ParseOptionsArgument(const nlohmann::json& arguments) {
    if (!arguments.contains("options")) {
        return nlohmann::json::object();
    }

    const auto& optionsValue = arguments.at("options");
    nlohmann::json options = optionsValue;
    if (optionsValue.is_string()) {
        options = ParseJsonString(optionsValue.get<std::string>(), "options");
    }

    if (!options.is_object()) {
        throw std::invalid_argument("options must be a JSON object or JSON object string.");
    }

    return options;
}

LonLatPoint ParseCoordinateArray(const nlohmann::json& coordinates) {
    if (!coordinates.is_array() || coordinates.size() < 2 || !coordinates[0].is_number() || !coordinates[1].is_number()) {
        throw std::invalid_argument("Point coordinates must be [lon, lat].");
    }

    const double lon = coordinates[0].get<double>();
    const double lat = coordinates[1].get<double>();
    ValidateCoordinate(lat, lon);
    return {lon, lat};
}

LonLatPoint ParsePointLike(const nlohmann::json& pointNode) {
    if (pointNode.is_array()) {
        return ParseCoordinateArray(pointNode);
    }

    if (!pointNode.is_object() || !pointNode.contains("type") || !pointNode["type"].is_string()) {
        throw std::invalid_argument("Point input must be a GeoJSON Point geometry, Feature<Point>, or [lon, lat].");
    }

    const std::string type = pointNode["type"].get<std::string>();
    if (type == "Feature") {
        if (!pointNode.contains("geometry") || pointNode["geometry"].is_null()) {
            throw std::invalid_argument("Feature point input must include a non-null geometry.");
        }
        return ParsePointLike(pointNode["geometry"]);
    }

    if (type != "Point") {
        throw std::invalid_argument("GeoJSON point input must be type Point or Feature<Point>.");
    }

    if (!pointNode.contains("coordinates")) {
        throw std::invalid_argument("Point geometry must contain coordinates.");
    }

    return ParseCoordinateArray(pointNode["coordinates"]);
}

nlohmann::json ParsePointGeometryLike(const nlohmann::json& node, const std::string& fieldName) {
    if (!node.is_object() || !node.contains("type") || !node["type"].is_string()) {
        throw std::invalid_argument("Field '" + fieldName + "' must be a GeoJSON Point geometry or Feature<Point>.");
    }

    const std::string type = node["type"].get<std::string>();
    if (type == "Feature") {
        if (!node.contains("geometry") || node["geometry"].is_null()) {
            throw std::invalid_argument("Field '" + fieldName + "' feature must include non-null geometry.");
        }
        return ParsePointGeometryLike(node["geometry"], fieldName);
    }

    if (type != "Point") {
        throw std::invalid_argument("Field '" + fieldName + "' must be Point or Feature<Point>.");
    }

    if (!node.contains("coordinates") || !node["coordinates"].is_array() || node["coordinates"].size() < 2 ||
        !node["coordinates"][0].is_number() || !node["coordinates"][1].is_number()) {
        throw std::invalid_argument("Field '" + fieldName + "' must have coordinates [lon, lat].");
    }

    return node;
}

nlohmann::json ParseLineStringGeometryLike(const nlohmann::json& node, const std::string& fieldName) {
    if (!node.is_object() || !node.contains("type") || !node["type"].is_string()) {
        throw std::invalid_argument("Field '" + fieldName + "' must be a GeoJSON LineString geometry or Feature<LineString>.");
    }

    const std::string type = node["type"].get<std::string>();
    if (type == "Feature") {
        if (!node.contains("geometry") || node["geometry"].is_null()) {
            throw std::invalid_argument("Field '" + fieldName + "' feature must include non-null geometry.");
        }
        return ParseLineStringGeometryLike(node["geometry"], fieldName);
    }

    if (type != "LineString") {
        throw std::invalid_argument("Field '" + fieldName + "' must be LineString or Feature<LineString>.");
    }

    if (!node.contains("coordinates") || !node["coordinates"].is_array()) {
        throw std::invalid_argument("Field '" + fieldName + "' LineString must contain coordinates array.");
    }

    return node;
}

std::vector<LonLatPoint> ParseLineStringPoints(const nlohmann::json& lineNode, const std::string& fieldName) {
    const nlohmann::json geometry = ParseLineStringGeometryLike(lineNode, fieldName);
    const auto& coordinates = geometry["coordinates"];

    if (coordinates.size() < 2) {
        throw std::invalid_argument("Field '" + fieldName + "' LineString must contain at least 2 coordinates.");
    }

    std::vector<LonLatPoint> points;
    points.reserve(coordinates.size());
    for (const auto& coord : coordinates) {
        points.push_back(ParseCoordinateArray(coord));
    }

    return points;
}

std::vector<std::vector<LonLatPoint>> ParseLineOrMultiLine(const nlohmann::json& geojsonNode,
                                                           const std::string& fieldName) {
    nlohmann::json node = geojsonNode;
    if (!node.is_object() || !node.contains("type") || !node["type"].is_string()) {
        throw std::invalid_argument("Field '" + fieldName + "' must be a LineString/MultiLineString geometry or Feature.");
    }

    if (node["type"].get<std::string>() == "Feature") {
        if (!node.contains("geometry") || node["geometry"].is_null()) {
            throw std::invalid_argument("Field '" + fieldName + "' feature must contain non-null geometry.");
        }
        node = node["geometry"];
    }

    if (!node.contains("type") || !node["type"].is_string()) {
        throw std::invalid_argument("Field '" + fieldName + "' must be a valid geometry object.");
    }

    const std::string type = node["type"].get<std::string>();
    std::vector<std::vector<LonLatPoint>> lines;

    if (type == "LineString") {
        lines.push_back(ParseLineStringPoints(node, fieldName));
        return lines;
    }

    if (type == "MultiLineString") {
        if (!node.contains("coordinates") || !node["coordinates"].is_array()) {
            throw std::invalid_argument("Field '" + fieldName + "' MultiLineString must contain coordinates array.");
        }

        for (const auto& lineCoords : node["coordinates"]) {
            nlohmann::json line = {
                {"type", "LineString"},
                {"coordinates", lineCoords}
            };
            lines.push_back(ParseLineStringPoints(line, fieldName));
        }

        if (lines.empty()) {
            throw std::invalid_argument("Field '" + fieldName + "' MultiLineString must contain at least one line.");
        }

        return lines;
    }

    throw std::invalid_argument("Field '" + fieldName + "' must be LineString/MultiLineString geometry or Feature.");
}

nlohmann::json ParseFeatureCollectionLike(const nlohmann::json& node, const std::string& fieldName) {
    if (!node.is_object() || !node.contains("type") || !node["type"].is_string() ||
        node["type"].get<std::string>() != "FeatureCollection") {
        throw std::invalid_argument("Field '" + fieldName + "' must be a FeatureCollection.");
    }

    if (!node.contains("features") || !node["features"].is_array()) {
        throw std::invalid_argument("Field '" + fieldName + "' FeatureCollection must contain a features array.");
    }

    return node;
}

} // namespace GeoJsonParsingUtils
