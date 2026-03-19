#include "GeoAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "GeoMath/GeoMath.h"

namespace GeoAlgorithms {

PlanarPoint ToPlanar(const LonLatPoint& coordinate, double cosLat0) {
    const double lonRad = GeoMath::DegreesToRadians(coordinate.lon);
    const double latRad = GeoMath::DegreesToRadians(coordinate.lat);
    return {GeoMath::kEarthRadiusMeters * lonRad * cosLat0, GeoMath::kEarthRadiusMeters * latRad};
}

PolygonAreaCentroid ComputeRingMetrics(const std::vector<LonLatPoint>& ring, double cosLat0) {
    if (ring.size() < 4) {
        throw std::invalid_argument("Polygon ring must contain at least 4 coordinates including closure.");
    }

    double crossSum = 0.0;
    double centroidXAccumulator = 0.0;
    double centroidYAccumulator = 0.0;

    for (size_t i = 0; i + 1 < ring.size(); ++i) {
        const PlanarPoint p1 = ToPlanar(ring[i], cosLat0);
        const PlanarPoint p2 = ToPlanar(ring[i + 1], cosLat0);
        const double cross = (p1.x * p2.y) - (p2.x * p1.y);
        crossSum += cross;
        centroidXAccumulator += (p1.x + p2.x) * cross;
        centroidYAccumulator += (p1.y + p2.y) * cross;
    }

    const double signedArea = 0.5 * crossSum;
    if (std::abs(signedArea) < 1e-12) {
        return {0.0, 0.0, 0.0};
    }

    return {
        std::abs(signedArea),
        centroidXAccumulator / (6.0 * signedArea),
        centroidYAccumulator / (6.0 * signedArea)
    };
}

PolygonAreaCentroid ComputePolygonMetrics(const PolygonRings& polygon) {
    if (polygon.outerRing.empty()) {
        throw std::invalid_argument("Polygon must contain an outer ring.");
    }

    double latSum = 0.0;
    size_t latCount = 0;
    for (const auto& point : polygon.outerRing) {
        latSum += point.lat;
        ++latCount;
    }

    const double lat0Rad = GeoMath::DegreesToRadians(latSum / static_cast<double>(latCount));
    const double cosLat0 = std::cos(lat0Rad);
    if (std::abs(cosLat0) < 1e-12) {
        throw std::invalid_argument("Polygon is too close to poles for planar approximation.");
    }

    const PolygonAreaCentroid outer = ComputeRingMetrics(polygon.outerRing, cosLat0);
    double totalArea = outer.area;
    double weightedX = outer.area * outer.centroidX;
    double weightedY = outer.area * outer.centroidY;

    for (const auto& hole : polygon.holes) {
        const PolygonAreaCentroid holeMetrics = ComputeRingMetrics(hole, cosLat0);
        totalArea -= holeMetrics.area;
        weightedX -= holeMetrics.area * holeMetrics.centroidX;
        weightedY -= holeMetrics.area * holeMetrics.centroidY;
    }

    if (totalArea <= 0.0) {
        throw std::invalid_argument("Polygon area is zero after subtracting holes.");
    }

    return {
        totalArea,
        weightedX / totalArea,
        weightedY / totalArea
    };
}

LonLatPoint MeanCentroid(const std::vector<LonLatPoint>& coordinates) {
    double sumLon = 0.0;
    double sumLat = 0.0;
    for (const auto& c : coordinates) {
        sumLon += c.lon;
        sumLat += c.lat;
    }

    return {
        sumLon / static_cast<double>(coordinates.size()),
        sumLat / static_cast<double>(coordinates.size())
    };
}

bool PointInRing(double lon, double lat, const std::vector<LonLatPoint>& ring) {
    bool inside = false;
    const size_t n = ring.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = ring[i].lon;
        const double yi = ring[i].lat;
        const double xj = ring[j].lon;
        const double yj = ring[j].lat;

        const bool intersects = ((yi > lat) != (yj > lat)) &&
            (lon < (xj - xi) * (lat - yi) / (yj - yi + 1e-20) + xi);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool PointInPolygon(double lon, double lat, const PolygonRings& polygon) {
    if (!PointInRing(lon, lat, polygon.outerRing)) {
        return false;
    }
    for (const auto& hole : polygon.holes) {
        if (PointInRing(lon, lat, hole)) {
            return false;
        }
    }
    return true;
}

double PointToSegmentDistanceMeters(const LonLatPoint& p, const LonLatPoint& a, const LonLatPoint& b) {
    const double lat0Rad = GeoMath::DegreesToRadians(p.lat);
    const double cosLat0 = std::cos(lat0Rad);

    const auto toPlanar = [cosLat0](const LonLatPoint& c) -> PlanarPoint {
        return {
            GeoMath::kEarthRadiusMeters * GeoMath::DegreesToRadians(c.lon) * cosLat0,
            GeoMath::kEarthRadiusMeters * GeoMath::DegreesToRadians(c.lat)
        };
    };

    const PlanarPoint pp = toPlanar(p);
    const PlanarPoint pa = toPlanar(a);
    const PlanarPoint pb = toPlanar(b);

    const double abx = pb.x - pa.x;
    const double aby = pb.y - pa.y;
    const double apx = pp.x - pa.x;
    const double apy = pp.y - pa.y;

    const double ab2 = abx * abx + aby * aby;
    if (ab2 <= 1e-18) {
        const double dx = pp.x - pa.x;
        const double dy = pp.y - pa.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    const double t = std::clamp((apx * abx + apy * aby) / ab2, 0.0, 1.0);
    const double projX = pa.x + t * abx;
    const double projY = pa.y + t * aby;

    const double dx = pp.x - projX;
    const double dy = pp.y - projY;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace GeoAlgorithms
