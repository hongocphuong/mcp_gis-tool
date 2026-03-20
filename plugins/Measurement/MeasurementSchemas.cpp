#include "MeasurementSchemas.h"

namespace MeasurementSchemas {

// Along
const char* kAlongName = "along";
const char* kAlongDescription = "Calculate a point at a specified distance along a LineString.";
const char* kAlongSchema = R"({
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

// Area
const char* kAreaName = "area";
const char* kAreaDescription = "Calculate the area of a polygon in square meters.";
const char* kAreaSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "polygon": {"type": ["string", "object"]}
    },
    "required": ["polygon"],
    "additionalProperties": false
})";

// Bbox
const char* kBboxName = "bbox";
const char* kBboxDescription = "Calculate the bounding box of a GeoJSON object.";
const char* kBboxSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// BboxPolygon
const char* kBboxPolygonName = "bboxPolygon";
const char* kBboxPolygonDescription = "Convert a bounding box array to a Polygon feature.";
const char* kBboxPolygonSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "bbox": {"type": ["string", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["bbox"],
    "additionalProperties": false
})";

// Bearing
const char* kBearingName = "bearing";
const char* kBearingDescription = "Calculate the bearing between two points.";
const char* kBearingSchema = R"({
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

// Center
const char* kCenterName = "center";
const char* kCenterDescription = "Calculate the center point of a FeatureCollection.";
const char* kCenterSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// CenterOfMass
const char* kCenterOfMassName = "centerOfMass";
const char* kCenterOfMassDescription = "Calculate the center of mass of a GeoJSON object.";
const char* kCenterOfMassSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// Centroid
const char* kCentroidName = "centroid";
const char* kCentroidDescription = "Calculate the centroid of a GeoJSON object.";
const char* kCentroidSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// Destination
const char* kDestinationName = "destination";
const char* kDestinationDescription = "Calculate a destination point given a starting point, distance, and bearing.";
const char* kDestinationSchema = R"({
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

// Distance
const char* kDistanceName = "distance";
const char* kDistanceDescription = "Calculate the distance between two points.";
const char* kDistanceSchema = R"({
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

// Envelope
const char* kEnvelopeName = "envelope";
const char* kEnvelopeDescription = "Calculate the envelope (bounding box polygon) of a GeoJSON object.";
const char* kEnvelopeSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// GreatCircle
const char* kGreatCircleName = "greatCircle";
const char* kGreatCircleDescription = "Calculate the great circle path between two points.";
const char* kGreatCircleSchema = R"({
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

// Length
const char* kLengthName = "length";
const char* kLengthDescription = "Calculate the length of a LineString or MultiLineString.";
const char* kLengthSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// Midpoint
const char* kMidpointName = "midpoint";
const char* kMidpointDescription = "Calculate the midpoint between two points.";
const char* kMidpointSchema = R"({
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

// PointOnFeature
const char* kPointOnFeatureName = "pointOnFeature";
const char* kPointOnFeatureDescription = "Find a point on a GeoJSON feature.";
const char* kPointOnFeatureSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

// PointToLineDistance
const char* kPointToLineDistanceName = "pointToLineDistance";
const char* kPointToLineDistanceDescription = "Calculate the shortest distance from a point to a line.";
const char* kPointToLineDistanceSchema = R"({
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

// PolygonTangents
const char* kPolygonTangentsName = "polygonTangents";
const char* kPolygonTangentsDescription = "Calculate the two tangent points from a point to a polygon.";
const char* kPolygonTangentsSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point": {"type": ["string", "object", "array"]},
        "polygon": {"type": ["string", "object"]}
    },
    "required": ["point", "polygon"],
    "additionalProperties": false
})";

// RhumbBearing
const char* kRhumbBearingName = "rhumbBearing";
const char* kRhumbBearingDescription = "Calculate the rhumb bearing between two points.";
const char* kRhumbBearingSchema = R"({
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

// RhumbDestination
const char* kRhumbDestinationName = "rhumbDestination";
const char* kRhumbDestinationDescription = "Calculate a destination point along a rhumb line.";
const char* kRhumbDestinationSchema = R"({
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

// RhumbDistance
const char* kRhumbDistanceName = "rhumbDistance";
const char* kRhumbDistanceDescription = "Calculate the rhumb distance between two points.";
const char* kRhumbDistanceSchema = R"({
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

// Square
const char* kSquareName = "square";
const char* kSquareDescription = "Calculate the smallest square bounding box that contains a given bounding box.";
const char* kSquareSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "bbox": {"type": ["string", "array"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["bbox"],
    "additionalProperties": false
})";

} // namespace MeasurementSchemas
