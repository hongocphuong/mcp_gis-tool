#include "TransformationTools.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TransformationSchemas.h"
#include "TransformationGeometryOps.h"
#include "GisTypes.h"
#include "GeoAlgorithms/GeoAlgorithms.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"
#include "GeoMath/GeoMath.h"
#include "GeoUnits/GeoUnits.h"
#include "geojson/GeoJsonTraversal.h"
#include "json.hpp"

namespace {

using json = nlohmann::json;
using GeoAlgorithms::MeanCentroid;
using GeoJsonParsingUtils::ParseFeatureCollectionLike;
using GeoJsonTraversal::CollectCoordinates;
using GeoJsonParsingUtils::ParseJsonLikeArgument;
using GeoJsonParsingUtils::ParseLineOrMultiLine;
using GeoJsonParsingUtils::ParseLineStringPoints;
using GeoJsonParsingUtils::ParseOptionsArgument;
using GeoJsonParsingUtils::ParsePointLike;
using GeoMath::DegreesToRadians;
using GeoMath::DestinationPoint;
using GeoMath::HaversineDistanceMeters;
using GeoMath::kEarthRadiusMeters;
using GeoMath::NormalizeLon180;
using GeoMath::RadiansToDegrees;

bool IsCoordinateArray(const json& node) {
    return node.is_array() && node.size() >= 2 && node[0].is_number() && node[1].is_number();
}

void TransformCoordinateArrays(json& node, const std::function<void(json&)>& transform) {
    if (IsCoordinateArray(node)) {
        transform(node);
        return;
    }

    if (!node.is_array()) {
        return;
    }

    for (auto& child : node) {
        TransformCoordinateArrays(child, transform);
    }
}

void ApplyTransformToGeometry(json& geometry, const std::function<void(json&)>& transform);

void ApplyTransformToGeoJson(json& geojson, const std::function<void(json&)>& transform) {
    if (!geojson.is_object() || !geojson.contains("type") || !geojson["type"].is_string()) {
        throw std::invalid_argument("GeoJSON input must be an object with a string type.");
    }

    const std::string type = geojson["type"].get<std::string>();
    if (type == "Feature") {
        if (!geojson.contains("geometry") || geojson["geometry"].is_null()) {
            throw std::invalid_argument("Feature must contain a non-null geometry.");
        }
        ApplyTransformToGeometry(geojson["geometry"], transform);
        return;
    }

    if (type == "FeatureCollection") {
        if (!geojson.contains("features") || !geojson["features"].is_array()) {
            throw std::invalid_argument("FeatureCollection must contain a features array.");
        }
        for (auto& feature : geojson["features"]) {
            if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
                throw std::invalid_argument("FeatureCollection items must be Feature objects.");
            }
            if (!feature.contains("geometry") || feature["geometry"].is_null()) {
                continue;
            }
            ApplyTransformToGeometry(feature["geometry"], transform);
        }
        return;
    }

    ApplyTransformToGeometry(geojson, transform);
}

void ApplyTransformToGeometry(json& geometry, const std::function<void(json&)>& transform) {
    if (!geometry.is_object() || !geometry.contains("type") || !geometry["type"].is_string()) {
        throw std::invalid_argument("Geometry must be a valid GeoJSON geometry object.");
    }

    const std::string type = geometry["type"].get<std::string>();
    if (type == "GeometryCollection") {
        if (!geometry.contains("geometries") || !geometry["geometries"].is_array()) {
            throw std::invalid_argument("GeometryCollection must contain geometries array.");
        }
        for (auto& child : geometry["geometries"]) {
            ApplyTransformToGeometry(child, transform);
        }
        return;
    }

    if (!geometry.contains("coordinates")) {
        throw std::invalid_argument("Geometry must contain coordinates.");
    }

    TransformCoordinateArrays(geometry["coordinates"], transform);
}

int ReadStepsOption(const json& options, int defaultValue = 64) {
    if (!options.contains("steps")) {
        return defaultValue;
    }
    if (!options["steps"].is_number_integer()) {
        throw std::invalid_argument("options.steps must be an integer.");
    }

    const int steps = options["steps"].get<int>();
    if (steps < 4) {
        throw std::invalid_argument("options.steps must be >= 4.");
    }

    return steps;
}

LonLatPoint ResolveOrigin(const json& options, const std::string& fieldName, const json& geojsonNode) {
    if (!options.contains(fieldName)) {
        return MeanCentroid(CollectCoordinates(geojsonNode));
    }

    const auto& originNode = options[fieldName];
    if (originNode.is_string()) {
        const std::string mode = originNode.get<std::string>();
        if (mode == "centroid") {
            return MeanCentroid(CollectCoordinates(geojsonNode));
        }
        throw std::invalid_argument("options." + fieldName + " supports only 'centroid' or a GeoJSON point input.");
    }

    return ParsePointLike(originNode);
}

json BuildCircleFeature(const LonLatPoint& center,
                        double radiusMeters,
                        int steps,
                        const json& properties = json::object()) {
    json ring = json::array();

    for (int i = 0; i < steps; ++i) {
        const double bearing = (static_cast<double>(i) * 360.0) / static_cast<double>(steps);
        const LonLatPoint point = DestinationPoint(center.lat, center.lon, bearing, radiusMeters);
        ring.push_back(json::array({point.lon, point.lat}));
    }

    if (!ring.empty()) {
        ring.push_back(ring.front());
    }

    return {
        {"type", "Feature"},
        {"properties", properties},
        {"geometry", {
            {"type", "Polygon"},
            {"coordinates", json::array({ring})}
        }}
    };
}

json ParseBboxArray(const json& node) {
    if (!node.is_array() || node.size() != 4) {
        throw std::invalid_argument("Field 'bbox' must be [minX, minY, maxX, maxY].");
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!node[i].is_number()) {
            throw std::invalid_argument("Field 'bbox' coordinates must be numeric.");
        }
    }

    if (node[0].get<double>() > node[2].get<double>() || node[1].get<double>() > node[3].get<double>()) {
        throw std::invalid_argument("Field 'bbox' must satisfy minX <= maxX and minY <= maxY.");
    }

    return node;
}

void ValidateBboxClipGeometryType(const std::string& geometryType) {
    if (geometryType != "LineString" && geometryType != "MultiLineString" &&
        geometryType != "Polygon" && geometryType != "MultiPolygon") {
        throw std::invalid_argument("bboxClip supports LineString, MultiLineString, Polygon, and MultiPolygon.");
    }
}

json ParseFeatureCollectionArgument(const json& arguments, const std::string& fieldName) {
    const json node = ParseJsonLikeArgument(arguments, fieldName);
    return ParseFeatureCollectionLike(node, fieldName);
}

std::string TrimLowerAscii(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    std::string out = value.substr(begin, end - begin);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<LonLatPoint> ChaikinSmooth(const std::vector<LonLatPoint>& points, int iterations) {
    std::vector<LonLatPoint> current = points;

    for (int iter = 0; iter < iterations; ++iter) {
        if (current.size() < 2) {
            break;
        }

        std::vector<LonLatPoint> next;
        next.reserve(current.size() * 2);
        next.push_back(current.front());

        for (size_t i = 0; i + 1 < current.size(); ++i) {
            const LonLatPoint& p = current[i];
            const LonLatPoint& q = current[i + 1];

            next.push_back({0.75 * p.lon + 0.25 * q.lon, 0.75 * p.lat + 0.25 * q.lat});
            next.push_back({0.25 * p.lon + 0.75 * q.lon, 0.25 * p.lat + 0.75 * q.lat});
        }

        next.push_back(current.back());
        current = std::move(next);
    }

    return current;
}

std::vector<LonLatPoint> Downsample(const std::vector<LonLatPoint>& points, size_t maxPoints) {
    if (maxPoints < 2 || points.size() <= maxPoints) {
        return points;
    }

    std::vector<LonLatPoint> sampled;
    sampled.reserve(maxPoints);
    sampled.push_back(points.front());

    const double stride = static_cast<double>(points.size() - 1) / static_cast<double>(maxPoints - 1);
    for (size_t i = 1; i + 1 < maxPoints; ++i) {
        const size_t index = static_cast<size_t>(std::round(stride * static_cast<double>(i)));
        sampled.push_back(points[std::min(index, points.size() - 1)]);
    }

    sampled.push_back(points.back());
    return sampled;
}

double Cross2D(const LonLatPoint& a, const LonLatPoint& b, const LonLatPoint& c) {
    return (b.lon - a.lon) * (c.lat - a.lat) - (b.lat - a.lat) * (c.lon - a.lon);
}

json BuildConvexHullPolygon(const json& featureCollection) {
    const json pointsCollection = ParseFeatureCollectionLike(featureCollection, "points");

    std::vector<LonLatPoint> points;
    points.reserve(pointsCollection["features"].size());

    for (const auto& feature : pointsCollection["features"]) {
        points.push_back(ParsePointLike(feature));
    }

    if (points.size() < 3) {
        throw std::invalid_argument("Field 'points.features' must contain at least 3 point features.");
    }

    std::sort(points.begin(), points.end(), [](const LonLatPoint& left, const LonLatPoint& right) {
        if (left.lon != right.lon) {
            return left.lon < right.lon;
        }
        return left.lat < right.lat;
    });

    points.erase(std::unique(points.begin(), points.end(), [](const LonLatPoint& l, const LonLatPoint& r) {
        return std::abs(l.lon - r.lon) < 1e-12 && std::abs(l.lat - r.lat) < 1e-12;
    }), points.end());

    if (points.size() < 3) {
        throw std::invalid_argument("Convex hull requires at least 3 distinct points.");
    }

    std::vector<LonLatPoint> hull;
    hull.reserve(points.size() * 2);

    for (const auto& point : points) {
        while (hull.size() >= 2 && Cross2D(hull[hull.size() - 2], hull.back(), point) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(point);
    }

    const size_t lowerSize = hull.size();
    for (size_t i = points.size() - 1; i > 0; --i) {
        const auto& point = points[i - 1];
        while (hull.size() > lowerSize && Cross2D(hull[hull.size() - 2], hull.back(), point) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(point);
    }

    if (!hull.empty()) {
        hull.pop_back();
    }

    if (hull.size() < 3) {
        throw std::invalid_argument("Convex hull computation failed to produce a polygon.");
    }

    json ring = json::array();
    for (const auto& p : hull) {
        ring.push_back(json::array({p.lon, p.lat}));
    }
    ring.push_back(ring.front());

    return {
        {"type", "Feature"},
        {"properties", json::object()},
        {"geometry", {
            {"type", "Polygon"},
            {"coordinates", json::array({ring})}
        }}
    };
}

double PerpendicularDistance(const LonLatPoint& p, const LonLatPoint& a, const LonLatPoint& b) {
    const double dx = b.lon - a.lon;
    const double dy = b.lat - a.lat;

    if (std::abs(dx) < 1e-15 && std::abs(dy) < 1e-15) {
        const double ex = p.lon - a.lon;
        const double ey = p.lat - a.lat;
        return std::sqrt(ex * ex + ey * ey);
    }

    const double t = std::clamp(((p.lon - a.lon) * dx + (p.lat - a.lat) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    const double px = a.lon + t * dx;
    const double py = a.lat + t * dy;
    const double ex = p.lon - px;
    const double ey = p.lat - py;
    return std::sqrt(ex * ex + ey * ey);
}

void SimplifyRdpRecursive(const std::vector<LonLatPoint>& points,
                          size_t start,
                          size_t end,
                          double tolerance,
                          std::vector<bool>& keep) {
    if (end <= start + 1) {
        return;
    }

    double maxDistance = -1.0;
    size_t index = start;

    for (size_t i = start + 1; i < end; ++i) {
        const double distance = PerpendicularDistance(points[i], points[start], points[end]);
        if (distance > maxDistance) {
            maxDistance = distance;
            index = i;
        }
    }

    if (maxDistance > tolerance) {
        keep[index] = true;
        SimplifyRdpRecursive(points, start, index, tolerance, keep);
        SimplifyRdpRecursive(points, index, end, tolerance, keep);
    }
}

json SimplifyCoordinateList(const json& coords, double tolerance, bool closedRing) {
    if (!coords.is_array() || coords.empty()) {
        return coords;
    }

    std::vector<json> coordNodes;
    coordNodes.reserve(coords.size());
    for (const auto& node : coords) {
        coordNodes.push_back(node);
    }

    if (closedRing && coordNodes.size() >= 2) {
        const auto& first = coordNodes.front();
        const auto& last = coordNodes.back();
        if (IsCoordinateArray(first) && IsCoordinateArray(last) &&
            std::abs(first[0].get<double>() - last[0].get<double>()) < 1e-12 &&
            std::abs(first[1].get<double>() - last[1].get<double>()) < 1e-12) {
            coordNodes.pop_back();
        }
    }

    if (coordNodes.size() < 3) {
        return coords;
    }

    std::vector<LonLatPoint> points;
    points.reserve(coordNodes.size());
    for (const auto& node : coordNodes) {
        if (!IsCoordinateArray(node)) {
            return coords;
        }
        points.push_back({node[0].get<double>(), node[1].get<double>()});
    }

    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;
    SimplifyRdpRecursive(points, 0, points.size() - 1, tolerance, keep);

    json simplified = json::array();
    for (size_t i = 0; i < coordNodes.size(); ++i) {
        if (keep[i]) {
            simplified.push_back(coordNodes[i]);
        }
    }

    if (closedRing) {
        if (simplified.size() < 3) {
            simplified = coords;
        }
        if (!simplified.empty()) {
            simplified.push_back(simplified.front());
        }
        if (simplified.size() < 4) {
            return coords;
        }
    }

    return simplified;
}

void SimplifyGeometry(json& geometry, double tolerance) {
    if (!geometry.is_object() || !geometry.contains("type") || !geometry["type"].is_string()) {
        throw std::invalid_argument("Invalid geometry for simplify.");
    }

    const std::string type = geometry["type"].get<std::string>();
    if (type == "GeometryCollection") {
        if (!geometry.contains("geometries") || !geometry["geometries"].is_array()) {
            throw std::invalid_argument("GeometryCollection must contain geometries array.");
        }
        for (auto& child : geometry["geometries"]) {
            SimplifyGeometry(child, tolerance);
        }
        return;
    }

    if (!geometry.contains("coordinates") || !geometry["coordinates"].is_array()) {
        return;
    }

    if (type == "LineString") {
        geometry["coordinates"] = SimplifyCoordinateList(geometry["coordinates"], tolerance, false);
        return;
    }

    if (type == "MultiLineString") {
        for (auto& line : geometry["coordinates"]) {
            line = SimplifyCoordinateList(line, tolerance, false);
        }
        return;
    }

    if (type == "Polygon") {
        for (auto& ring : geometry["coordinates"]) {
            ring = SimplifyCoordinateList(ring, tolerance, true);
        }
        return;
    }

    if (type == "MultiPolygon") {
        for (auto& polygon : geometry["coordinates"]) {
            for (auto& ring : polygon) {
                ring = SimplifyCoordinateList(ring, tolerance, true);
            }
        }
    }
}

json BuildOffsetLineCoordinates(const std::vector<LonLatPoint>& line, double offsetMeters) {
    if (line.size() < 2) {
        throw std::invalid_argument("lineOffset requires a line with at least 2 points.");
    }

    double meanLat = 0.0;
    double meanLon = 0.0;
    for (const auto& point : line) {
        meanLat += point.lat;
        meanLon += point.lon;
    }

    meanLat /= static_cast<double>(line.size());
    meanLon /= static_cast<double>(line.size());

    const double cosLat0 = std::cos(DegreesToRadians(meanLat));
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("lineOffset is unstable near the poles.");
    }

    std::vector<PlanarPoint> planar;
    planar.reserve(line.size());
    for (const auto& point : line) {
        planar.push_back({
            kEarthRadiusMeters * DegreesToRadians(point.lon - meanLon) * cosLat0,
            kEarthRadiusMeters * DegreesToRadians(point.lat - meanLat)
        });
    }

    struct Vec2 {
        double x;
        double y;
    };

    std::vector<Vec2> segmentNormals;
    segmentNormals.reserve(planar.size() - 1);

    for (size_t i = 0; i + 1 < planar.size(); ++i) {
        const double dx = planar[i + 1].x - planar[i].x;
        const double dy = planar[i + 1].y - planar[i].y;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-12) {
            segmentNormals.push_back({0.0, 0.0});
        } else {
            segmentNormals.push_back({-dy / len, dx / len});
        }
    }

    json out = json::array();
    for (size_t i = 0; i < planar.size(); ++i) {
        Vec2 normal{0.0, 0.0};

        if (i == 0) {
            normal = segmentNormals.front();
        } else if (i + 1 == planar.size()) {
            normal = segmentNormals.back();
        } else {
            normal.x = segmentNormals[i - 1].x + segmentNormals[i].x;
            normal.y = segmentNormals[i - 1].y + segmentNormals[i].y;
            const double nlen = std::sqrt(normal.x * normal.x + normal.y * normal.y);
            if (nlen > 1e-12) {
                normal.x /= nlen;
                normal.y /= nlen;
            } else {
                normal = segmentNormals[i];
            }
        }

        const double ox = planar[i].x + normal.x * offsetMeters;
        const double oy = planar[i].y + normal.y * offsetMeters;

        const double lon = NormalizeLon180(meanLon + RadiansToDegrees(ox / (kEarthRadiusMeters * cosLat0)));
        const double lat = meanLat + RadiansToDegrees(oy / kEarthRadiusMeters);
        out.push_back(json::array({lon, lat}));
    }

    return out;
}

ToolResult TransformationBboxClip(const json& arguments) {
    json feature = ParseJsonLikeArgument(arguments, "feature");
    const json bbox = ParseBboxArray(ParseJsonLikeArgument(arguments, "bbox"));

    if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
        throw std::invalid_argument("Field 'feature' must be a GeoJSON Feature.");
    }
    if (!feature.contains("geometry") || feature["geometry"].is_null() ||
        !feature["geometry"].contains("type") || !feature["geometry"]["type"].is_string()) {
        throw std::invalid_argument("Field 'feature.geometry' must have a valid type.");
    }

    ValidateBboxClipGeometryType(feature["geometry"]["type"].get<std::string>());

    feature = TransformationGeometryOps::ClipFeatureToBbox(feature, bbox);

    return {
        "Clipped feature to bounding box.",
        feature
    };
}

ToolResult TransformationBezierSpline(const json& arguments) {
    const json lineNode = ParseJsonLikeArgument(arguments, "line");
    const json options = ParseOptionsArgument(arguments);

    const auto linePoints = ParseLineStringPoints(lineNode, "line");
    double sharpness = 0.85;
    if (options.contains("sharpness")) {
        if (!options["sharpness"].is_number()) {
            throw std::invalid_argument("options.sharpness must be a number.");
        }
        sharpness = std::clamp(options["sharpness"].get<double>(), 0.1, 1.0);
    }

    int resolution = 10000;
    if (options.contains("resolution")) {
        if (!options["resolution"].is_number_integer()) {
            throw std::invalid_argument("options.resolution must be an integer.");
        }
        resolution = std::max(2, options["resolution"].get<int>());
    }

    const int iterations = std::clamp(static_cast<int>(std::round(sharpness * 4.0)), 1, 6);
    auto smoothed = ChaikinSmooth(linePoints, iterations);
    smoothed = Downsample(smoothed, static_cast<size_t>(resolution));

    json coordinates = json::array();
    for (const auto& point : smoothed) {
        coordinates.push_back(json::array({point.lon, point.lat}));
    }

    json out = {
        {"type", "Feature"},
        {"properties", json::object()},
        {"geometry", {
            {"type", "LineString"},
            {"coordinates", coordinates}
        }}
    };

    if (lineNode.is_object() && lineNode.contains("type") && lineNode["type"] == "Feature") {
        if (lineNode.contains("properties") && lineNode["properties"].is_object()) {
            out["properties"] = lineNode["properties"];
        }
    }

    return {
        "Generated smooth Bezier-like spline.",
        out
    };
}

ToolResult TransformationBuffer(const json& arguments) {
    const json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    if (!arguments.contains("radius") || !arguments["radius"].is_number()) {
        throw std::invalid_argument("Field 'radius' must be a number.");
    }

    const json options = ParseOptionsArgument(arguments);
    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const int steps = ReadStepsOption(options, 64);
    const double radius = arguments["radius"].get<double>();
    if (radius < 0.0) {
        throw std::invalid_argument("Field 'radius' must be >= 0.");
    }

    const auto coordinates = CollectCoordinates(geojsonNode);
    const LonLatPoint center = MeanCentroid(coordinates);

    double maxDistance = 0.0;
    for (const auto& point : coordinates) {
        maxDistance = std::max(maxDistance, HaversineDistanceMeters(center.lat, center.lon, point.lat, point.lon));
    }

    const double totalRadiusMeters = maxDistance + GeoUnits::ToMeters(radius, units);
    const json properties = options.contains("properties") && options["properties"].is_object()
        ? options["properties"]
        : json::object();

    return {
        "Created approximate buffer polygon.",
        BuildCircleFeature(center, totalRadiusMeters, steps, properties)
    };
}

ToolResult TransformationCircle(const json& arguments) {
    const LonLatPoint center = ParsePointLike(ParseJsonLikeArgument(arguments, "center"));
    if (!arguments.contains("radius") || !arguments["radius"].is_number()) {
        throw std::invalid_argument("Field 'radius' must be a number.");
    }

    const json options = ParseOptionsArgument(arguments);
    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const int steps = ReadStepsOption(options, 64);

    const double radius = arguments["radius"].get<double>();
    if (radius < 0.0) {
        throw std::invalid_argument("Field 'radius' must be >= 0.");
    }

    const json properties = options.contains("properties") && options["properties"].is_object()
        ? options["properties"]
        : json::object();

    const double radiusMeters = GeoUnits::ToMeters(radius, units);
    json result = BuildCircleFeature(center, radiusMeters, steps, properties);
    if (options.contains("id")) {
        result["id"] = options["id"];
    }

    return {
        "Created circle polygon feature.",
        result
    };
}

ToolResult TransformationClone(const json& arguments) {
    const json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    return {
        "Cloned GeoJSON object.",
        geojsonNode
    };
}

ToolResult TransformationConcave(const json& arguments) {
    const json pointsNode = ParseJsonLikeArgument(arguments, "points");
    (void)ParseOptionsArgument(arguments);

    return {
        "Calculated concave hull using convex-hull fallback.",
        BuildConvexHullPolygon(pointsNode)
    };
}

ToolResult TransformationConvex(const json& arguments) {
    const json pointsNode = ParseJsonLikeArgument(arguments, "points");
    return {
        "Calculated convex hull.",
        BuildConvexHullPolygon(pointsNode)
    };
}

ToolResult TransformationDifference(const json& arguments) {
    const json featureCollection = ParseFeatureCollectionArgument(arguments, "featureCollection");
    const json output = TransformationGeometryOps::PolygonDifference(featureCollection);
    return {
        "Calculated polygon difference (convex approximation).",
        output
    };
}

ToolResult TransformationDissolve(const json& arguments) {
    const json featureCollection = ParseFeatureCollectionArgument(arguments, "featureCollection");
    const json options = ParseOptionsArgument(arguments);
    const json output = TransformationGeometryOps::PolygonDissolve(featureCollection, options);
    return {
        "Dissolved polygon feature collection (convex approximation).",
        output
    };
}

ToolResult TransformationIntersect(const json& arguments) {
    const json featureCollection = ParseFeatureCollectionArgument(arguments, "featureCollection");
    const json output = TransformationGeometryOps::PolygonIntersect(featureCollection);
    if (output.is_null()) {
        return {
            "Intersection is empty.",
            nullptr
        };
    }

    return {
        "Calculated polygon intersection (convex approximation).",
        output
    };
}

ToolResult TransformationLineOffset(const json& arguments) {
    const json lineNode = ParseJsonLikeArgument(arguments, "line");
    if (!arguments.contains("distance") || !arguments["distance"].is_number()) {
        throw std::invalid_argument("Field 'distance' must be a number.");
    }

    const json options = ParseOptionsArgument(arguments);
    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double offsetMeters = GeoUnits::ToMeters(arguments["distance"].get<double>(), units);

    const auto lines = ParseLineOrMultiLine(lineNode, "line");

    json geometry;
    if (lines.size() == 1) {
        geometry = {
            {"type", "LineString"},
            {"coordinates", BuildOffsetLineCoordinates(lines.front(), offsetMeters)}
        };
    } else {
        json multiCoordinates = json::array();
        for (const auto& line : lines) {
            multiCoordinates.push_back(BuildOffsetLineCoordinates(line, offsetMeters));
        }
        geometry = {
            {"type", "MultiLineString"},
            {"coordinates", multiCoordinates}
        };
    }

    return {
        "Calculated offset line.",
        {
            {"type", "Feature"},
            {"properties", json::object()},
            {"geometry", geometry}
        }
    };
}

ToolResult TransformationSimplify(const json& arguments) {
    json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const json options = ParseOptionsArgument(arguments);

    double tolerance = 1.0;
    if (options.contains("tolerance")) {
        if (!options["tolerance"].is_number()) {
            throw std::invalid_argument("options.tolerance must be a number.");
        }
        tolerance = std::max(0.0, options["tolerance"].get<double>());
    }

    if (geojsonNode["type"] == "Feature") {
        if (!geojsonNode.contains("geometry") || geojsonNode["geometry"].is_null()) {
            throw std::invalid_argument("Feature must contain non-null geometry.");
        }
        SimplifyGeometry(geojsonNode["geometry"], tolerance);
    } else if (geojsonNode["type"] == "FeatureCollection") {
        if (!geojsonNode.contains("features") || !geojsonNode["features"].is_array()) {
            throw std::invalid_argument("FeatureCollection must contain features array.");
        }
        for (auto& feature : geojsonNode["features"]) {
            if (!feature.contains("geometry") || feature["geometry"].is_null()) {
                continue;
            }
            SimplifyGeometry(feature["geometry"], tolerance);
        }
    } else {
        SimplifyGeometry(geojsonNode, tolerance);
    }

    return {
        "Simplified GeoJSON geometry.",
        geojsonNode
    };
}

ToolResult TransformationTesselate(const json& arguments) {
    const json polygonNode = ParseJsonLikeArgument(arguments, "polygon");

    json geometry = polygonNode;
    json properties = json::object();
    if (polygonNode.is_object() && polygonNode.contains("type") && polygonNode["type"] == "Feature") {
        if (!polygonNode.contains("geometry") || polygonNode["geometry"].is_null()) {
            throw std::invalid_argument("Field 'polygon' must contain non-null geometry.");
        }
        geometry = polygonNode["geometry"];
        if (polygonNode.contains("properties") && polygonNode["properties"].is_object()) {
            properties = polygonNode["properties"];
        }
    }

    if (!geometry.is_object() || !geometry.contains("type") || geometry["type"] != "Polygon") {
        throw std::invalid_argument("Field 'polygon' must be Polygon or Feature<Polygon>.");
    }

    if (!geometry.contains("coordinates") || !geometry["coordinates"].is_array() || geometry["coordinates"].empty()) {
        throw std::invalid_argument("Polygon must contain coordinates with an outer ring.");
    }

    const json& outer = geometry["coordinates"][0];
    if (!outer.is_array() || outer.size() < 4) {
        throw std::invalid_argument("Polygon outer ring must contain at least 4 coordinates.");
    }

    size_t vertexCount = outer.size();
    if (IsCoordinateArray(outer.front()) && IsCoordinateArray(outer.back()) &&
        std::abs(outer.front()[0].get<double>() - outer.back()[0].get<double>()) < 1e-12 &&
        std::abs(outer.front()[1].get<double>() - outer.back()[1].get<double>()) < 1e-12) {
        vertexCount -= 1;
    }

    if (vertexCount < 3) {
        throw std::invalid_argument("Polygon must contain at least 3 distinct vertices.");
    }

    json triangles = json::array();
    for (size_t i = 1; i + 1 < vertexCount; ++i) {
        json ring = json::array({
            outer[0],
            outer[i],
            outer[i + 1],
            outer[0]
        });

        triangles.push_back({
            {"type", "Feature"},
            {"properties", properties},
            {"geometry", {
                {"type", "Polygon"},
                {"coordinates", json::array({ring})}
            }}
        });
    }

    return {
        "Tessellated polygon into triangles.",
        {
            {"type", "FeatureCollection"},
            {"features", triangles}
        }
    };
}

ToolResult TransformationTransformRotate(const json& arguments) {
    json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    if (!arguments.contains("angle") || !arguments["angle"].is_number()) {
        throw std::invalid_argument("Field 'angle' must be a number.");
    }

    const double angleDegrees = arguments["angle"].get<double>();
    const json options = ParseOptionsArgument(arguments);
    const LonLatPoint pivot = ResolveOrigin(options, "pivot", geojsonNode);

    const double theta = -DegreesToRadians(angleDegrees);
    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    const double cosLat0 = std::cos(DegreesToRadians(pivot.lat));
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("transformRotate is unstable near the poles.");
    }

    ApplyTransformToGeoJson(geojsonNode, [pivot, cosLat0, cosTheta, sinTheta](json& coordinate) {
        const double x = kEarthRadiusMeters * DegreesToRadians(coordinate[0].get<double>() - pivot.lon) * cosLat0;
        const double y = kEarthRadiusMeters * DegreesToRadians(coordinate[1].get<double>() - pivot.lat);

        const double xr = (x * cosTheta) - (y * sinTheta);
        const double yr = (x * sinTheta) + (y * cosTheta);

        coordinate[0] = NormalizeLon180(pivot.lon + RadiansToDegrees(xr / (kEarthRadiusMeters * cosLat0)));
        coordinate[1] = pivot.lat + RadiansToDegrees(yr / kEarthRadiusMeters);
    });

    return {
        "Rotated GeoJSON object.",
        geojsonNode
    };
}

ToolResult TransformationTransformTranslate(const json& arguments) {
    json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    if (!arguments.contains("distance") || !arguments["distance"].is_number()) {
        throw std::invalid_argument("Field 'distance' must be a number.");
    }
    if (!arguments.contains("direction") || !arguments["direction"].is_number()) {
        throw std::invalid_argument("Field 'direction' must be a number.");
    }

    const json options = ParseOptionsArgument(arguments);
    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double distanceMeters = GeoUnits::ToMeters(arguments["distance"].get<double>(), units);
    const double direction = arguments["direction"].get<double>();

    double zTranslation = 0.0;
    if (options.contains("zTranslation")) {
        if (!options["zTranslation"].is_number()) {
            throw std::invalid_argument("options.zTranslation must be a number.");
        }
        zTranslation = options["zTranslation"].get<double>();
    }

    ApplyTransformToGeoJson(geojsonNode, [distanceMeters, direction, zTranslation](json& coordinate) {
        const LonLatPoint dest = DestinationPoint(coordinate[1].get<double>(), coordinate[0].get<double>(), direction, distanceMeters);
        coordinate[0] = dest.lon;
        coordinate[1] = dest.lat;

        if (coordinate.size() > 2 && coordinate[2].is_number()) {
            coordinate[2] = coordinate[2].get<double>() + zTranslation;
        }
    });

    return {
        "Translated GeoJSON object.",
        geojsonNode
    };
}

ToolResult TransformationTransformScale(const json& arguments) {
    json geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    if (!arguments.contains("factor") || !arguments["factor"].is_number()) {
        throw std::invalid_argument("Field 'factor' must be a number.");
    }

    const double factor = arguments["factor"].get<double>();
    const json options = ParseOptionsArgument(arguments);
    const LonLatPoint origin = ResolveOrigin(options, "origin", geojsonNode);

    const double cosLat0 = std::cos(DegreesToRadians(origin.lat));
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("transformScale is unstable near the poles.");
    }

    ApplyTransformToGeoJson(geojsonNode, [origin, factor, cosLat0](json& coordinate) {
        const double x = kEarthRadiusMeters * DegreesToRadians(coordinate[0].get<double>() - origin.lon) * cosLat0;
        const double y = kEarthRadiusMeters * DegreesToRadians(coordinate[1].get<double>() - origin.lat);

        const double xs = x * factor;
        const double ys = y * factor;

        coordinate[0] = NormalizeLon180(origin.lon + RadiansToDegrees(xs / (kEarthRadiusMeters * cosLat0)));
        coordinate[1] = origin.lat + RadiansToDegrees(ys / kEarthRadiusMeters);
    });

    return {
        "Scaled GeoJSON object.",
        geojsonNode
    };
}

ToolResult TransformationUnion(const json& arguments) {
    const json featureCollection = ParseFeatureCollectionArgument(arguments, "featureCollection");
    const json output = TransformationGeometryOps::PolygonUnion(featureCollection);
    return {
        "Calculated polygon union (convex approximation).",
        output
    };
}

ToolResult TransformationVoronoi(const json& arguments) {
    const json points = ParseFeatureCollectionArgument(arguments, "points");
    const json options = ParseOptionsArgument(arguments);
    const json output = TransformationGeometryOps::BuildVoronoi(points, options);
    return {
        "Generated Voronoi polygons from points.",
        output
    };
}

} // namespace

void RegisterTransformationTools(ToolRegistry& registry) {
    using namespace TransformationSchemas;

    registry.Register({
        {kBboxClipName, kBboxClipDescription, kBboxClipSchema},
        TransformationBboxClip
    });
    registry.Register({
        {kBezierSplineName, kBezierSplineDescription, kBezierSplineSchema},
        TransformationBezierSpline
    });
    registry.Register({
        {kBufferName, kBufferDescription, kBufferSchema},
        TransformationBuffer
    });
    registry.Register({
        {kCircleName, kCircleDescription, kCircleSchema},
        TransformationCircle
    });
    registry.Register({
        {kCloneName, kCloneDescription, kCloneSchema},
        TransformationClone
    });
    registry.Register({
        {kConcaveName, kConcaveDescription, kConcaveSchema},
        TransformationConcave
    });
    registry.Register({
        {kConvexName, kConvexDescription, kConvexSchema},
        TransformationConvex
    });
    registry.Register({
        {kDifferenceName, kDifferenceDescription, kDifferenceSchema},
        TransformationDifference
    });
    registry.Register({
        {kDissolveName, kDissolveDescription, kDissolveSchema},
        TransformationDissolve
    });
    registry.Register({
        {kIntersectName, kIntersectDescription, kIntersectSchema},
        TransformationIntersect
    });
    registry.Register({
        {kLineOffsetName, kLineOffsetDescription, kLineOffsetSchema},
        TransformationLineOffset
    });
    registry.Register({
        {kSimplifyName, kSimplifyDescription, kSimplifySchema},
        TransformationSimplify
    });
    registry.Register({
        {kTesselateName, kTesselateDescription, kTesselateSchema},
        TransformationTesselate
    });
    registry.Register({
        {kTransformRotateName, kTransformRotateDescription, kTransformRotateSchema},
        TransformationTransformRotate
    });
    registry.Register({
        {kTransformTranslateName, kTransformTranslateDescription, kTransformTranslateSchema},
        TransformationTransformTranslate
    });
    registry.Register({
        {kTransformScaleName, kTransformScaleDescription, kTransformScaleSchema},
        TransformationTransformScale
    });
    registry.Register({
        {kUnionName, kUnionDescription, kUnionSchema},
        TransformationUnion
    });
    registry.Register({
        {kVoronoiName, kVoronoiDescription, kVoronoiSchema},
        TransformationVoronoi
    });
}
