#include "BooleanTools.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "geojson/GeoJsonTraversal.h"
#include "GisTypes.h"

namespace {

// ---------------------------------------------------------------------------
// JSON schemas
// ---------------------------------------------------------------------------

constexpr char kRingSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "ring": {"type": ["string", "array"]}
    },
    "required": ["ring"],
    "additionalProperties": false
})";

constexpr char kTwoGeomSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "geojson1": {"type": ["string", "object"]},
        "geojson2": {"type": ["string", "object"]}
    },
    "required": ["geojson1", "geojson2"],
    "additionalProperties": false
})";

constexpr char kTwoLineSchema[] = R"({
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "line1": {"type": ["string", "object"]},
        "line2": {"type": ["string", "object"]}
    },
    "required": ["line1", "line2"],
    "additionalProperties": false
})";

constexpr char kPointInPolygonSchema[] = R"({
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

constexpr char kPointOnLineSchema[] = R"({
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

// ---------------------------------------------------------------------------
// Argument parsing helpers (mirrors MeasurementTools pattern)
// ---------------------------------------------------------------------------

json ParseJsonArg(const json& arguments, const std::string& key) {
    if (!arguments.contains(key)) {
        throw std::invalid_argument("Missing required argument: " + key);
    }
    const auto& val = arguments[key];
    if (val.is_string()) {
        auto parsed = json::parse(val.get<std::string>(), nullptr, false);
        if (parsed.is_discarded()) {
            throw std::invalid_argument("Argument '" + key + "' is not valid JSON.");
        }
        return parsed;
    }
    return val;
}

json ParseOptionsArg(const json& arguments) {
    if (!arguments.contains("options")) return json::object();
    const auto& val = arguments["options"];
    if (val.is_string()) {
        auto parsed = json::parse(val.get<std::string>(), nullptr, false);
        if (!parsed.is_discarded() && parsed.is_object()) return parsed;
        return json::object();
    }
    return val.is_object() ? val : json::object();
}

// Unwrap Feature wrapper → bare geometry
json ExtractGeometry(const json& node) {
    if (!node.is_object()) return node;
    if (node.contains("type") && node["type"] == "Feature") {
        if (!node.contains("geometry") || node["geometry"].is_null()) {
            throw std::invalid_argument("Feature has null geometry.");
        }
        return node["geometry"];
    }
    return node;
}

std::string GeomType(const json& geom) {
    if (!geom.is_object() || !geom.contains("type") || !geom["type"].is_string()) {
        throw std::invalid_argument("GeoJSON object must have a string 'type' field.");
    }
    return geom["type"].get<std::string>();
}


PlanarPoint ExtractPoint(const json& geom) {
    const auto& c = geom.at("coordinates");
    return {c[0].get<double>(), c[1].get<double>()};
}

std::vector<PlanarPoint> ParsePtArray(const json& arr) {
    if (!arr.is_array()) {
        throw std::invalid_argument("Expected an array of coordinate pairs.");
    }
    std::vector<PlanarPoint> pts;
    pts.reserve(arr.size());
    for (const auto& c : arr) {
        if (!c.is_array() || c.size() < 2) {
            throw std::invalid_argument("Each coordinate must be [x, y].");
        }
        pts.push_back({c[0].get<double>(), c[1].get<double>()});
    }
    return pts;
}

std::vector<PlanarPoint> ExtractLineCoords(const json& geom) {
    return ParsePtArray(geom.at("coordinates"));
}

// ---------------------------------------------------------------------------
// Core geometry algorithms
// ---------------------------------------------------------------------------

// Shoelace signed-area sum: > 0 → clockwise (GeoJSON / Turf.js convention)
double RingSignedSum(const std::vector<PlanarPoint>& ring) {
    double sum = 0.0;
    const size_t n = ring.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        sum += (ring[i + 1].x - ring[i].x) * (ring[i + 1].y + ring[i].y);
    }
    return sum;
}

// Point-in-ring ray casting
bool PointInRing(const PlanarPoint& p, const std::vector<PlanarPoint>& ring) {
    bool inside = false;
    const size_t n = ring.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const PlanarPoint& pi = ring[i];
        const PlanarPoint& pj = ring[j];
        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Point collinear and within segment bounding box
bool PointOnSegment(const PlanarPoint& p, const PlanarPoint& a, const PlanarPoint& b) {
    if (p.x < std::min(a.x, b.x) || p.x > std::max(a.x, b.x)) return false;
    if (p.y < std::min(a.y, b.y) || p.y > std::max(a.y, b.y)) return false;
    const double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    return std::fabs(cross) < 1e-10;
}

bool PointOnRingBoundary(const PlanarPoint& p, const std::vector<PlanarPoint>& ring) {
    const size_t n = ring.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (PointOnSegment(p, ring[j], ring[i])) return true;
    }
    return false;
}

// Full point-in-polygon including hole subtraction
bool PointInPolygon(const PlanarPoint& p, const PolygonRings& poly, bool ignoreBoundary) {
    std::vector<PlanarPoint> outer;
    outer.reserve(poly.outerRing.size());
    for (const auto& c : poly.outerRing) outer.push_back({c.lon, c.lat});

    if (PointOnRingBoundary(p, outer)) return !ignoreBoundary;
    if (!PointInRing(p, outer)) return false;

    for (const auto& hole : poly.holes) {
        std::vector<PlanarPoint> holePts;
        holePts.reserve(hole.size());
        for (const auto& c : hole) holePts.push_back({c.lon, c.lat});
        if (PointOnRingBoundary(p, holePts)) return !ignoreBoundary;
        if (PointInRing(p, holePts)) return false;
    }
    return true;
}

// Segment–segment proper crossing (interior points only)
bool SegmentsProperCross(const PlanarPoint& a, const PlanarPoint& b, const PlanarPoint& c, const PlanarPoint& d) {
    const double d1x = b.x - a.x, d1y = b.y - a.y;
    const double d2x = d.x - c.x, d2y = d.y - c.y;
    const double denom = d1x * d2y - d1y * d2x;
    if (std::fabs(denom) < 1e-14) return false;
    const double t = ((c.x - a.x) * d2y - (c.y - a.y) * d2x) / denom;
    const double u = ((c.x - a.x) * d1y - (c.y - a.y) * d1x) / denom;
    return t > 1e-10 && t < 1.0 - 1e-10 && u > 1e-10 && u < 1.0 - 1e-10;
}

// Segment–segment intersection including boundary touches
bool SegmentsIntersect(const PlanarPoint& a, const PlanarPoint& b, const PlanarPoint& c, const PlanarPoint& d) {
    const double d1x = b.x - a.x, d1y = b.y - a.y;
    const double d2x = d.x - c.x, d2y = d.y - c.y;
    const double denom = d1x * d2y - d1y * d2x;
    if (std::fabs(denom) < 1e-14) {
        return PointOnSegment(a, c, d) || PointOnSegment(b, c, d) ||
               PointOnSegment(c, a, b) || PointOnSegment(d, a, b);
    }
    const double t = ((c.x - a.x) * d2y - (c.y - a.y) * d2x) / denom;
    const double u = ((c.x - a.x) * d1y - (c.y - a.y) * d1x) / denom;
    return t >= -1e-10 && t <= 1.0 + 1e-10 && u >= -1e-10 && u <= 1.0 + 1e-10;
}

// Extract all edge segments from Line/Polygon geometries
std::vector<std::pair<PlanarPoint, PlanarPoint>> GetSegments(const json& geom) {
    std::vector<std::pair<PlanarPoint, PlanarPoint>> segs;
    const std::string type = GeomType(geom);

    auto addSegs = [&](const json& coordArr) {
        const auto pts = ParsePtArray(coordArr);
        for (size_t i = 1; i < pts.size(); ++i) {
            segs.push_back({pts[i - 1], pts[i]});
        }
    };

    if (type == "LineString") {
        addSegs(geom.at("coordinates"));
    } else if (type == "MultiLineString") {
        for (const auto& line : geom.at("coordinates")) addSegs(line);
    } else if (type == "Polygon") {
        for (const auto& ring : geom.at("coordinates")) addSegs(ring);
    } else if (type == "MultiPolygon") {
        for (const auto& poly : geom.at("coordinates"))
            for (const auto& ring : poly) addSegs(ring);
    }
    return segs;
}

// Generic intersects test for any two geometries
bool GeometriesIntersect(const json& geom1, const json& geom2) {
    const std::string type1 = GeomType(geom1);
    const std::string type2 = GeomType(geom2);

    // Point vs …
    if (type1 == "Point") {
        const PlanarPoint p= ExtractPoint(geom1);
        if (type2 == "Point") {
            const PlanarPoint p2= ExtractPoint(geom2);
            return std::fabs(p.x - p2.x) < 1e-12 && std::fabs(p.y - p2.y) < 1e-12;
        }
        if (type2 == "LineString" || type2 == "MultiLineString") {
            for (const auto& s : GetSegments(geom2))
                if (PointOnSegment(p, s.first, s.second)) return true;
            return false;
        }
        if (type2 == "Polygon" || type2 == "MultiPolygon") {
            for (const auto& poly : CollectPolygons(geom2))
                if (PointInPolygon(p, poly, false)) return true;
            return false;
        }
    }

    // Reverse Point
    if (type2 == "Point") return GeometriesIntersect(geom2, geom1);

    // Shared segment-intersection check covers Line×Line, Line×Polygon, Polygon×Polygon
    const auto segs1 = GetSegments(geom1);
    const auto segs2 = GetSegments(geom2);
    for (const auto& s1 : segs1)
        for (const auto& s2 : segs2)
            if (SegmentsIntersect(s1.first, s1.second, s2.first, s2.second)) return true;

    // One geometry might be fully inside the other (no boundary crossing)
    if (!segs2.empty()) {
        const PlanarPoint probe= segs2[0].first;
        for (const auto& poly : CollectPolygons(geom1))
            if (PointInPolygon(probe, poly, false)) return true;
    }
    if (!segs1.empty()) {
        const PlanarPoint probe= segs1[0].first;
        for (const auto& poly : CollectPolygons(geom2))
            if (PointInPolygon(probe, poly, false)) return true;
    }

    return false;
}

// True if every coordinate of geom2 lies inside geom1 (geom1 must be polygon)
bool GeomContainsAllCoords(const json& geom1, const json& geom2) {
    const std::string type1 = GeomType(geom1);
    if (type1 != "Polygon" && type1 != "MultiPolygon") return false;

    const auto polys = CollectPolygons(geom1);
    const auto coords = CollectCoordinates(geom2);
    if (coords.empty()) return false;

    for (const auto& c : coords) {
        const PlanarPoint p= {c.lon, c.lat};
        bool insideAny = false;
        for (const auto& poly : polys) {
            if (PointInPolygon(p, poly, false)) { insideAny = true; break; }
        }
        if (!insideAny) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Tool handlers
// ---------------------------------------------------------------------------

ToolResult BooleanClockwise(const json& arguments) {
    const auto ring = ParsePtArray(ParseJsonArg(arguments, "ring"));
    return {
        "Checked ring winding order.",
        {{"value", RingSignedSum(ring) > 0.0}}
    };
}

ToolResult BooleanContains(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));
    return {
        "Checked if geojson1 contains geojson2.",
        {{"value", GeomContainsAllCoords(g1, g2)}}
    };
}

ToolResult BooleanCrosses(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));
    const std::string type1 = GeomType(g1);
    const std::string type2 = GeomType(g2);

    bool result = false;

    // Line × Line: segments must cross at interior points
    if ((type1 == "LineString" || type1 == "MultiLineString") &&
        (type2 == "LineString" || type2 == "MultiLineString")) {
        for (const auto& s1 : GetSegments(g1)) {
            for (const auto& s2 : GetSegments(g2)) {
                if (SegmentsProperCross(s1.first, s1.second, s2.first, s2.second)) {
                    result = true;
                    break;
                }
            }
            if (result) break;
        }
    }
    // Line × Polygon: some vertices inside, some outside
    else if ((type1 == "LineString" || type1 == "MultiLineString") &&
             (type2 == "Polygon" || type2 == "MultiPolygon")) {
        const auto polys = CollectPolygons(g2);
        bool someIn = false, someOut = false;
        for (const auto& c : CollectCoordinates(g1)) {
            const PlanarPoint p= {c.lon, c.lat};
            bool inside = false;
            for (const auto& poly : polys)
                if (PointInPolygon(p, poly, true)) { inside = true; break; }
            if (inside) someIn = true; else someOut = true;
        }
        result = someIn && someOut;
    }
    // Polygon × Line: reverse
    else if ((type2 == "LineString" || type2 == "MultiLineString") &&
             (type1 == "Polygon" || type1 == "MultiPolygon")) {
        const auto polys = CollectPolygons(g1);
        bool someIn = false, someOut = false;
        for (const auto& c : CollectCoordinates(g2)) {
            const PlanarPoint p= {c.lon, c.lat};
            bool inside = false;
            for (const auto& poly : polys)
                if (PointInPolygon(p, poly, true)) { inside = true; break; }
            if (inside) someIn = true; else someOut = true;
        }
        result = someIn && someOut;
    }

    return {"Checked if geometries cross.", {{"value", result}}};
}

ToolResult BooleanDisjoint(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));
    return {
        "Checked if geometries are disjoint.",
        {{"value", !GeometriesIntersect(g1, g2)}}
    };
}

ToolResult BooleanEqual(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));

    if (GeomType(g1) != GeomType(g2)) {
        return {"Checked geometry equality.", {{"value", false}}};
    }

    const auto c1 = CollectCoordinates(g1);
    const auto c2 = CollectCoordinates(g2);
    if (c1.size() != c2.size()) {
        return {"Checked geometry equality.", {{"value", false}}};
    }

    for (size_t i = 0; i < c1.size(); ++i) {
        if (std::fabs(c1[i].lon - c2[i].lon) > 1e-10 ||
            std::fabs(c1[i].lat - c2[i].lat) > 1e-10) {
            return {"Checked geometry equality.", {{"value", false}}};
        }
    }

    return {"Checked geometry equality.", {{"value", true}}};
}

ToolResult BooleanOverlap(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));

    if (!GeometriesIntersect(g1, g2)) {
        return {"Checked if geometries overlap.", {{"value", false}}};
    }

    const bool aContainsB = GeomContainsAllCoords(g1, g2);
    const bool bContainsA = GeomContainsAllCoords(g2, g1);

    return {
        "Checked if geometries overlap.",
        {{"value", !aContainsB && !bContainsA}}
    };
}

ToolResult BooleanParallel(const json& arguments) {
    const auto l1 = ExtractGeometry(ParseJsonArg(arguments, "line1"));
    const auto l2 = ExtractGeometry(ParseJsonArg(arguments, "line2"));

    if (GeomType(l1) != "LineString" || GeomType(l2) != "LineString") {
        throw std::invalid_argument("line1 and line2 must both be LineString geometries.");
    }

    const auto pts1 = ExtractLineCoords(l1);
    const auto pts2 = ExtractLineCoords(l2);
    if (pts1.size() < 2 || pts2.size() < 2) {
        throw std::invalid_argument("LineString must have at least 2 coordinates.");
    }

    const double dx1 = pts1.back().x - pts1.front().x;
    const double dy1 = pts1.back().y - pts1.front().y;
    const double dx2 = pts2.back().x - pts2.front().x;
    const double dy2 = pts2.back().y - pts2.front().y;

    const double mag1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    const double mag2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    if (mag1 < 1e-14 || mag2 < 1e-14) {
        throw std::invalid_argument("LineString has zero length.");
    }

    // |sin θ| = |cross| / (mag1 * mag2); parallel when ≈ 0
    const double sinTheta = std::fabs(dx1 * dy2 - dy1 * dx2) / (mag1 * mag2);

    return {
        "Checked if lines are parallel.",
        {{"value", sinTheta < 1e-8}}
    };
}

ToolResult BooleanPointInPolygon(const json& arguments) {
    const auto pointGeom = ExtractGeometry(ParseJsonArg(arguments, "point"));
    const auto polyGeom  = ExtractGeometry(ParseJsonArg(arguments, "polygon"));
    const auto options   = ParseOptionsArg(arguments);

    if (GeomType(pointGeom) != "Point") {
        throw std::invalid_argument("point must be a GeoJSON Point geometry.");
    }

    const bool ignoreBoundary = options.contains("ignoreBoundary") &&
                                options["ignoreBoundary"].is_boolean() &&
                                options["ignoreBoundary"].get<bool>();

    const PlanarPoint p= ExtractPoint(pointGeom);
    bool result = false;
    for (const auto& poly : CollectPolygons(polyGeom)) {
        if (PointInPolygon(p, poly, ignoreBoundary)) { result = true; break; }
    }

    return {
        "Checked if point is inside polygon.",
        {{"value", result}}
    };
}

ToolResult BooleanPointOnLine(const json& arguments) {
    const auto pointGeom = ExtractGeometry(ParseJsonArg(arguments, "point"));
    const auto lineGeom  = ExtractGeometry(ParseJsonArg(arguments, "line"));
    const auto options   = ParseOptionsArg(arguments);

    if (GeomType(pointGeom) != "Point") {
        throw std::invalid_argument("point must be a GeoJSON Point geometry.");
    }

    const std::string lineType = GeomType(lineGeom);
    if (lineType != "LineString" && lineType != "MultiLineString") {
        throw std::invalid_argument("line must be a LineString or MultiLineString geometry.");
    }

    const bool ignoreEndVertices = options.contains("ignoreEndVertices") &&
                                   options["ignoreEndVertices"].is_boolean() &&
                                   options["ignoreEndVertices"].get<bool>();

    const PlanarPoint p= ExtractPoint(pointGeom);

    // Collect global endpoints for ignoreEndVertices check
    std::vector<PlanarPoint> endpoints;
    if (ignoreEndVertices) {
        auto addEndpts = [&](const json& coordArr) {
            const auto pts = ParsePtArray(coordArr);
            if (pts.size() >= 2) {
                endpoints.push_back(pts.front());
                endpoints.push_back(pts.back());
            }
        };
        if (lineType == "LineString") {
            addEndpts(lineGeom.at("coordinates"));
        } else {
            for (const auto& line : lineGeom.at("coordinates")) addEndpts(line);
        }
    }

    for (const auto& s : GetSegments(lineGeom)) {
        if (!PointOnSegment(p, s.first, s.second)) continue;
        if (ignoreEndVertices) {
            bool isEndpt = false;
            for (const auto& ep : endpoints) {
                if (std::fabs(p.x - ep.x) < 1e-12 && std::fabs(p.y - ep.y) < 1e-12) {
                    isEndpt = true;
                    break;
                }
            }
            if (isEndpt) continue;
        }
        return {"Checked if point is on line.", {{"value", true}}};
    }

    return {"Checked if point is on line.", {{"value", false}}};
}

ToolResult BooleanWithin(const json& arguments) {
    const auto g1 = ExtractGeometry(ParseJsonArg(arguments, "geojson1"));
    const auto g2 = ExtractGeometry(ParseJsonArg(arguments, "geojson2"));
    return {
        "Checked if geojson1 is within geojson2.",
        {{"value", GeomContainsAllCoords(g2, g1)}}
    };
}

} // namespace

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void RegisterBooleanTools(ToolRegistry& registry) {
    registry.Register({
        {"booleanClockwise", "Check if a ring is clockwise.", kRingSchema},
        BooleanClockwise
    });
    registry.Register({
        {"booleanContains", "Check if the first geometry contains the second geometry.", kTwoGeomSchema},
        BooleanContains
    });
    registry.Register({
        {"booleanCrosses", "Check if two geometries cross each other.", kTwoGeomSchema},
        BooleanCrosses
    });
    registry.Register({
        {"booleanDisjoint", "Check if two geometries are disjoint (have no common points).", kTwoGeomSchema},
        BooleanDisjoint
    });
    registry.Register({
        {"booleanEqual", "Check if two geometries are equal.", kTwoGeomSchema},
        BooleanEqual
    });
    registry.Register({
        {"booleanOverlap", "Check if two geometries overlap.", kTwoGeomSchema},
        BooleanOverlap
    });
    registry.Register({
        {"booleanParallel", "Check if two line segments are parallel.", kTwoLineSchema},
        BooleanParallel
    });
    registry.Register({
        {"booleanPointInPolygon", "Check if a point is inside a polygon.", kPointInPolygonSchema},
        BooleanPointInPolygon
    });
    registry.Register({
        {"booleanPointOnLine", "Check if a point is on a line.", kPointOnLineSchema},
        BooleanPointOnLine
    });
    registry.Register({
        {"booleanWithin", "Check if the first geometry is within the second geometry.", kTwoGeomSchema},
        BooleanWithin
    });
}
