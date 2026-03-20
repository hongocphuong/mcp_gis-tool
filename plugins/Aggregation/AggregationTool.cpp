#include "AggregationTool.h"

#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include "AggregationCommon.h"
#include "AggregationSchemas.h"
#include "GeoJsonParsing/GeoJsonParsingUtils.h"
#include "json.hpp"

namespace {

using json = nlohmann::json;
using GeoJsonParsingUtils::ParseFeatureCollectionLike;
using GeoJsonParsingUtils::ParseJsonLikeArgument;
using GeoJsonParsingUtils::ParseOptionsArgument;
using AggregationCommon::ValidatePolygonFeatureCollection;
using AggregationCommon::ParsePointFeatures;
using AggregationCommon::FeatureContainsPoint;
using AggregationCommon::ConvertDistanceToKilometers;
using AggregationCommon::BuildNeighborhoods;
using AggregationCommon::EnsureFeatureProperties;
using AggregationCommon::SetClusterLabels;

ToolResult AggregationCollect(const json& arguments) {
    const json polygons = ParseFeatureCollectionLike(ParseJsonLikeArgument(arguments, "polygons"), "polygons");
    const json points = ParseFeatureCollectionLike(ParseJsonLikeArgument(arguments, "points"), "points");
    ValidatePolygonFeatureCollection(polygons);

    if (!arguments.contains("in_field") || !arguments["in_field"].is_string()) {
        throw std::invalid_argument("Field 'in_field' must be a string.");
    }
    if (!arguments.contains("out_field") || !arguments["out_field"].is_string()) {
        throw std::invalid_argument("Field 'out_field' must be a string.");
    }

    const std::string inField = arguments["in_field"].get<std::string>();
    const std::string outField = arguments["out_field"].get<std::string>();

    const auto pointFeatures = ParsePointFeatures(points);

    json output = polygons;
    for (auto& polygonFeature : output["features"]) {
        json collected = json::array();
        for (const auto& pointFeature : pointFeatures) {
            if (!FeatureContainsPoint(polygonFeature, pointFeature.point)) {
                continue;
            }

            if (pointFeature.feature.contains("properties") &&
                pointFeature.feature["properties"].is_object() &&
                pointFeature.feature["properties"].contains(inField)) {
                collected.push_back(pointFeature.feature["properties"][inField]);
            }
        }

        EnsureFeatureProperties(polygonFeature);
        polygonFeature["properties"][outField] = std::move(collected);
    }

    return {
        "Collected point properties into polygon features.",
        output
    };
}

ToolResult AggregationClustersDbscan(const json& arguments) {
    const json pointsCollection = ParseFeatureCollectionLike(ParseJsonLikeArgument(arguments, "points"), "points");
    auto points = ParsePointFeatures(pointsCollection);

    if (!arguments.contains("max_distance") || !arguments["max_distance"].is_number()) {
        throw std::invalid_argument("Field 'max_distance' must be a number.");
    }

    const json options = ParseOptionsArgument(arguments);
    const std::string units = options.contains("units") && options["units"].is_string()
        ? options["units"].get<std::string>()
        : "kilometers";
    const int minPoints = options.contains("minPoints") && options["minPoints"].is_number_integer()
        ? options["minPoints"].get<int>()
        : 3;

    if (minPoints < 1) {
        throw std::invalid_argument("options.minPoints must be >= 1.");
    }

    const double epsilonKm = ConvertDistanceToKilometers(arguments["max_distance"].get<double>(), units);
    if (epsilonKm < 0.0) {
        throw std::invalid_argument("Field 'max_distance' must be >= 0.");
    }

    json output = pointsCollection;
    const int n = static_cast<int>(points.size());
    const int kUnvisited = -99;
    const int kNoise = -1;

    std::vector<int> labels(n, kUnvisited);
    const auto neighborhoods = BuildNeighborhoods(points, epsilonKm);

    int clusterId = 0;
    for (int i = 0; i < n; ++i) {
        if (labels[i] != kUnvisited) {
            continue;
        }

        if (static_cast<int>(neighborhoods[i].size()) < minPoints) {
            labels[i] = kNoise;
            continue;
        }

        labels[i] = clusterId;
        std::queue<int> seeds;
        for (const int neighbor : neighborhoods[i]) {
            if (neighbor != i) {
                seeds.push(neighbor);
            }
        }

        while (!seeds.empty()) {
            const int current = seeds.front();
            seeds.pop();

            if (labels[current] == kNoise) {
                labels[current] = clusterId;
            }
            if (labels[current] != kUnvisited) {
                continue;
            }

            labels[current] = clusterId;
            if (static_cast<int>(neighborhoods[current].size()) >= minPoints) {
                for (const int nn : neighborhoods[current]) {
                    if (labels[nn] == kUnvisited || labels[nn] == kNoise) {
                        seeds.push(nn);
                    }
                }
            }
        }

        ++clusterId;
    }

    SetClusterLabels(output, labels);

    return {
        "Clustered points using DBSCAN.",
        output
    };
}

ToolResult AggregationClustersKmeans(const json& arguments) {
    const json pointsCollection = ParseFeatureCollectionLike(ParseJsonLikeArgument(arguments, "points"), "points");
    auto points = ParsePointFeatures(pointsCollection);

    if (!arguments.contains("number_of_clusters") || !arguments["number_of_clusters"].is_number_integer()) {
        throw std::invalid_argument("Field 'number_of_clusters' must be an integer.");
    }

    const json options = ParseOptionsArgument(arguments);

    int k = arguments["number_of_clusters"].get<int>();
    if (options.contains("numberOfClusters") && options["numberOfClusters"].is_number_integer()) {
        k = options["numberOfClusters"].get<int>();
    }

    if (k < 1) {
        throw std::invalid_argument("number_of_clusters must be >= 1.");
    }

    const int n = static_cast<int>(points.size());
    if (n == 0) {
        throw std::invalid_argument("Field 'points.features' must be a non-empty array.");
    }

    if (k > n) {
        k = n;
    }

    std::vector<LonLatPoint> centroids;
    centroids.reserve(k);
    for (int i = 0; i < k; ++i) {
        const int index = static_cast<int>(std::floor((static_cast<double>(i) * n) / k));
        centroids.push_back(points[index].point);
    }

    std::vector<int> labels(n, 0);
    constexpr int kMaxIterations = 100;
    for (int iter = 0; iter < kMaxIterations; ++iter) {
        bool changed = false;

        for (int i = 0; i < n; ++i) {
            double bestDistance = std::numeric_limits<double>::infinity();
            int bestCluster = 0;

            for (int c = 0; c < k; ++c) {
                const double dx = points[i].point.lon - centroids[c].lon;
                const double dy = points[i].point.lat - centroids[c].lat;
                const double distance = dx * dx + dy * dy;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestCluster = c;
                }
            }

            if (labels[i] != bestCluster) {
                labels[i] = bestCluster;
                changed = true;
            }
        }

        std::vector<double> sumLon(k, 0.0);
        std::vector<double> sumLat(k, 0.0);
        std::vector<int> count(k, 0);

        for (int i = 0; i < n; ++i) {
            const int c = labels[i];
            sumLon[c] += points[i].point.lon;
            sumLat[c] += points[i].point.lat;
            count[c] += 1;
        }

        for (int c = 0; c < k; ++c) {
            if (count[c] > 0) {
                centroids[c].lon = sumLon[c] / count[c];
                centroids[c].lat = sumLat[c] / count[c];
            }
        }

        if (!changed) {
            break;
        }
    }

    json output = pointsCollection;
    SetClusterLabels(output, labels);

    return {
        "Clustered points using K-means.",
        output
    };
}

} // namespace

void RegisterAggregationTools(ToolRegistry& registry) {
    registry.Register({
        {
            AggregationSchemas::kCollectName,
            AggregationSchemas::kCollectDescription,
            AggregationSchemas::kCollectSchema
        },
        AggregationCollect
    });

    registry.Register({
        {
            AggregationSchemas::kClustersDbscanName,
            AggregationSchemas::kClustersDbscanDescription,
            AggregationSchemas::kClustersDbscanSchema
        },
        AggregationClustersDbscan
    });

    registry.Register({
        {
            AggregationSchemas::kClustersKmeansName,
            AggregationSchemas::kClustersKmeansDescription,
            AggregationSchemas::kClustersKmeansSchema
        },
        AggregationClustersKmeans
    });
}
