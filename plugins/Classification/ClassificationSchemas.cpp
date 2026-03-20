#include "ClassificationSchemas.h"

namespace ClassificationSchemas {

const char* kNearestPointName = "nearestPoint";

const char* kNearestPointDescription =
    "Find the nearest point to a target point.";

const char* kNearestPointSchema = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "target_point": {"type": ["string", "object"]},
        "points": {"type": ["string", "object"]}
    },
    "required": ["target_point", "points"],
    "additionalProperties": false
})";

} // namespace ClassificationSchemas
