#pragma once

#include <string>
#include <vector>

#include "GisTypes.h"
#include "json.hpp"

namespace GeoJsonParsingUtils {

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

} // namespace GeoJsonParsingUtils
