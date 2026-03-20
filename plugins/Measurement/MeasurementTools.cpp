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
#include "FeatureBuilders/FeatureBuilders.h"
#include "GeoUnits/GeoUnits.h"
#include "MeasurementSchemas.h"

using json = nlohmann::json;

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
using GeoJsonTraversal::CollectCoordinates;
using GeoJsonTraversal::CollectPolygons;


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
        {"value", GeoUnits::FromMeters(meters, units)},
        {"units", units}
    };
}

ToolResult MeasurementAlong(const json& arguments) {
    const auto lineNode = ParseJsonLikeArgument(arguments, "line");
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const double targetMeters = GeoUnits::ToMeters(distance, units);
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
                FeatureBuilders::BuildPointFeature(p.lon, p.lat)
            };
        }
        accumulated += segLen;
    }

    const auto& last = points.back();
    return {
        "Distance exceeds line length; returned line endpoint.",
        FeatureBuilders::BuildPointFeature(last.lon, last.lat)
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
    const Bbox bbox = FeatureBuilders::ComputeBbox(geojsonNode);

    return {
        "Computed bounding box.",
        json::array({bbox.minX, bbox.minY, bbox.maxX, bbox.maxY})
    };
}

ToolResult MeasurementBboxPolygon(const json& arguments) {
    const auto bboxNode = ParseJsonLikeArgument(arguments, "bbox");
    const auto options = ParseOptionsArgument(arguments);
    const Bbox bbox = FeatureBuilders::ParseBbox(bboxNode);

    return {
        "Converted bbox to polygon.",
        FeatureBuilders::BboxToPolygonFeature(bbox, options)
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

    const json validatedNode = ParseFeatureCollectionLike(geojsonNode, "geojson");

    const Bbox bbox = FeatureBuilders::ComputeBbox(validatedNode);
    const double centerLon = (bbox.minX + bbox.maxX) / 2.0;
    const double centerLat = (bbox.minY + bbox.maxY) / 2.0;

    return {
        "Computed FeatureCollection center point.",
        FeatureBuilders::BuildPointFeature(centerLon, centerLat, options)
    };
}

ToolResult MeasurementCenterOfMass(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const LonLatPoint com = CenterOfMass(geojsonNode);
    return {
        "Computed center of mass.",
        FeatureBuilders::BuildPointFeature(com.lon, com.lat, options)
    };
}

ToolResult MeasurementCentroid(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const auto coords = CollectCoordinates(geojsonNode);
    const LonLatPoint centroid = MeanCentroid(coords);

    return {
        "Computed centroid.",
        FeatureBuilders::BuildPointFeature(centroid.lon, centroid.lat, options)
    };
}

ToolResult MeasurementDestination(const json& arguments) {
    const LonLatPoint origin = ParsePointLike(ParseJsonLikeArgument(arguments, "origin"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    const double bearing = arguments.at("bearing").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const LonLatPoint destination = DestinationPoint(origin.lat, origin.lon, bearing, GeoUnits::ToMeters(distance, units));

    return {
        "Computed destination point.",
        FeatureBuilders::BuildPointFeature(destination.lon, destination.lat, options)
    };
}

ToolResult MeasurementDistance(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double meters = HaversineDistanceMeters(p1.lat, p1.lon, p2.lat, p2.lon);

    return {
        "Computed distance.",
        DistanceResultObject(meters, units)
    };
}

ToolResult MeasurementEnvelope(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const Bbox bbox = FeatureBuilders::ComputeBbox(geojsonNode);

    return {
        "Computed envelope polygon.",
        FeatureBuilders::BboxToPolygonFeature(bbox)
    };
}

ToolResult MeasurementLength(const json& arguments) {
    const auto geojsonNode = ParseJsonLikeArgument(arguments, "geojson");
    const auto options = ParseOptionsArgument(arguments);

    const auto lines = ParseLineOrMultiLine(geojsonNode, "geojson");
    const std::string units = GeoUnits::ReadUnits(options, "kilometers");

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
        FeatureBuilders::BuildPointFeature(midpoint.lon, midpoint.lat)
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
        return {"Point lies on feature.", FeatureBuilders::BuildPointFeature(p.lon, p.lat)};
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
                return {"Computed point on line feature.", FeatureBuilders::BuildPointFeature(p.lon, p.lat)};
            }
            accumulated += seg;
        }
        return {"Returned line endpoint.", FeatureBuilders::BuildPointFeature(line.back().lon, line.back().lat)};
    }

    if (type == "Polygon" || type == "MultiPolygon") {
        const LonLatPoint candidate = CenterOfMass(node);
        const auto polygons = CollectPolygons(node);
        for (const auto& poly : polygons) {
            if (PointInPolygon(candidate.lon, candidate.lat, poly)) {
                return {"Computed point on polygon feature.", FeatureBuilders::BuildPointFeature(candidate.lon, candidate.lat)};
            }
        }

        if (!polygons.empty() && !polygons[0].outerRing.empty()) {
            const LonLatPoint& fallback = polygons[0].outerRing.front();
            return {"Center fell outside polygon; returned polygon vertex.", FeatureBuilders::BuildPointFeature(fallback.lon, fallback.lat)};
        }
    }

    const auto coords = CollectCoordinates(geojsonNode);
    const LonLatPoint fallback = MeanCentroid(coords);
    return {
        "Returned centroid as point on feature fallback.",
        FeatureBuilders::BuildPointFeature(fallback.lon, fallback.lat)
    };
}

ToolResult MeasurementPointToLineDistance(const json& arguments) {
    const LonLatPoint point = ParsePointLike(ParseJsonLikeArgument(arguments, "point"));
    const auto lineNode = ParseJsonLikeArgument(arguments, "line");
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
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

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double distance = arguments.at("distance").get<double>();
    const double bearing = arguments.at("bearing").get<double>();
    if (distance < 0.0) {
        throw std::invalid_argument("distance must be >= 0.");
    }

    const LonLatPoint destination = RhumbDestinationPoint(origin.lat, origin.lon, bearing, GeoUnits::ToMeters(distance, units));

    return {
        "Computed rhumb destination.",
        FeatureBuilders::BuildPointFeature(destination.lon, destination.lat, options)
    };
}

ToolResult MeasurementRhumbDistance(const json& arguments) {
    const LonLatPoint p1 = ParsePointLike(ParseJsonLikeArgument(arguments, "point1"));
    const LonLatPoint p2 = ParsePointLike(ParseJsonLikeArgument(arguments, "point2"));
    const auto options = ParseOptionsArgument(arguments);

    const std::string units = GeoUnits::ReadUnits(options, "kilometers");
    const double meters = RhumbDistanceMeters(p1.lat, p1.lon, p2.lat, p2.lon);

    return {
        "Computed rhumb distance.",
        DistanceResultObject(meters, units)
    };
}

ToolResult MeasurementSquare(const json& arguments) {
    const auto bboxNode = ParseJsonLikeArgument(arguments, "bbox");
    const Bbox bbox = FeatureBuilders::ParseBbox(bboxNode);

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
            FeatureBuilders::BuildPointFeature(left.lon, left.lat),
            FeatureBuilders::BuildPointFeature(right.lon, right.lat)
        })}
    };

    return {
        "Computed polygon tangents.",
        collection
    };
}

} // namespace

void RegisterMeasurementTools(ToolRegistry& registry) {
    using namespace MeasurementSchemas;

    registry.Register({
        {kGreatCircleName, kGreatCircleDescription, kGreatCircleSchema},
        MeasurementGreatCircle
    });

    registry.Register({
        {kPolygonTangentsName, kPolygonTangentsDescription, kPolygonTangentsSchema},
        MeasurementPolygonTangents
    });

    registry.Register({
        {kAlongName, kAlongDescription, kAlongSchema},
        MeasurementAlong
    });

    registry.Register({
        {kAreaName, kAreaDescription, kAreaSchema},
        MeasurementArea
    });

    registry.Register({
        {kBboxName, kBboxDescription, kBboxSchema},
        MeasurementBbox
    });

    registry.Register({
        {kBboxPolygonName, kBboxPolygonDescription, kBboxPolygonSchema},
        MeasurementBboxPolygon
    });

    registry.Register({
        {kBearingName, kBearingDescription, kBearingSchema},
        MeasurementBearing
    });

    registry.Register({
        {kCenterName, kCenterDescription, kCenterSchema},
        MeasurementCenter
    });

    registry.Register({
        {kCenterOfMassName, kCenterOfMassDescription, kCenterOfMassSchema},
        MeasurementCenterOfMass
    });

    registry.Register({
        {kCentroidName, kCentroidDescription, kCentroidSchema},
        MeasurementCentroid
    });

    registry.Register({
        {kDestinationName, kDestinationDescription, kDestinationSchema},
        MeasurementDestination
    });

    registry.Register({
        {kDistanceName, kDistanceDescription, kDistanceSchema},
        MeasurementDistance
    });

    registry.Register({
        {kEnvelopeName, kEnvelopeDescription, kEnvelopeSchema},
        MeasurementEnvelope
    });

    registry.Register({
        {kLengthName, kLengthDescription, kLengthSchema},
        MeasurementLength
    });

    registry.Register({
        {kMidpointName, kMidpointDescription, kMidpointSchema},
        MeasurementMidpoint
    });

    registry.Register({
        {kPointOnFeatureName, kPointOnFeatureDescription, kPointOnFeatureSchema},
        MeasurementPointOnFeature
    });

    registry.Register({
        {kPointToLineDistanceName, kPointToLineDistanceDescription, kPointToLineDistanceSchema},
        MeasurementPointToLineDistance
    });

    registry.Register({
        {kRhumbBearingName, kRhumbBearingDescription, kRhumbBearingSchema},
        MeasurementRhumbBearing
    });

    registry.Register({
        {kRhumbDestinationName, kRhumbDestinationDescription, kRhumbDestinationSchema},
        MeasurementRhumbDestination
    });

    registry.Register({
        {kRhumbDistanceName, kRhumbDistanceDescription, kRhumbDistanceSchema},
        MeasurementRhumbDistance
    });

    registry.Register({
        {kSquareName, kSquareDescription, kSquareSchema},
        MeasurementSquare
    });
}
