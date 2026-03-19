#include "GeoMath.h"

#include <algorithm>
#include <cmath>

namespace GeoMath {

double DegreesToRadians(double degrees) {
    return degrees * (kPi / 180.0);
}

double RadiansToDegrees(double radians) {
    return radians * (180.0 / kPi);
}

double NormalizeLon180(double lonDeg) {
    return std::fmod(lonDeg + 540.0, 360.0) - 180.0;
}

double NormalizeDegrees360(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double HaversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLat = DegreesToRadians(lat2 - lat1);
    const double dLon = DegreesToRadians(lon2 - lon1);

    const double sinHalfDLat = std::sin(dLat / 2.0);
    const double sinHalfDLon = std::sin(dLon / 2.0);
    const double a = (sinHalfDLat * sinHalfDLat) +
                     std::cos(lat1Rad) * std::cos(lat2Rad) * (sinHalfDLon * sinHalfDLon);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return kEarthRadiusMeters * c;
}

double InitialBearingDegrees(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLonRad = DegreesToRadians(lon2 - lon1);

    const double y = std::sin(dLonRad) * std::cos(lat2Rad);
    const double x = (std::cos(lat1Rad) * std::sin(lat2Rad)) -
                     (std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(dLonRad));

    return NormalizeDegrees360(RadiansToDegrees(std::atan2(y, x)));
}

LonLatPoint DestinationPoint(double lat1, double lon1, double bearingDeg, double distanceMeters) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lon1Rad = DegreesToRadians(lon1);
    const double bearingRad = DegreesToRadians(bearingDeg);
    const double delta = distanceMeters / kEarthRadiusMeters;

    const double lat2Rad = std::asin(
        std::sin(lat1Rad) * std::cos(delta) +
        std::cos(lat1Rad) * std::sin(delta) * std::cos(bearingRad)
    );
    const double lon2Rad = lon1Rad + std::atan2(
        std::sin(bearingRad) * std::sin(delta) * std::cos(lat1Rad),
        std::cos(delta) - std::sin(lat1Rad) * std::sin(lat2Rad)
    );

    return {NormalizeLon180(RadiansToDegrees(lon2Rad)), RadiansToDegrees(lat2Rad)};
}

LonLatPoint SphericalMidpoint(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLonRad = DegreesToRadians(lon2 - lon1);

    const double bx = std::cos(lat2Rad) * std::cos(dLonRad);
    const double by = std::cos(lat2Rad) * std::sin(dLonRad);

    const double latMidRad = std::atan2(
        std::sin(lat1Rad) + std::sin(lat2Rad),
        std::sqrt((std::cos(lat1Rad) + bx) * (std::cos(lat1Rad) + bx) + by * by)
    );
    const double lonMidRad = DegreesToRadians(lon1) + std::atan2(by, std::cos(lat1Rad) + bx);

    return {NormalizeLon180(RadiansToDegrees(lonMidRad)), RadiansToDegrees(latMidRad)};
}

LonLatPoint InterpolateGreatCircle(const LonLatPoint& start, const LonLatPoint& end, double t) {
    const double lat1 = DegreesToRadians(start.lat);
    const double lon1 = DegreesToRadians(start.lon);
    const double lat2 = DegreesToRadians(end.lat);
    const double lon2 = DegreesToRadians(end.lon);

    const double x1 = std::cos(lat1) * std::cos(lon1);
    const double y1 = std::cos(lat1) * std::sin(lon1);
    const double z1 = std::sin(lat1);

    const double x2 = std::cos(lat2) * std::cos(lon2);
    const double y2 = std::cos(lat2) * std::sin(lon2);
    const double z2 = std::sin(lat2);

    const double dot = std::clamp(x1 * x2 + y1 * y2 + z1 * z2, -1.0, 1.0);
    const double omega = std::acos(dot);

    if (std::abs(omega) < 1e-12) {
        return start;
    }

    const double sinOmega = std::sin(omega);
    const double a = std::sin((1.0 - t) * omega) / sinOmega;
    const double b = std::sin(t * omega) / sinOmega;

    const double x = a * x1 + b * x2;
    const double y = a * y1 + b * y2;
    const double z = a * z1 + b * z2;

    const double lat = std::atan2(z, std::sqrt(x * x + y * y));
    const double lon = std::atan2(y, x);

    return {NormalizeLon180(RadiansToDegrees(lon)), RadiansToDegrees(lat)};
}

double RhumbDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);
    const double dLat = lat2Rad - lat1Rad;

    const double dPsi = std::log(
        std::tan(lat2Rad / 2.0 + kPi / 4.0) /
        std::tan(lat1Rad / 2.0 + kPi / 4.0)
    );
    const double q = (std::abs(dPsi) > 1e-12) ? dLat / dPsi : std::cos(lat1Rad);

    double dLon = DegreesToRadians(lon2 - lon1);
    if (std::abs(dLon) > kPi) {
        dLon = (dLon > 0.0) ? dLon - 2.0 * kPi : dLon + 2.0 * kPi;
    }

    return kEarthRadiusMeters * std::sqrt(dLat * dLat + q * q * dLon * dLon);
}

double RhumbBearingDegrees(double lat1, double lon1, double lat2, double lon2) {
    const double lat1Rad = DegreesToRadians(lat1);
    const double lat2Rad = DegreesToRadians(lat2);

    const double dPsi = std::log(
        std::tan(lat2Rad / 2.0 + kPi / 4.0) /
        std::tan(lat1Rad / 2.0 + kPi / 4.0)
    );

    double dLon = DegreesToRadians(lon2 - lon1);
    if (std::abs(dLon) > kPi) {
        dLon = (dLon > 0.0) ? dLon - 2.0 * kPi : dLon + 2.0 * kPi;
    }

    return NormalizeDegrees360(RadiansToDegrees(std::atan2(dLon, dPsi)));
}

LonLatPoint RhumbDestinationPoint(double lat, double lon, double bearingDeg, double distanceMeters) {
    const double delta = distanceMeters / kEarthRadiusMeters;
    const double theta = DegreesToRadians(bearingDeg);

    const double phi1 = DegreesToRadians(lat);
    const double lambda1 = DegreesToRadians(lon);

    double phi2 = phi1 + delta * std::cos(theta);

    if (std::abs(phi2) > kPi / 2.0) {
        phi2 = (phi2 > 0.0) ? kPi - phi2 : -kPi - phi2;
    }

    const double dPsi = std::log(
        std::tan(phi2 / 2.0 + kPi / 4.0) /
        std::tan(phi1 / 2.0 + kPi / 4.0)
    );

    const double q = (std::abs(dPsi) > 1e-12) ? (phi2 - phi1) / dPsi : std::cos(phi1);
    const double dLambda = delta * std::sin(theta) / q;
    const double lambda2 = lambda1 + dLambda;

    return {NormalizeLon180(RadiansToDegrees(lambda2)), RadiansToDegrees(phi2)};
}

} // namespace GeoMath
