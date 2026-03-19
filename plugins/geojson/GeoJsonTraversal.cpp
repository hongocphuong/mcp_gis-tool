#include "geojson/GeoJsonTraversal.h"

#include <stdexcept>
#include <string>
#include <vector>
#include "GisTypes.h"

namespace {

bool IsValidCoordinate(double latitude, double longitude) {
    return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

LonLatPoint ParseCoordinate(const json& node) {
    if (!node.is_array() || node.size() < 2 || !node[0].is_number() || !node[1].is_number()) {
        throw std::invalid_argument("Coordinate must be [lon, lat] with numeric values.");
    }

    const double lon = node[0].get<double>();
    const double lat = node[1].get<double>();
    if (!IsValidCoordinate(lat, lon)) {
        throw std::invalid_argument("GeoJSON contains coordinate out of WGS84 range.");
    }

    return {lon, lat};
}

std::vector<LonLatPoint> ParseLinearRing(const json& ringNode) {
    if (!ringNode.is_array()) {
        throw std::invalid_argument("Polygon ring must be an array of coordinates.");
    }

    std::vector<LonLatPoint> ring;
    ring.reserve(ringNode.size() + 1);
    for (const auto& node : ringNode) {
        ring.push_back(ParseCoordinate(node));
    }

    if (ring.size() < 3) {
        throw std::invalid_argument("Polygon ring must contain at least 3 distinct points.");
    }

    const LonLatPoint& first = ring.front();
    const LonLatPoint& last  = ring.back();
    if (first.lon != last.lon || first.lat != last.lat) {
        ring.push_back(first);
    }

    return ring;
}

PolygonRings ParsePolygon(const json& polygonCoordinatesNode) {
    if (!polygonCoordinatesNode.is_array() || polygonCoordinatesNode.empty()) {
        throw std::invalid_argument("Polygon coordinates must be a non-empty array of rings.");
    }

    PolygonRings polygon;
    polygon.outerRing = ParseLinearRing(polygonCoordinatesNode[0]);

    for (size_t i = 1; i < polygonCoordinatesNode.size(); ++i) {
        polygon.holes.push_back(ParseLinearRing(polygonCoordinatesNode[i]));
    }

    return polygon;
}

void CollectCoordinatesAtDepth(const json& node, int depth, std::vector<LonLatPoint>& output) {
    if (depth == 0) {
        output.push_back(ParseCoordinate(node));
        return;
    }

    if (!node.is_array()) {
        throw std::invalid_argument("Invalid GeoJSON coordinates nesting level.");
    }

    for (const auto& child : node) {
        CollectCoordinatesAtDepth(child, depth - 1, output);
    }
}

int ExpectedCoordinateDepth(const std::string& geometryType) {
    if (geometryType == "Point")           return 0;
    if (geometryType == "MultiPoint")      return 1;
    if (geometryType == "LineString")      return 1;
    if (geometryType == "MultiLineString") return 2;
    if (geometryType == "Polygon")         return 2;
    if (geometryType == "MultiPolygon")    return 3;
    return -1;
}

// Forward declarations
void CollectFromGeometry(const json& geometry, std::vector<LonLatPoint>& output);
void CollectPolygonsFromGeometry(const json& geometry, std::vector<PolygonRings>& output);

void CollectFromFeature(const json& feature, std::vector<LonLatPoint>& output) {
    if (!feature.contains("geometry")) {
        throw std::invalid_argument("Feature must contain geometry.");
    }
    const auto& geometry = feature["geometry"];
    if (!geometry.is_null()) {
        CollectFromGeometry(geometry, output);
    }
}

void CollectPolygonsFromFeature(const json& feature, std::vector<PolygonRings>& output) {
    if (!feature.contains("geometry")) {
        throw std::invalid_argument("Feature must contain geometry.");
    }
    const auto& geometry = feature["geometry"];
    if (!geometry.is_null()) {
        CollectPolygonsFromGeometry(geometry, output);
    }
}

void CollectFromFeatureCollection(const json& collection, std::vector<LonLatPoint>& output) {
    if (!collection.contains("features") || !collection["features"].is_array()) {
        throw std::invalid_argument("FeatureCollection must contain a features array.");
    }
    for (const auto& feature : collection["features"]) {
        if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
            throw std::invalid_argument("FeatureCollection items must be Feature objects.");
        }
        CollectFromFeature(feature, output);
    }
}

void CollectPolygonsFromFeatureCollection(const json& collection, std::vector<PolygonRings>& output) {
    if (!collection.contains("features") || !collection["features"].is_array()) {
        throw std::invalid_argument("FeatureCollection must contain a features array.");
    }
    for (const auto& feature : collection["features"]) {
        if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
            throw std::invalid_argument("FeatureCollection items must be Feature objects.");
        }
        CollectPolygonsFromFeature(feature, output);
    }
}

void CollectFromGeometryCollection(const json& geometryCollection, std::vector<LonLatPoint>& output) {
    if (!geometryCollection.contains("geometries") || !geometryCollection["geometries"].is_array()) {
        throw std::invalid_argument("GeometryCollection must contain a geometries array.");
    }
    for (const auto& geometry : geometryCollection["geometries"]) {
        CollectFromGeometry(geometry, output);
    }
}

void CollectPolygonsFromGeometryCollection(const json& geometryCollection, std::vector<PolygonRings>& output) {
    if (!geometryCollection.contains("geometries") || !geometryCollection["geometries"].is_array()) {
        throw std::invalid_argument("GeometryCollection must contain a geometries array.");
    }
    for (const auto& geometry : geometryCollection["geometries"]) {
        CollectPolygonsFromGeometry(geometry, output);
    }
}

void CollectFromGeometry(const json& geometry, std::vector<LonLatPoint>& output) {
    if (!geometry.is_object() || !geometry.contains("type") || !geometry["type"].is_string()) {
        throw std::invalid_argument("GeoJSON geometry must have a string type.");
    }

    const std::string geometryType = geometry["type"].get<std::string>();

    if (geometryType == "GeometryCollection") {
        CollectFromGeometryCollection(geometry, output);
        return;
    }

    const int depth = ExpectedCoordinateDepth(geometryType);
    if (depth < 0) {
        throw std::invalid_argument("Unsupported GeoJSON geometry type: " + geometryType);
    }

    if (!geometry.contains("coordinates")) {
        throw std::invalid_argument("GeoJSON geometry must contain coordinates.");
    }

    CollectCoordinatesAtDepth(geometry["coordinates"], depth, output);
}

void CollectPolygonsFromGeometry(const json& geometry, std::vector<PolygonRings>& output) {
    if (!geometry.is_object() || !geometry.contains("type") || !geometry["type"].is_string()) {
        throw std::invalid_argument("GeoJSON geometry must have a string type.");
    }

    const std::string geometryType = geometry["type"].get<std::string>();

    if (geometryType == "GeometryCollection") {
        CollectPolygonsFromGeometryCollection(geometry, output);
        return;
    }

    if (geometryType == "Polygon") {
        if (!geometry.contains("coordinates")) {
            throw std::invalid_argument("GeoJSON polygon must contain coordinates.");
        }
        output.push_back(ParsePolygon(geometry["coordinates"]));
        return;
    }

    if (geometryType == "MultiPolygon") {
        if (!geometry.contains("coordinates") || !geometry["coordinates"].is_array()) {
            throw std::invalid_argument("GeoJSON multi-polygon must contain coordinates array.");
        }
        for (const auto& polygonNode : geometry["coordinates"]) {
            output.push_back(ParsePolygon(polygonNode));
        }
        return;
    }
}

} // namespace

std::vector<LonLatPoint> CollectCoordinates(const json& geojson) {
    if (!geojson.is_object() || !geojson.contains("type") || !geojson["type"].is_string()) {
        throw std::invalid_argument("GeoJSON input must be an object with a string type.");
    }

    const std::string type = geojson["type"].get<std::string>();
    std::vector<LonLatPoint> coordinates;

    if (type == "FeatureCollection") {
        CollectFromFeatureCollection(geojson, coordinates);
    } else if (type == "Feature") {
        CollectFromFeature(geojson, coordinates);
    } else {
        CollectFromGeometry(geojson, coordinates);
    }

    if (coordinates.empty()) {
        throw std::invalid_argument("No coordinates found in geojson.");
    }

    return coordinates;
}

std::vector<PolygonRings> CollectPolygons(const json& geojson) {
    if (!geojson.is_object() || !geojson.contains("type") || !geojson["type"].is_string()) {
        throw std::invalid_argument("GeoJSON input must be an object with a string type.");
    }

    const std::string type = geojson["type"].get<std::string>();
    std::vector<PolygonRings> polygons;

    if (type == "FeatureCollection") {
        CollectPolygonsFromFeatureCollection(geojson, polygons);
    } else if (type == "Feature") {
        CollectPolygonsFromFeature(geojson, polygons);
    } else {
        CollectPolygonsFromGeometry(geojson, polygons);
    }

    return polygons;
}
