#pragma once

#include <string>
#include <vector>

#include "GisTypes.h"
#include "json.hpp"

namespace GeoJsonParsingUtils {

bool IsValidCoordinate(double latitude, double longitude);
void ValidateCoordinate(double latitude, double longitude);

nlohmann::json ParseJsonString(const std::string& value, const std::string& fieldName);

nlohmann::json ParseJsonLikeArgument(const nlohmann::json& arguments, const std::string& fieldName);

nlohmann::json ParseOptionsArgument(const nlohmann::json& arguments);

LonLatPoint ParseCoordinateArray(const nlohmann::json& coordinates);

LonLatPoint ParsePointLike(const nlohmann::json& pointNode);

nlohmann::json ParsePointGeometryLike(const nlohmann::json& node, const std::string& fieldName);

nlohmann::json ParseLineStringGeometryLike(const nlohmann::json& node, const std::string& fieldName);

std::vector<LonLatPoint> ParseLineStringPoints(const nlohmann::json& lineNode, const std::string& fieldName);

std::vector<std::vector<LonLatPoint>> ParseLineOrMultiLine(const nlohmann::json& geojsonNode, const std::string& fieldName);

nlohmann::json ParseFeatureCollectionLike(const nlohmann::json& node, const std::string& fieldName);

// Unwrap a Feature wrapper to its bare geometry. Non-Feature nodes are returned unchanged.
// Throws if the node is a Feature with null geometry.
nlohmann::json ExtractGeometry(const nlohmann::json& node);

// Return the geometry type string from a GeoJSON geometry object.
// Throws if the object does not have a string "type" field.
std::string GetGeometryType(const nlohmann::json& geom);

// Parse a single [x, y] coordinate array into a PlanarPoint.
// Throws if the array has fewer than 2 numeric elements.
PlanarPoint ParseCoordinatePair(const nlohmann::json& coord);

// Parse an array of [x, y] coordinate pairs into a vector of PlanarPoints.
// Throws if input is not an array or any element is not a valid coordinate pair.
std::vector<PlanarPoint> ParseCoordinatePairArray(const nlohmann::json& arr);

} // namespace GeoJsonParsingUtils
