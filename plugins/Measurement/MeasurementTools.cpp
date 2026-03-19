#include "MeasurementTools.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "geojson/GeoJsonTraversal.h"
#include "GisTypes.h"
#include "GeoAlgorithms/GeoAlgorithms.h"
#include "GeoMath/GeoMath.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"

namespace {

using GeoMath::kPi;
using GeoMath::kEarthRadiusMeters;
using GeoMath::DegreesToRadians;
using GeoMath::RadiansToDegrees;
using GeoMath::NormalizeLon180;
using GeoMath::NormalizeDegrees360;
using GeoMath::HaversineDistanceMeters;
using GeoMath::InitialBearingDegrees;
using GeoMath::DestinationPoint;
using GeoMath::SphericalMidpoint;
using GeoMath::InterpolateGreatCircle;
using GeoMath::RhumbDistanceMeters;
using GeoMath::RhumbBearingDegrees;
using GeoMath::RhumbDestinationPoint;
using GeoAlgorithms::PolygonAreaCentroid;
using GeoAlgorithms::ComputePolygonMetrics;
using GeoAlgorithms::MeanCentroid;
using GeoAlgorithms::PointInPolygon;
using GeoAlgorithms::PointToSegmentDistanceMeters;
using GeoJsonParsingUtils::ParseFeatureCollectionLike;
using GeoJsonParsingUtils::ParseJsonLikeArgument;
using GeoJsonParsingUtils::ParseLineOrMultiLine;
using GeoJsonParsingUtils::ParseLineStringPoints;
using GeoJsonParsingUtils::ParseOptionsArgument;
using GeoJsonParsingUtils::ParsePointLike;

constexpr char kAlongSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "line": {"type": ["string", "object"]},
        "distance": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["line", "distance"],
    "additionalProperties": false
})";

constexpr char kAreaSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "polygon": {"type": ["string", "object"]}
    },
    "required": ["polygon"],
    "additionalProperties": false
})";

constexpr char kGeoJsonSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

constexpr char kBboxSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "bbox": {"type": ["string", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["bbox"],
    "additionalProperties": false
})";

constexpr char kBearingSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point1": {"type": ["string", "object", "array"]},
        "point2": {"type": ["string", "object", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["point1", "point2"],
    "additionalProperties": false
})";

constexpr char kDestinationSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "origin": {"type": ["string", "object", "array"]},
        "distance": {"type": "number"},
        "bearing": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["origin", "distance", "bearing"],
    "additionalProperties": false
})";

constexpr char kDistanceSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point1": {"type": ["string", "object", "array"]},
        "point2": {"type": ["string", "object", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["point1", "point2"],
    "additionalProperties": false
})";

constexpr char kLengthSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

constexpr char kPointToLineSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point": {"type": ["string", "object", "array"]},
        "line": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["point", "line"],
    "additionalProperties": false
})";

constexpr char kGreatCircleSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "start": {"type": ["string", "object", "array"]},
        "end": {"type": ["string", "object", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["start", "end"],
    "additionalProperties": false
})";

constexpr char kPolygonTangentsSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point": {"type": ["string", "object", "array"]},
        "polygon": {"type": ["string", "object"]}
    },
    "required": ["point", "polygon"],
    "additionalProperties": false
})";

std::string ReadUnits(const json& options, const std::string& defaultUnits = "kilometers") {
    if (!options.contains("units")) {
        return defaultUnits;
    }

    const std::string units = options.at("units").get<std::string>();
    return units;
}

double MetersPerUnit(const std::string& units) {
    if (units == "meters" || units == "meter") {
        return 1.0;
    }
    if (units == "kilometers" || units == "kilometer") {
        return 1000.0;
    }
    if (units == "miles" || units == "mile") {
        return 1609.344;
    }
    if (units == "nauticalmiles" || units == "nauticalmile") {
        return 1852.0;
    }
    if (units == "radians" || units == "radian") {
        return kEarthRadiusMeters;
    }
    if (units == "degrees" || units == "degree") {
        return kEarthRadiusMeters * (kPi / 180.0);
    }

    throw std::invalid_argument("Unsupported units: " + units + ". Supported: meters, kilometers, miles, nauticalmiles, degrees, radians.");
}

double ToMeters(double value, const std::string& units) {
    return value * MetersPerUnit(units);
}

double FromMeters(double meters, const std::string& units) {
    return meters / MetersPerUnit(units);
}

json BuildPointFeature(double lon, double lat, const json& options = json::object()) {
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

Bbox ComputeBbox(const json& geojsonNode) {
    const auto coords = CollectCoordinates(geojsonNode);

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

Bbox ParseBbox(const json& bboxNode) {
    if (!bboxNode.is_array() || bboxNode.size() != 4) {
        throw std::invalid_argument("bbox must be an array [minX, minY, maxX, maxY].");
    }
    if (!bboxNode[0].is_number() || !bboxNode[1].is_number() || !bboxNode[2].is_number() || !bboxNode[3].is_number()) {
        throw std::invalid_argument("bbox coordinates must be numbers.");
    }

    Bbox bbox{
        bboxNode[0].get<double>(),
        bboxNode[1].get<double>(),
        bboxNode[2].get<double>(),
        bboxNode[3].get<double>()
    };

    if (bbox.minX > bbox.maxX || bbox.minY > bbox.maxY) {
        throw std::invalid_argument("bbox must satisfy minX <= maxX and minY <= maxY.");
    }

    return bbox;
}

json BboxToPolygonFeature(const Bbox& bbox, const json& options = json::object()) {
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

LonLatPoint CenterOfMass(const json& geojsonNode) {
    const auto polygons = CollectPolygons(geojsonNode);
    if (polygons.empty()) {
        const auto coords = CollectCoordinates(geojsonNode);
        return MeanCentroid(coords);
    }

    double totalArea = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;

    double lat0Sum = 0.0;
    size_t lat0Count = 0;

    for (const auto& polygon : polygons) {
        for (const auto& point : polygon.outerRing) {
            lat0Sum += point.lat;
            ++lat0Count;
        }

        const PolygonAreaCentroid metrics = ComputePolygonMetrics(polygon);
        totalArea += metrics.area;
        weightedX += metrics.area * metrics.centroidX;
        weightedY += metrics.area * metrics.centroidY;
    }

    if (totalArea <= 0.0) {
        throw std::invalid_argument("Cannot compute center of mass from zero polygon area.");
    }

    const double lat0Rad = DegreesToRadians(lat0Sum / static_cast<double>(lat0Count));
    const double cosLat0 = std::cos(lat0Rad);
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("Center of mass conversion failed near poles.");
    }

    const double centroidX = weightedX / totalArea;
    const double centroidY = weightedY / totalArea;

    return {
        RadiansToDegrees(centroidX / (kEarthRadiusMeters * cosLat0)),
        RadiansToDegrees(centroidY / kEarthRadiusMeters)
    };
}

json DistanceResultObject(double meters, const std::string& units) {
    return {
        {"value", FromMeters(meters, units)},
        {"units", units}
    };
}

ToolResult MeasurementAlong(const json& arguments) {
    const auto lineNode = ParseJsonLikeArgument(arguments, "line");
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const double targetMeters = ToMeters(distance, units);
    const auto points = ParseLineStringPoints(lineNode, "line");

    double accumulated = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        const double segLen = HaversineDistanceMeters(points[i - 1].lat, points[i - 1].lon, points[i].lat, points[i].lon);
        if (accumulated + segLen >= targetMeters) {
            const double remaining = targetMeters - accumulated;
            const double bearing = InitialBearingDegrees(points[i - 1].lat, points[i - 1].lon, points[i].lat, points[i].lon);
            const LonLatPoint p = DestinationPoint(points[i - 1].lat, points[i - 1].lon, bearing, remaining);
            return {
                "Computed point along line.",
                BuildPointFeature(p.lon, p.lat)
            };
        }
        accumulated += segLen;
    }

    const auto& last = points.back();
    return {
        "Distance exceeds line length; returned line endpoint.",
        BuildPointFeature(last.lon, last.lat)
    };
}

ToolResult MeasurementArea(const json& arguments) {
    const auto polygonNode = ParseJsonLikeArgument(arguments, "polygon");
    const auto polygons = CollectPolygons(polygonNode);
    if (polygons.empty()) {
        throw std::invalid_argument("polygon must contain Polygon or MultiPolygon geometry.");
    }

    double totalArea = 0.0;
    for (const auto& polygon : polygons) {
        totalArea += ComputePolygonMetrics(polygon).area;
    }

    return {
        "Computed polygon area.",
        {
            {"value", totalArea},
            {"units", "square meters"}
        }
    };
}

ToolResult MeasurementBbox(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const Bbox bbox = ComputeBbox(geojsonNode);

    return {
        "Computed bounding box.",
        json::array({bbox.minX, bbox.minY, bbox.maxX, bbox.maxY})
    };
}

ToolResult MeasurementBboxPolygon(const json& arguments) {
    const auto bboxNode = ParseJsonLikeArgument(arguments, "bbox");
    const auto options = ParseOptionsArgument(arguments);
    const Bbox bbox = ParseBbox(bboxNode);

    return {
        "Converted bbox to polygon.",
        BboxToPolygonFeature(bbox, options)
    };
}

ToolResult MeasurementBearing(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));

    if (p1.lat == p2.lat && p1.lon == p2.lon) {
        throw std::invalid_argument("Cannot compute bearing for identical points.");
    }

    const double value = InitialBearingDegrees(p1.lat, p1.lon, p2.lat, p2.lon);
    return {
        "Computed bearing.",
        {
            {"value", value},
            {"units", "degrees"}
        }
    };
}

ToolResult MeasurementCenter(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    ParseFeatureCollectionLike(geojsonNode, "geojson");

    const Bbox bbox = ComputeBbox(geojsonNode);
    const double centerLon = (bbox.minX + bbox.maxX) / 2.0;
    const double centerLat = (bbox.minY + bbox.maxY) / 2.0;

    return {
        "Computed FeatureCollection center point.",
        BuildPointFeature(centerLon, centerLat, options)
    };
}

ToolResult MeasurementCenterOfMass(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const LonLatPoint com = CenterOfMass(geojsonNode);
    return {
        "Computed center of mass.",
        BuildPointFeature(com.lon, com.lat, options)
    };
}

ToolResult MeasurementCentroid(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const auto coords = CollectCoordinates(geojsonNode);
    const LonLatPoint centroid = MeanCentroid(coords);

    return {
        "Computed centroid.",
        BuildPointFeature(centroid.lon, centroid.lat, options)
    };
}

ToolResult MeasurementDestination(const json& arguments) {
    const LonLatPoint origin = ParsePointLike(ParseJsonLikeArgument(arguments, "origin"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    const double bearing = arguments.at("bearing").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const LonLatPoint destination = DestinationPoint(origin.lat, origin.lon, bearing, ToMeters(distance, units));

    return {
        "Computed destination point.",
        BuildPointFeature(destination.lon, destination.lat, options)
    };
}

ToolResult MeasurementDistance(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const double meters = HaversineDistanceMeters(p1.lat, p1.lon, p2.lat, p2.lon);

    return {
        "Computed distance.",
        DistanceResultObject(meters, units)
    };
}

ToolResult MeasurementEnvelope(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const Bbox bbox = ComputeBbox(geojsonNode);

    return {
        "Computed envelope polygon.",
        BboxToPolygonFeature(bbox)
    };
}

ToolResult MeasurementLength(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const auto lines = ParseLineOrMultiLine(geojsonNode, "geojson");
    const std::string units = ReadUnits(options, "kilometers");

    double totalMeters = 0.0;
    for (const auto& line : lines) {
        for (size_t i = 1; i < line.size(); ++i) {
            totalMeters += HaversineDistanceMeters(line[i - 1].lat, line[i - 1].lon, line[i].lat, line[i].lon);
        }
    }

    return {
        "Computed line length.",
        DistanceResultObject(totalMeters, units)
    };
}

ToolResult MeasurementMidpoint(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));

    const LonLatPoint midpoint = SphericalMidpoint(p1.lat, p1.lon, p2.lat, p2.lon);

    return {
        "Computed midpoint.",
        BuildPointFeature(midpoint.lon, midpoint.lat)
    };
}

ToolResult MeasurementPointOnFeature(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");

    if (!geojsonNode.is_object() || !geojsonNode.contains("type") || !geojsonNode["type"].is_string()) {
        throw std::invalid_argument("geojson must be a valid GeoJSON object.");
    }

    json node = geojsonNode;
    if (node["type"] == "Feature") {
        if (!node.contains("geometry") || node["geometry"].is_null()) {
            throw std::invalid_argument("Feature must have non-null geometry.");
        }
        node = node["geometry"];
    }

    const std::string type = node["type"].get<std::string>();

    if (type == "Point") {
        const LonLatPoint p = ParsePointLike(node);
        return {"Point lies on feature.", BuildPointFeature(p.lon, p.lat)};
    }

    if (type == "LineString") {
        const auto line = ParseLineStringPoints(node, "geojson");
        double totalMeters = 0.0;
        for (size_t i = 1; i < line.size(); ++i) {
            totalMeters += HaversineDistanceMeters(line[i - 1].lat, line[i - 1].lon, line[i].lat, line[i].lon);
        }
        const double half = totalMeters / 2.0;
        double accumulated = 0.0;
        for (size_t i = 1; i < line.size(); ++i) {
            const double seg = HaversineDistanceMeters(line[i - 1].lat, line[i - 1].lon, line[i].lat, line[i].lon);
            if (accumulated + seg >= half) {
                const double bearing = InitialBearingDegrees(line[i - 1].lat, line[i - 1].lon, line[i].lat, line[i].lon);
                const LonLatPoint p = DestinationPoint(line[i - 1].lat, line[i - 1].lon, bearing, half - accumulated);
                return {"Computed point on line feature.", BuildPointFeature(p.lon, p.lat)};
            }
            accumulated += seg;
        }
        return {"Returned line endpoint.", BuildPointFeature(line.back().lon, line.back().lat)};
    }

    if (type == "Polygon" || type == "MultiPolygon") {
        const LonLatPoint candidate = CenterOfMass(node);
        const auto polygons = CollectPolygons(node);
        for (const auto& poly : polygons) {
            if (PointInPolygon(candidate.lon, candidate.lat, poly)) {
                return {"Computed point on polygon feature.", BuildPointFeature(candidate.lon, candidate.lat)};
            }
        }

        if (!polygons.empty() && !polygons[0].outerRing.empty()) {
            const LonLatPoint& fallback = polygons[0].outerRing.front();
            return {"Center fell outside polygon; returned polygon vertex.", BuildPointFeature(fallback.lon, fallback.lat)};
        }
    }

    const auto coords = CollectCoordinates(geojsonNode);
    const LonLatPoint fallback = MeanCentroid(coords);
    return {
        "Returned centroid as point on feature fallback.",
        BuildPointFeature(fallback.lon, fallback.lat)
    };
}

ToolResult MeasurementPointToLineDistance(const json& arguments) {
    const LonLatPoint point = ParsePointLike(ParseJsonLikeArgument(arguments, "point"));
    const auto lineNode = ParseJsonLikeArgument(arguments, "line");
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const std::string method = options.contains("method") ? options.at("method").get<std::string>() : "geodesic";
    if (method != "geodesic" && method != "planar") {
        throw std::invalid_argument("options.method must be 'geodesic' or 'planar'.");
    }

    const auto points = ParseLineStringPoints(lineNode, "line");

    double minMeters = std::numeric_limits<double>::max();
    for (size_t i = 1; i < points.size(); ++i) {
        minMeters = std::min(minMeters, PointToSegmentDistanceMeters(point, points[i - 1], points[i]));
    }

    return {
        "Computed point-to-line distance.",
        DistanceResultObject(minMeters, units)
    };
}

ToolResult MeasurementRhumbBearing(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));
    const auto options = ParseOptionsArgument(arguments);

    if (p1.lat == p2.lat && p1.lon == p2.lon) {
        throw std::invalid_argument("Cannot compute rhumb bearing for identical points.");
    }

    double bearing = RhumbBearingDegrees(p1.lat, p1.lon, p2.lat, p2.lon);
    if (options.contains("final") && options.at("final").get<bool>()) {
        bearing = NormalizeDegrees360(RhumbBearingDegrees(p2.lat, p2.lon, p1.lat, p1.lon) + 180.0);
    }

    return {
        "Computed rhumb bearing.",
        {
            {"value", bearing},
            {"units", "degrees"}
        }
    };
}

ToolResult MeasurementRhumbDestination(const json& arguments) {
    const LonLatPoint origin = ParsePointLike(ParseJsonLikeArgument(arguments, "origin"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    const double bearing = arguments.at("bearing").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const LonLatPoint destination = RhumbDestinationPoint(origin.lat, origin.lon, bearing, ToMeters(distance, units));

    return {
        "Computed rhumb destination.",
        BuildPointFeature(destination.lon, destination.lat, options)
    };
}

ToolResult MeasurementRhumbDistance(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = ReadUnits(options, "kilometers");
    const double meters = RhumbDistanceMeters(p1.lat, p1.lon, p2.lat, p2.lon);

    return {
        "Computed rhumb distance.",
        DistanceResultObject(meters, units)
    };
}

ToolResult MeasurementSquare(const json& arguments) {
    const auto bboxNode = ParseJsonLikeArgument(arguments, "bbox");
    const Bbox bbox = ParseBbox(bboxNode);

    const double width = bbox.maxX - bbox.minX;
    const double height = bbox.maxY - bbox.minY;
    const double side = std::max(width, height);

    const double cx = (bbox.minX + bbox.maxX) / 2.0;
    const double cy = (bbox.minY + bbox.maxY) / 2.0;

    const Bbox square{
        cx - side / 2.0,
        cy - side / 2.0,
        cx + side / 2.0,
        cy + side / 2.0
    };

    return {
        "Computed square bbox.",
        json::array({square.minX, square.minY, square.maxX, square.maxY})
    };
}

ToolResult MeasurementGreatCircle(const json& arguments) {
    const LonLatPoint start = ParsePointLike(ParseJsonLikeArgument(arguments, "start"));
    const LonLatPoint end = ParsePointLike(ParseJsonLikeArgument(arguments, "end"));
    const auto options = ParseOptionsArgument(arguments);

    int npoints = 100;
    if (options.contains("npoints")) {
        npoints = options.at("npoints").get<int>();
    }
    if (npoints < 2) {
        npoints = 2;
    }

    json coordinates = json::array();
    coordinates.push_back(json::array({start.lon, start.lat}));
    for (int i = 1; i < npoints - 1; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(npoints - 1);
        const LonLatPoint p = InterpolateGreatCircle(start, end, t);
        coordinates.push_back(json::array({p.lon, p.lat}));
    }
    coordinates.push_back(json::array({end.lon, end.lat}));

    json feature = {
        {"type", "Feature"},
        {"properties", json::object()},
        {"geometry", {
            {"type", "LineString"},
            {"coordinates", coordinates}
        }}
    };

    if (options.contains("properties") && options["properties"].is_object()) {
        feature["properties"] = options["properties"];
    }

    return {
        "Computed great circle path.",
        feature
    };
}

ToolResult MeasurementPolygonTangents(const json& arguments) {
    const LonLatPoint point = ParsePointLike(ParseJsonLikeArgument(arguments, "point"));
    const auto polygonNode = ParseJsonLikeArgument(arguments, "polygon");

    const auto polygons = CollectPolygons(polygonNode);
    if (polygons.empty()) {
        throw std::invalid_argument("polygon must contain Polygon or MultiPolygon geometry.");
    }

    bool found = false;
    double minAngle = 0.0;
    double maxAngle = 0.0;
    LonLatPoint left{};
    LonLatPoint right{};

    for (const auto& poly : polygons) {
        for (const auto& vertex : poly.outerRing) {
            const double angle = std::atan2(vertex.lat - point.lat, vertex.lon - point.lon);
            if (!found || angle < minAngle) {
                minAngle = angle;
                left = {vertex.lon, vertex.lat};
            }
            if (!found || angle > maxAngle) {
                maxAngle = angle;
                right = {vertex.lon, vertex.lat};
            }
            found = true;
        }
    }

    if (!found) {
        throw std::invalid_argument("Polygon has no vertices.");
    }

    json collection = {
        {"type", "FeatureCollection"},
        {"features", json::array({
            BuildPointFeature(left.lon, left.lat),
            BuildPointFeature(right.lon, right.lat)
        })}
    };

    return {
        "Computed polygon tangents.",
        collection
    };
}

} // namespace

void RegisterMeasurementTools(ToolRegistry& registry) {
    registry.Register({
        {"greatCircle", "Calculate the great circle path between two points.", kGreatCircleSchema},
        MeasurementGreatCircle
    });

    registry.Register({
        {"polygonTangents", "Calculate the two tangent points from a point to a polygon.", kPolygonTangentsSchema},
        MeasurementPolygonTangents
    });

    registry.Register({
        {"along", "Calculate a point at a specified distance along a LineString.", kAlongSchema},
        MeasurementAlong
    });

    registry.Register({
        {"area", "Calculate the area of a polygon in square meters.", kAreaSchema},
        MeasurementArea
    });

    registry.Register({
        {"bbox", "Calculate the bounding box of a GeoJSON object.", kGeoJsonSchema},
        MeasurementBbox
    });

    registry.Register({
        {"bboxPolygon", "Convert a bounding box array to a Polygon feature.", kBboxSchema},
        MeasurementBboxPolygon
    });

    registry.Register({
        {"bearing", "Calculate the bearing between two points.", kBearingSchema},
        MeasurementBearing
    });

    registry.Register({
        {"center", "Calculate the center point of a FeatureCollection.", kGeoJsonSchema},
        MeasurementCenter
    });

    registry.Register({
        {"centerOfMass", "Calculate the center of mass of a GeoJSON object.", kGeoJsonSchema},
        MeasurementCenterOfMass
    });

    registry.Register({
        {"centroid", "Calculate the centroid of a GeoJSON object.", kGeoJsonSchema},
        MeasurementCentroid
    });

    registry.Register({
        {"destination", "Calculate a destination point given a starting point, distance, and bearing.", kDestinationSchema},
        MeasurementDestination
    });

    registry.Register({
        {"distance", "Calculate the distance between two points.", kDistanceSchema},
        MeasurementDistance
    });

    registry.Register({
        {"envelope", "Calculate the envelope (bounding box polygon) of a GeoJSON object.", kGeoJsonSchema},
        MeasurementEnvelope
    });

    registry.Register({
        {"length", "Calculate the length of a LineString or MultiLineString.", kLengthSchema},
        MeasurementLength
    });

    registry.Register({
        {"midpoint", "Calculate the midpoint between two points.", kDistanceSchema},
        MeasurementMidpoint
    });

    registry.Register({
        {"pointOnFeature", "Find a point on a GeoJSON feature.", kGeoJsonSchema},
        MeasurementPointOnFeature
    });

    registry.Register({
        {"pointToLineDistance", "Calculate the shortest distance from a point to a line.", kPointToLineSchema},
        MeasurementPointToLineDistance
    });

    registry.Register({
        {"rhumbBearing", "Calculate the rhumb bearing between two points.", kBearingSchema},
        MeasurementRhumbBearing
    });

    registry.Register({
        {"rhumbDestination", "Calculate a destination point along a rhumb line.", kDestinationSchema},
        MeasurementRhumbDestination
    });

    registry.Register({
        {"rhumbDistance", "Calculate the rhumb distance between two points.", kDistanceSchema},
        MeasurementRhumbDistance
    });

    registry.Register({
        {"square", "Calculate the smallest square bounding box that contains a given bounding box.", kBboxSchema},
        MeasurementSquare
    });
}
