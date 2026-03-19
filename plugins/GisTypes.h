#pragma once

#include <vector>

// PlanarPoint: 2-D Cartesian coordinate for planar geometry computations.
struct PlanarPoint {
    double x;
    double y;
};

// LonLatPoint: WGS-84 geographic coordinate for geodetic calculations.
struct LonLatPoint {
    double lon;
    double lat;
};

// Bbox: axis-aligned bounding box in geographic or planar coordinates.
struct Bbox {
    double minX;
    double minY;
    double maxX;
    double maxY;
};

struct PolygonRings {
    std::vector<LonLatPoint> outerRing;
    std::vector<std::vector<LonLatPoint>> holes;
};
