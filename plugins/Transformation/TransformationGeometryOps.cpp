#include "TransformationGeometryOps.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "GeoAlgorithms/GeoAlgorithms.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"
#include "GisTypes.h"
#include "GeoMath/GeoMath.h"
#include "FeatureBuilders/FeatureBuilders.h"

namespace TransformationGeometryOps {

namespace {

using json = nlohmann::json;
using GeoMath::kEpsilon;
using GeoJsonParsingUtils::ParseCoordinatePair;

// Local: convert PlanarPoint to a json [x, y] coordinate array
json ToJsonPt(const PlanarPoint& pt) {
    return json::array({pt.x, pt.y});
}

json ParseFeatureCollection(const json& node, const std::string& fieldName) {
    json fc = node;
    if (fc.is_string()) {
        fc = GeoJsonParsingUtils::ParseJsonString(fc.get<std::string>(), fieldName);
    }
    return GeoJsonParsingUtils::ParseFeatureCollectionLike(fc, fieldName);
}

json ParseFeatureGeometry(const json& feature) {
    if (!feature.is_object() || !feature.contains("type") || feature["type"] != "Feature") {
        throw std::invalid_argument("Input must be a GeoJSON Feature.");
    }

    if (!feature.contains("geometry") || feature["geometry"].is_null()) {
        throw std::invalid_argument("Feature must contain non-null geometry.");
    }

    if (!feature["geometry"].contains("type") || !feature["geometry"]["type"].is_string()) {
        throw std::invalid_argument("Feature geometry must contain a string type.");
    }

    return feature["geometry"];
}

std::vector<PlanarPoint> ParseRing(const json& ringNode) {
    if (!ringNode.is_array() || ringNode.size() < 4) {
        throw std::invalid_argument("Polygon ring must contain at least 4 coordinates.");
    }

    std::vector<PlanarPoint> ring;
    ring.reserve(ringNode.size());
    for (const auto& coord : ringNode) {
        ring.push_back(ParseCoordinatePair(coord));
    }

    if (std::abs(ring.front().x - ring.back().x) > kEpsilon || std::abs(ring.front().y - ring.back().y) > kEpsilon) {
        ring.push_back(ring.front());
    }

    return ring;
}

bool IsClosed(const std::vector<PlanarPoint>& ring) {
    if (ring.size() < 2) {
        return false;
    }
    return std::abs(ring.front().x - ring.back().x) <= kEpsilon &&
           std::abs(ring.front().y - ring.back().y) <= kEpsilon;
}


std::vector<PlanarPoint> EnsureClosedRing(std::vector<PlanarPoint> ring) {
    if (ring.empty()) {
        return ring;
    }

    if (!IsClosed(ring)) {
        ring.push_back(ring.front());
    }

    return ring;
}

std::vector<PlanarPoint> OpenRing(const std::vector<PlanarPoint>& closedRing) {
    if (closedRing.empty()) {
        return {};
    }

    std::vector<PlanarPoint> out = closedRing;
    if (IsClosed(out)) {
        out.pop_back();
    }
    return out;
}

PlanarPoint InterpolateX(const PlanarPoint& a, const PlanarPoint& b, double x) {
    const double dx = b.x - a.x;
    if (std::abs(dx) < kEpsilon) {
        return {x, a.y};
    }
    const double t = (x - a.x) / dx;
    return {x, a.y + t * (b.y - a.y)};
}

PlanarPoint InterpolateY(const PlanarPoint& a, const PlanarPoint& b, double y) {
    const double dy = b.y - a.y;
    if (std::abs(dy) < kEpsilon) {
        return {a.x, y};
    }
    const double t = (y - a.y) / dy;
    return {a.x + t * (b.x - a.x), y};
}

std::vector<PlanarPoint> ClipRingAgainstEdge(const std::vector<PlanarPoint>& ring, int edge, double value) {
    auto inside = [edge, value](const PlanarPoint& p) {
        if (edge == 0) return p.x >= value - kEpsilon;
        if (edge == 1) return p.x <= value + kEpsilon;
        if (edge == 2) return p.y >= value - kEpsilon;
        return p.y <= value + kEpsilon;
    };

    auto intersect = [edge, value](const PlanarPoint& a, const PlanarPoint& b) {
        if (edge == 0 || edge == 1) {
            return InterpolateX(a, b, value);
        }
        return InterpolateY(a, b, value);
    };

    std::vector<PlanarPoint> out;
    if (ring.empty()) {
        return out;
    }

    PlanarPoint prev = ring.back();
    bool prevIn = inside(prev);

    for (const auto& curr : ring) {
        const bool currIn = inside(curr);
        if (currIn) {
            if (!prevIn) {
                out.push_back(intersect(prev, curr));
            }
            out.push_back(curr);
        } else if (prevIn) {
            out.push_back(intersect(prev, curr));
        }

        prev = curr;
        prevIn = currIn;
    }

    return out;
}

std::vector<PlanarPoint> ClipRingToBbox(const std::vector<PlanarPoint>& inputRing, const Bbox& bbox) {
    std::vector<PlanarPoint> ring = EnsureClosedRing(inputRing);
    ring = OpenRing(ring);

    ring = ClipRingAgainstEdge(ring, 0, bbox.minX);
    ring = ClipRingAgainstEdge(ring, 1, bbox.maxX);
    ring = ClipRingAgainstEdge(ring, 2, bbox.minY);
    ring = ClipRingAgainstEdge(ring, 3, bbox.maxY);

    ring = EnsureClosedRing(ring);
    if (ring.size() < 4 || std::abs(GeoAlgorithms::ComputeRingSignedArea(ring)) <= kEpsilon) {
        return {};
    }

    return ring;
}

enum OutCode : unsigned {
    kInside = 0,
    kLeft = 1,
    kRight = 2,
    kBottom = 4,
    kTop = 8
};

unsigned ComputeOutCode(const PlanarPoint& p, const Bbox& b) {
    unsigned code = kInside;
    if (p.x < b.minX) code |= kLeft;
    else if (p.x > b.maxX) code |= kRight;

    if (p.y < b.minY) code |= kBottom;
    else if (p.y > b.maxY) code |= kTop;

    return code;
}

bool ClipSegmentToBbox(PlanarPoint p0, PlanarPoint p1, const Bbox& b, PlanarPoint* out0, PlanarPoint* out1) {
    unsigned code0 = ComputeOutCode(p0, b);
    unsigned code1 = ComputeOutCode(p1, b);

    while (true) {
        if ((code0 | code1) == 0) {
            *out0 = p0;
            *out1 = p1;
            return true;
        }

        if ((code0 & code1) != 0) {
            return false;
        }

        const unsigned codeOut = code0 != 0 ? code0 : code1;
        PlanarPoint p;

        if ((codeOut & kTop) != 0) {
            p = InterpolateY(p0, p1, b.maxY);
        } else if ((codeOut & kBottom) != 0) {
            p = InterpolateY(p0, p1, b.minY);
        } else if ((codeOut & kRight) != 0) {
            p = InterpolateX(p0, p1, b.maxX);
        } else {
            p = InterpolateX(p0, p1, b.minX);
        }

        if (codeOut == code0) {
            p0 = p;
            code0 = ComputeOutCode(p0, b);
        } else {
            p1 = p;
            code1 = ComputeOutCode(p1, b);
        }
    }
}

bool SamePoint(const PlanarPoint& a, const PlanarPoint& b) {
    return std::abs(a.x - b.x) <= 1e-10 && std::abs(a.y - b.y) <= 1e-10;
}

json LineSegmentsToGeometry(const std::vector<std::vector<PlanarPoint>>& parts, const std::string& preferType) {
    if (parts.empty()) {
        return nullptr;
    }

    if (parts.size() == 1 && preferType != "MultiLineString") {
        json coords = json::array();
        for (const auto& p : parts.front()) {
            coords.push_back(ToJsonPt(p));
        }

        return {
            {"type", "LineString"},
            {"coordinates", coords}
        };
    }

    json lines = json::array();
    for (const auto& part : parts) {
        if (part.size() < 2) {
            continue;
        }

        json line = json::array();
        for (const auto& p : part) {
            line.push_back(ToJsonPt(p));
        }
        lines.push_back(line);
    }

    if (lines.empty()) {
        return nullptr;
    }

    return {
        {"type", "MultiLineString"},
        {"coordinates", lines}
    };
}

json PolygonRingsToGeometry(const std::vector<std::vector<PlanarPoint>>& rings, const std::string& preferType) {
    if (rings.empty()) {
        return nullptr;
    }

    if (rings.size() == 1 && preferType != "MultiPolygon") {
        json ringJson = json::array();
        for (const auto& p : rings.front()) {
            ringJson.push_back(ToJsonPt(p));
        }

        return {
            {"type", "Polygon"},
            {"coordinates", json::array({ringJson})}
        };
    }

    json polys = json::array();
    for (const auto& ring : rings) {
        if (ring.size() < 4) {
            continue;
        }

        json ringJson = json::array();
        for (const auto& p : ring) {
            ringJson.push_back(ToJsonPt(p));
        }
        polys.push_back(json::array({ringJson}));
    }

    if (polys.empty()) {
        return nullptr;
    }

    return {
        {"type", "MultiPolygon"},
        {"coordinates", polys}
    };
}

std::vector<PlanarPoint> ParseOuterRing(const json& geometry) {
    if (!geometry.contains("type") || !geometry["type"].is_string()) {
        throw std::invalid_argument("Geometry must have string type.");
    }

    const std::string type = geometry["type"].get<std::string>();
    if (type == "Polygon") {
        if (!geometry.contains("coordinates") || !geometry["coordinates"].is_array() || geometry["coordinates"].empty()) {
            throw std::invalid_argument("Polygon must contain coordinates.");
        }
        return ParseRing(geometry["coordinates"][0]);
    }

    if (type == "MultiPolygon") {
        if (!geometry.contains("coordinates") || !geometry["coordinates"].is_array() || geometry["coordinates"].empty() ||
            !geometry["coordinates"][0].is_array() || geometry["coordinates"][0].empty()) {
            throw std::invalid_argument("MultiPolygon must contain coordinates.");
        }
        return ParseRing(geometry["coordinates"][0][0]);
    }

    throw std::invalid_argument("Geometry must be Polygon or MultiPolygon.");
}

std::vector<PlanarPoint> ConvexHull(const std::vector<PlanarPoint>& ptsIn) {
    std::vector<PlanarPoint> pts = ptsIn;
    if (pts.size() < 3) {
        return {};
    }

    std::sort(pts.begin(), pts.end(), [](const PlanarPoint& a, const PlanarPoint& b) {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });

    pts.erase(std::unique(pts.begin(), pts.end(), [](const PlanarPoint& a, const PlanarPoint& b) {
        return SamePoint(a, b);
    }), pts.end());

    if (pts.size() < 3) {
        return {};
    }

    auto cross = [](const PlanarPoint& a, const PlanarPoint& b, const PlanarPoint& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    std::vector<PlanarPoint> hull;
    hull.reserve(pts.size() * 2);

    for (const auto& p : pts) {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), p) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    const size_t lower = hull.size();
    for (size_t i = pts.size(); i-- > 0;) {
        const PlanarPoint& p = pts[i];
        while (hull.size() > lower && cross(hull[hull.size() - 2], hull.back(), p) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    if (!hull.empty()) {
        hull.pop_back();
    }

    hull = EnsureClosedRing(hull);
    if (hull.size() < 4) {
        return {};
    }

    return hull;
}

std::vector<PlanarPoint> IntersectConvexPolygons(const std::vector<PlanarPoint>& subjectClosed,
                                        const std::vector<PlanarPoint>& clipClosed) {
    std::vector<PlanarPoint> subject = OpenRing(subjectClosed);
    std::vector<PlanarPoint> clip = OpenRing(clipClosed);
    if (subject.size() < 3 || clip.size() < 3) {
        return {};
    }

    const double clipArea = GeoAlgorithms::ComputeRingSignedArea(EnsureClosedRing(clip));
    const bool clipCcw = clipArea > 0.0;

    auto inside = [clipCcw](const PlanarPoint& a, const PlanarPoint& b, const PlanarPoint& p) {
        const double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        return clipCcw ? (cross >= -kEpsilon) : (cross <= kEpsilon);
    };

    auto segmentIntersect = [](const PlanarPoint& s, const PlanarPoint& e, const PlanarPoint& a, const PlanarPoint& b) {
        const double x1 = s.x;
        const double y1 = s.y;
        const double x2 = e.x;
        const double y2 = e.y;
        const double x3 = a.x;
        const double y3 = a.y;
        const double x4 = b.x;
        const double y4 = b.y;

        const double den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        if (std::abs(den) < 1e-14) {
            return e;
        }

        const double px = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / den;
        const double py = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / den;
        return PlanarPoint{px, py};
    };

    for (size_t i = 0; i < clip.size(); ++i) {
        const PlanarPoint a = clip[i];
        const PlanarPoint b = clip[(i + 1) % clip.size()];

        std::vector<PlanarPoint> input = subject;
        subject.clear();
        if (input.empty()) {
            break;
        }

        PlanarPoint s = input.back();
        for (const PlanarPoint& e : input) {
            const bool eIn = inside(a, b, e);
            const bool sIn = inside(a, b, s);

            if (eIn) {
                if (!sIn) {
                    subject.push_back(segmentIntersect(s, e, a, b));
                }
                subject.push_back(e);
            } else if (sIn) {
                subject.push_back(segmentIntersect(s, e, a, b));
            }

            s = e;
        }
    }

    subject = EnsureClosedRing(subject);
    if (subject.size() < 4 || std::abs(GeoAlgorithms::ComputeRingSignedArea(subject)) <= kEpsilon) {
        return {};
    }

    return subject;
}

bool PointInConvexPolygon(const std::vector<PlanarPoint>& closedPoly, const PlanarPoint& p) {
    const std::vector<PlanarPoint> poly = OpenRing(closedPoly);
    if (poly.size() < 3) {
        return false;
    }

    bool hasPositive = false;
    bool hasNegative = false;

    for (size_t i = 0; i < poly.size(); ++i) {
        const PlanarPoint a = poly[i];
        const PlanarPoint b = poly[(i + 1) % poly.size()];
        const double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        if (cross > 1e-10) {
            hasPositive = true;
        } else if (cross < -1e-10) {
            hasNegative = true;
        }
        if (hasPositive && hasNegative) {
            return false;
        }
    }

    return true;
}

std::vector<PlanarPoint> ReverseRing(const std::vector<PlanarPoint>& closedRing) {
    std::vector<PlanarPoint> out = OpenRing(closedRing);
    std::reverse(out.begin(), out.end());
    return EnsureClosedRing(out);
}

json BuildFeatureFromRing(const std::vector<PlanarPoint>& ring, const json& properties = json::object()) {
    if (ring.empty()) {
        return nullptr;
    }

    json ringJson = json::array();
    for (const auto& p : ring) {
        ringJson.push_back(ToJsonPt(p));
    }

    return {
        {"type", "Feature"},
        {"properties", properties},
        {"geometry", {
            {"type", "Polygon"},
            {"coordinates", json::array({ringJson})}
        }}
    };
}

json ParseOptions(const json& optionsNode) {
    if (optionsNode.is_null()) {
        return json::object();
    }

    json options = optionsNode;
    if (options.is_string()) {
        options = GeoJsonParsingUtils::ParseJsonString(options.get<std::string>(), "options");
    }

    if (!options.is_object()) {
        throw std::invalid_argument("options must be a JSON object.");
    }

    return options;
}

std::pair<std::vector<PlanarPoint>, std::vector<PlanarPoint>> ParseTwoPolygonRings(const json& featureCollectionNode) {
    const json fc = ParseFeatureCollection(featureCollectionNode, "featureCollection");
    if (fc["features"].size() != 2) {
        throw std::invalid_argument("featureCollection must contain exactly two polygon features.");
    }

    const std::vector<PlanarPoint> a = ParseOuterRing(ParseFeatureGeometry(fc["features"][0]));
    const std::vector<PlanarPoint> b = ParseOuterRing(ParseFeatureGeometry(fc["features"][1]));
    return {a, b};
}

std::vector<PlanarPoint> ParseVoronoiCellSeed(const Bbox& bbox) {
    return {
        {bbox.minX, bbox.minY},
        {bbox.maxX, bbox.minY},
        {bbox.maxX, bbox.maxY},
        {bbox.minX, bbox.maxY}
    };
}

std::vector<PlanarPoint> ClipConvexPolyHalfPlane(const std::vector<PlanarPoint>& polygon,
                                        double a,
                                        double b,
                                        double c) {
    std::vector<PlanarPoint> out;
    if (polygon.empty()) {
        return out;
    }

    auto inside = [a, b, c](const PlanarPoint& p) {
        return a * p.x + b * p.y <= c + 1e-10;
    };

    auto intersect = [a, b, c](const PlanarPoint& p1, const PlanarPoint& p2) {
        const double dx = p2.x - p1.x;
        const double dy = p2.y - p1.y;
        const double den = a * dx + b * dy;
        if (std::abs(den) < 1e-14) {
            return p2;
        }

        const double t = (c - a * p1.x - b * p1.y) / den;
        return PlanarPoint{p1.x + t * dx, p1.y + t * dy};
    };

    PlanarPoint prev = polygon.back();
    bool prevIn = inside(prev);

    for (const auto& curr : polygon) {
        const bool currIn = inside(curr);
        if (currIn) {
            if (!prevIn) {
                out.push_back(intersect(prev, curr));
            }
            out.push_back(curr);
        } else if (prevIn) {
            out.push_back(intersect(prev, curr));
        }

        prev = curr;
        prevIn = currIn;
    }

    return out;
}

} // namespace

json ClipFeatureToBbox(const json& feature, const json& bboxNode) {
    const json geometry = ParseFeatureGeometry(feature);
    const Bbox bbox = FeatureBuilders::ParseBbox(bboxNode);

    const std::string type = geometry["type"].get<std::string>();
    json output = feature;

    if (type == "LineString" || type == "MultiLineString") {
        std::vector<std::vector<PlanarPoint>> parts;

        auto clipOneLine = [&parts, &bbox](const json& lineCoords) {
            if (!lineCoords.is_array() || lineCoords.size() < 2) {
                return;
            }

            std::vector<PlanarPoint> current;
            for (size_t i = 0; i + 1 < lineCoords.size(); ++i) {
                const PlanarPoint p0 = ParseCoordinatePair(lineCoords[i]);
                const PlanarPoint p1 = ParseCoordinatePair(lineCoords[i + 1]);

                PlanarPoint c0;
                PlanarPoint c1;
                if (!ClipSegmentToBbox(p0, p1, bbox, &c0, &c1)) {
                    if (current.size() >= 2) {
                        parts.push_back(current);
                    }
                    current.clear();
                    continue;
                }

                if (current.empty()) {
                    current.push_back(c0);
                    current.push_back(c1);
                } else {
                    if (!SamePoint(current.back(), c0)) {
                        parts.push_back(current);
                        current.clear();
                        current.push_back(c0);
                    }
                    if (!SamePoint(current.back(), c1)) {
                        current.push_back(c1);
                    }
                }
            }

            if (current.size() >= 2) {
                parts.push_back(current);
            }
        };

        if (type == "LineString") {
            clipOneLine(geometry["coordinates"]);
            output["geometry"] = LineSegmentsToGeometry(parts, "LineString");
        } else {
            for (const auto& lineCoords : geometry["coordinates"]) {
                clipOneLine(lineCoords);
            }
            output["geometry"] = LineSegmentsToGeometry(parts, "MultiLineString");
        }

        if (output["geometry"].is_null()) {
            output["geometry"] = nullptr;
        }
        return output;
    }

    if (type == "Polygon" || type == "MultiPolygon") {
        std::vector<std::vector<PlanarPoint>> rings;

        auto clipOnePolygon = [&rings, &bbox](const json& polyCoords) {
            if (!polyCoords.is_array() || polyCoords.empty()) {
                return;
            }
            const std::vector<PlanarPoint> outer = ParseRing(polyCoords[0]);
            const std::vector<PlanarPoint> clipped = ClipRingToBbox(outer, bbox);
            if (!clipped.empty()) {
                rings.push_back(clipped);
            }
        };

        if (type == "Polygon") {
            clipOnePolygon(geometry["coordinates"]);
            output["geometry"] = PolygonRingsToGeometry(rings, "Polygon");
        } else {
            for (const auto& polyCoords : geometry["coordinates"]) {
                clipOnePolygon(polyCoords);
            }
            output["geometry"] = PolygonRingsToGeometry(rings, "MultiPolygon");
        }

        if (output["geometry"].is_null()) {
            output["geometry"] = nullptr;
        }
        return output;
    }

    throw std::invalid_argument("bboxClip supports LineString, MultiLineString, Polygon, and MultiPolygon.");
}

json PolygonUnion(const json& featureCollection) {
    const auto [a, b] = ParseTwoPolygonRings(featureCollection);

    std::vector<PlanarPoint> points = OpenRing(a);
    const std::vector<PlanarPoint> bOpen = OpenRing(b);
    points.insert(points.end(), bOpen.begin(), bOpen.end());

    const std::vector<PlanarPoint> hull = ConvexHull(points);
    if (hull.empty()) {
        throw std::invalid_argument("Union result is empty.");
    }

    return BuildFeatureFromRing(hull);
}

json PolygonIntersect(const json& featureCollection) {
    const auto [a, b] = ParseTwoPolygonRings(featureCollection);
    const std::vector<PlanarPoint> aHull = ConvexHull(OpenRing(a));
    const std::vector<PlanarPoint> bHull = ConvexHull(OpenRing(b));

    if (aHull.empty() || bHull.empty()) {
        return nullptr;
    }

    const std::vector<PlanarPoint> inter = IntersectConvexPolygons(aHull, bHull);
    if (inter.empty()) {
        return nullptr;
    }

    return BuildFeatureFromRing(inter);
}

json PolygonDifference(const json& featureCollection) {
    const auto [a, b] = ParseTwoPolygonRings(featureCollection);
    const std::vector<PlanarPoint> aHull = ConvexHull(OpenRing(a));
    const std::vector<PlanarPoint> bHull = ConvexHull(OpenRing(b));

    if (aHull.empty()) {
        throw std::invalid_argument("Difference result is empty.");
    }

    if (bHull.empty()) {
        return BuildFeatureFromRing(aHull);
    }

    const std::vector<PlanarPoint> inter = IntersectConvexPolygons(aHull, bHull);
    if (inter.empty()) {
        return BuildFeatureFromRing(aHull);
    }

    // Convex approximation strategy:
    // - if intersection is strictly interior to A, represent A - B as A with one hole.
    // - otherwise (edge-overlap/non-convex remainder), fall back to A hull.
    bool interior = true;
    const std::vector<PlanarPoint> interOpen = OpenRing(inter);
    for (const auto& point : interOpen) {
        if (!PointInConvexPolygon(aHull, point)) {
            interior = false;
            break;
        }
    }

    if (!interior) {
        return BuildFeatureFromRing(aHull);
    }

    json outer = json::array();
    for (const auto& p : aHull) {
        outer.push_back(ToJsonPt(p));
    }

    const std::vector<PlanarPoint> hole = ReverseRing(inter);
    json inner = json::array();
    for (const auto& p : hole) {
        inner.push_back(ToJsonPt(p));
    }

    return {
        {"type", "Feature"},
        {"properties", json::object()},
        {"geometry", {
            {"type", "Polygon"},
            {"coordinates", json::array({outer, inner})}
        }}
    };
}

json PolygonDissolve(const json& featureCollection, const json& optionsNode) {
    const json fc = ParseFeatureCollection(featureCollection, "featureCollection");
    const json options = ParseOptions(optionsNode);

    std::string propertyName;
    if (options.contains("propertyName")) {
        if (!options["propertyName"].is_string()) {
            throw std::invalid_argument("options.propertyName must be a string.");
        }
        propertyName = options["propertyName"].get<std::string>();
    }

    std::map<std::string, std::pair<std::vector<PlanarPoint>, json>> groups;
    int missingCounter = 0;

    for (const auto& feature : fc["features"]) {
        const std::vector<PlanarPoint> ring = ParseOuterRing(ParseFeatureGeometry(feature));
        std::vector<PlanarPoint> hull = ConvexHull(OpenRing(ring));
        if (hull.empty()) {
            continue;
        }

        std::string key;
        json props = json::object();
        if (propertyName.empty()) {
            key = "__all__";
        } else if (feature.contains("properties") && feature["properties"].is_object() && feature["properties"].contains(propertyName)) {
            key = feature["properties"][propertyName].dump();
            props[propertyName] = feature["properties"][propertyName];
        } else {
            key = "__missing_" + std::to_string(missingCounter++);
        }

        auto it = groups.find(key);
        if (it == groups.end()) {
            groups.emplace(key, std::make_pair(hull, props));
            continue;
        }

        std::vector<PlanarPoint> mergedPts = OpenRing(it->second.first);
        const std::vector<PlanarPoint> newPts = OpenRing(hull);
        mergedPts.insert(mergedPts.end(), newPts.begin(), newPts.end());
        it->second.first = ConvexHull(mergedPts);
    }

    json outFeatures = json::array();
    for (const auto& kv : groups) {
        const auto& hull = kv.second.first;
        if (hull.empty()) {
            continue;
        }

        json f = BuildFeatureFromRing(hull, kv.second.second);
        if (!f.is_null()) {
            outFeatures.push_back(f);
        }
    }

    return {
        {"type", "FeatureCollection"},
        {"features", outFeatures}
    };
}

json BuildVoronoi(const json& pointsFeatureCollection, const json& optionsNode) {
    const json fc = ParseFeatureCollection(pointsFeatureCollection, "points");
    const json options = ParseOptions(optionsNode);

    Bbox bbox{-180.0, -90.0, 180.0, 90.0};
    if (options.contains("bbox")) {
        bbox = FeatureBuilders::ParseBbox(options["bbox"]);
    }

    std::vector<PlanarPoint> points;
    std::vector<json> properties;
    points.reserve(fc["features"].size());
    properties.reserve(fc["features"].size());

    for (const auto& feature : fc["features"]) {
        const auto lonlat = GeoJsonParsingUtils::ParsePointLike(feature);
        points.push_back({lonlat.lon, lonlat.lat});

        if (feature.contains("properties") && feature["properties"].is_object()) {
            properties.push_back(feature["properties"]);
        } else {
            properties.push_back(json::object());
        }
    }

    if (points.empty()) {
        throw std::invalid_argument("points.features must be non-empty.");
    }

    json outFeatures = json::array();
    for (size_t i = 0; i < points.size(); ++i) {
        std::vector<PlanarPoint> cell = ParseVoronoiCellSeed(bbox);
        const double xi = points[i].x;
        const double yi = points[i].y;

        for (size_t j = 0; j < points.size(); ++j) {
            if (i == j) {
                continue;
            }

            const double xj = points[j].x;
            const double yj = points[j].y;

            const double a = xj - xi;
            const double b = yj - yi;
            const double c = 0.5 * ((xj * xj + yj * yj) - (xi * xi + yi * yi));

            cell = ClipConvexPolyHalfPlane(cell, a, b, c);
            if (cell.empty()) {
                break;
            }
        }

        cell = EnsureClosedRing(cell);
        if (cell.size() < 4 || std::abs(GeoAlgorithms::ComputeRingSignedArea(cell)) <= kEpsilon) {
            continue;
        }

        json feature = BuildFeatureFromRing(cell, properties[i]);
        if (!feature.is_null()) {
            outFeatures.push_back(feature);
        }
    }

    return {
        {"type", "FeatureCollection"},
        {"features", outFeatures}
    };
}

} // namespace TransformationGeometryOps
