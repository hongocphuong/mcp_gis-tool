#include "BooleanSchemas.h"

namespace BooleanSchemas {

// Clockwise
const char* kClockwiseName = "booleanClockwise";
const char* kClockwiseDescription = "Check if a ring is clockwise.";
const char* kClockwiseSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "ring": {"type": ["string", "array"]}
    },
    "required": ["ring"],
    "additionalProperties": false
})";

// Contains
const char* kContainsName = "booleanContains";
const char* kContainsDescription = "Check if the first geometry contains the second geometry.";
const char* kContainsSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

// Crosses
const char* kCrossesName = "booleanCrosses";
const char* kCrossesDescription = "Check if two geometries cross each other.";
const char* kCrossesSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

// Disjoint
const char* kDisjointName = "booleanDisjoint";
const char* kDisjointDescription = "Check if two geometries are disjoint (have no common points).";
const char* kDisjointSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

// Equal
const char* kEqualName = "booleanEqual";
const char* kEqualDescription = "Check if two geometries are equal.";
const char* kEqualSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

// Overlap
const char* kOverlapName = "booleanOverlap";
const char* kOverlapDescription = "Check if two geometries overlap.";
const char* kOverlapSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

// Parallel
const char* kParallelName = "booleanParallel";
const char* kParallelDescription = "Check if two line segments are parallel.";
const char* kParallelSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "line1": {"type": ["string", "object"]},
        "line2": {"type": ["string", "object"]}
    },
    "required": ["line1", "line2"],
    "additionalProperties": false
})";

// PointInPolygon
const char* kPointInPolygonName = "booleanPointInPolygon";
const char* kPointInPolygonDescription = "Check if a point is inside a polygon.";
const char* kPointInPolygonSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point":   {"type": ["string", "object", "array"]},
        "polygon": {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["point", "polygon"],
    "additionalProperties": false
})";

// PointOnLine
const char* kPointOnLineName = "booleanPointOnLine";
const char* kPointOnLineDescription = "Check if a point is on a line.";
const char* kPointOnLineSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "point":   {"type": ["string", "object", "array"]},
        "line":    {"type": ["string", "object"]},
        "options": {"type": ["string", "object"]}
    },
    "required": ["point", "line"],
    "additionalProperties": false
})";

// Within
const char* kWithinName = "booleanWithin";
const char* kWithinDescription = "Check if the first geometry is within the second geometry.";
const char* kWithinSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

} // namespace BooleanSchemas
