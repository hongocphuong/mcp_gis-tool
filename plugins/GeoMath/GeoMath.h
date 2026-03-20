#pragma once

#include "GisTypes.h"

namespace GeoMath {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kEarthRadiusMeters = 6371008.8;
inline constexpr double kEpsilon = 1e-12;

double DegreesToRadians(double degrees);
double RadiansToDegrees(double radians);
double NormalizeLon180(double lonDeg);
double NormalizeDegrees360(double degrees);

double HaversineDistanceMeters(double lat1, double lon1, double lat2, double lon2);

// Great-circle (spherical) navigation
double InitialBearingDegrees(double lat1, double lon1, double lat2, double lon2);
LonLatPoint DestinationPoint(double lat1, double lon1, double bearingDeg, double distanceMeters);
LonLatPoint SphericalMidpoint(double lat1, double lon1, double lat2, double lon2);
LonLatPoint InterpolateGreatCircle(const LonLatPoint& start, const LonLatPoint& end, double t);

// Rhumb-line navigation
double RhumbDistanceMeters(double lat1, double lon1, double lat2, double lon2);
double RhumbBearingDegrees(double lat1, double lon1, double lat2, double lon2);
LonLatPoint RhumbDestinationPoint(double lat, double lon, double bearingDeg, double distanceMeters);

} // namespace GeoMath
