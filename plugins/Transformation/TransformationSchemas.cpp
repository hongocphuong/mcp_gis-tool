#include "TransformationSchemas.h"

namespace TransformationSchemas {

const char* kBboxClipName = "bboxClip";
const char* kBboxClipDescription = "Clip a feature to a bounding box.";
const char* kBboxClipSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "feature": {"type": ["string", "object"]},
        "bbox": {"type": ["string", "array"]}
    },
    "required": ["feature", "bbox"],
    "additionalProperties": false
})";

const char* kBezierSplineName = "bezierSpline";
const char* kBezierSplineDescription = "Convert a line to a smooth Bezier curve.";
const char* kBezierSplineSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "line": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["line"],
    "additionalProperties": false
})";

const char* kBufferName = "buffer";
const char* kBufferDescription = "Create a buffer around a GeoJSON feature.";
const char* kBufferSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "radius": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson", "radius"],
    "additionalProperties": false
})";

const char* kCircleName = "circle";
const char* kCircleDescription = "Create a circular polygon.";
const char* kCircleSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "center": {"type": ["string", "object", "array"]},
        "radius": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["center", "radius"],
    "additionalProperties": false
})";

const char* kCloneName = "clone";
const char* kCloneDescription = "Create a deep copy of a GeoJSON object.";
const char* kCloneSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

const char* kConcaveName = "concave";
const char* kConcaveDescription = "Calculate the concave hull of a point set.";
const char* kConcaveSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["points"],
    "additionalProperties": false
})";

const char* kConvexName = "convex";
const char* kConvexDescription = "Calculate the convex hull of a point set.";
const char* kConvexSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {"type": ["string", "object"]}
    },
    "required": ["points"],
    "additionalProperties": false
})";

const char* kDifferenceName = "difference";
const char* kDifferenceDescription = "Calculate the difference of two polygons.";
const char* kDifferenceSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "featureCollection": {"type": ["string", "object"]}
    },
    "required": ["featureCollection"],
    "additionalProperties": false
})";

const char* kDissolveName = "dissolve";
const char* kDissolveDescription = "Merge adjacent polygons.";
const char* kDissolveSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "featureCollection": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["featureCollection"],
    "additionalProperties": false
})";

const char* kIntersectName = "intersect";
const char* kIntersectDescription = "Calculate the intersection of two polygons.";
const char* kIntersectSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "featureCollection": {"type": ["string", "object"]}
    },
    "required": ["featureCollection"],
    "additionalProperties": false
})";

const char* kLineOffsetName = "lineOffset";
const char* kLineOffsetDescription = "Calculate an offset line.";
const char* kLineOffsetSchema = R"({
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

const char* kSimplifyName = "simplify";
const char* kSimplifyDescription = "Simplify a GeoJSON geometry.";
const char* kSimplifySchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson"],
    "additionalProperties": false
})";

const char* kTesselateName = "tesselate";
const char* kTesselateDescription = "Tessellate a polygon into triangles.";
const char* kTesselateSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "polygon": {"type": ["string", "object"]}
    },
    "required": ["polygon"],
    "additionalProperties": false
})";

const char* kTransformRotateName = "transformRotate";
const char* kTransformRotateDescription = "Rotate a GeoJSON object.";
const char* kTransformRotateSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "angle": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson", "angle"],
    "additionalProperties": false
})";

const char* kTransformTranslateName = "transformTranslate";
const char* kTransformTranslateDescription = "Translate (move) a GeoJSON object.";
const char* kTransformTranslateSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "distance": {"type": "number"},
        "direction": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson", "distance", "direction"],
    "additionalProperties": false
})";

const char* kTransformScaleName = "transformScale";
const char* kTransformScaleDescription = "Scale a GeoJSON object.";
const char* kTransformScaleSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson": {"type": ["string", "object"]},
        "factor": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["geojson", "factor"],
    "additionalProperties": false
})";

const char* kUnionName = "union";
const char* kUnionDescription = "Merge two polygons.";
const char* kUnionSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "featureCollection": {"type": ["string", "object"]}
    },
    "required": ["featureCollection"],
    "additionalProperties": false
})";

const char* kVoronoiName = "voronoi";
const char* kVoronoiDescription = "Generate Voronoi polygons from points.";
const char* kVoronoiSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["points"],
    "additionalProperties": false
})";

} // namespace TransformationSchemas
