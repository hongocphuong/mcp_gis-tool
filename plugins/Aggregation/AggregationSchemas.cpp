#include "AggregationSchemas.h"

namespace AggregationSchemas {

const char kCollectName[] = "collect";
const char kCollectDescription[] = "Aggregate point properties into polygons.";

const char kCollectSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "polygons": {"type": ["string", "object"]},
        "points": {"type": ["string", "object"]},
        "in_field": {"type": "string"},
        "out_field": {"type": "string"}
    },
    "required": ["polygons", "points", "in_field", "out_field"],
    "additionalProperties": false
})";

const char kClustersDbscanName[] = "clustersDbscan";
const char kClustersDbscanDescription[] = "Cluster points using DBSCAN algorithm.";

const char kClustersDbscanSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {"type": ["string", "object"]},
        "max_distance": {"type": "number"},
        "options": {"type": ["string", "object"]}
    },
    "required": ["points", "max_distance"],
    "additionalProperties": false
})";

const char kClustersKmeansName[] = "clustersKmeans";
const char kClustersKmeansDescription[] = "Cluster points using K-means algorithm.";

const char kClustersKmeansSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "points": {"type": ["string", "object"]},
        "number_of_clusters": {"type": "integer", "minimum": 1},
        "options": {"type": ["string", "object"]}
    },
    "required": ["points", "number_of_clusters"],
    "additionalProperties": false
})";

} // namespace AggregationSchemas
