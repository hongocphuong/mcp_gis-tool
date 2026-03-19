#pragma once

#include <vector>

#include "GisTypes.h"

namespace GeoAlgorithms {

struct PolygonAreaCentroid {
    double area;
    double centroidX;
    double centroidY;
};

// Planar projection
PlanarPoint ToPlanar(const LonLatPoint& coordinate, double cosLat0);

// Polygon area and centroid
PolygonAreaCentroid ComputeRingMetrics(const std::vector<LonLatPoint>& ring, double cosLat0);
PolygonAreaCentroid ComputePolygonMetrics(const PolygonRings& polygon);

// Centroid helpers
LonLatPoint MeanCentroid(const std::vector<LonLatPoint>& coordinates);

// Point-in-polygon
bool PointInRing(double lon, double lat, const std::vector<LonLatPoint>& ring);
bool PointInPolygon(double lon, double lat, const PolygonRings& polygon);

// Proximity
double PointToSegmentDistanceMeters(const LonLatPoint& p, const LonLatPoint& a, const LonLatPoint& b);

} // namespace GeoAlgorithms
