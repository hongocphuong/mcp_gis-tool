import asyncio
import json
import math
import os
import sys
from contextlib import AsyncExitStack

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


class GisToolsTester:
    def __init__(self):
        self.exit_stack = AsyncExitStack()
        self.session = None

    async def connect(self, config_path: str):
        if not os.path.exists(config_path):
            raise FileNotFoundError(f"Configuration file not found: {config_path}")

        with open(config_path, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)

        servers = config.get("mcpServers", {})
        if not servers:
            raise RuntimeError("No mcpServers found in configuration.")

        _, server = next(iter(servers.items()))
        command = server.get("command")
        args = server.get("args", [])
        env = server.get("env")

        if not command:
            raise RuntimeError("Server command is missing in configuration.")

        server_params = StdioServerParameters(command=command, args=args, env=env)
        stdio_transport = await self.exit_stack.enter_async_context(stdio_client(server_params))
        stdio, write = stdio_transport
        self.session = await self.exit_stack.enter_async_context(ClientSession(stdio, write))
        await self.session.initialize()

    @staticmethod
    def _get_json_payload(result):
        contents = getattr(result, "content", [])
        for item in contents:
            item_type = getattr(item, "type", None)
            if item_type == "json":
                payload = getattr(item, "json", None)
                if payload is not None:
                    return payload
            if item_type == "text":
                text = getattr(item, "text", None)
                if isinstance(text, str) and text.startswith("{") and text.endswith("}"):
                    try:
                        payload = json.loads(text)
                        if isinstance(payload, dict):
                            return payload
                    except json.JSONDecodeError:
                        pass
            if isinstance(item, dict) and item.get("type") == "json":
                return item.get("json")
            if isinstance(item, dict) and item.get("type") == "text":
                text = item.get("text")
                if isinstance(text, str) and text.startswith("{") and text.endswith("}"):
                    try:
                        payload = json.loads(text)
                        if isinstance(payload, dict):
                            return payload
                    except json.JSONDecodeError:
                        pass
        raise AssertionError("No JSON payload found in tool result content.")

    @staticmethod
    def _assert_close(actual: float, expected: float, tolerance: float, message: str):
        if abs(actual - expected) > tolerance:
            raise AssertionError(f"{message}. Expected {expected}, got {actual}")

    async def run(self):
        response = await self.session.list_tools()
        tool_names = {tool.name for tool in response.tools}

        expected_tools = {
            "calculate_wgs84_polyline_length",
            "calculate_wgs84_azimuth",
            "calculate_wgs84_reverse_azimuth",
            "calculate_geojson_bbox",
            "calculate_geojson_centroid",
            "calculate_geojson_area",
        }
        missing = expected_tools - tool_names
        if missing:
            raise AssertionError(f"Missing expected GIS tools: {sorted(missing)}")

        feature_collection = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "geometry": {
                        "type": "LineString",
                        "coordinates": [
                            [106.7009, 10.7769],
                            [105.8342, 21.0278],
                        ],
                    },
                    "properties": {},
                },
                {
                    "type": "Feature",
                    "geometry": {
                        "type": "Point",
                        "coordinates": [108.2022, 16.0544],
                    },
                    "properties": {},
                },
            ],
        }

        bbox_result = await self.session.call_tool(
            "calculate_geojson_bbox",
            {"geojson": feature_collection},
        )
        bbox_payload = self._get_json_payload(bbox_result)

        expected_bbox = [105.8342, 10.7769, 108.2022, 21.0278]
        bbox_array = bbox_payload.get("bbox")
        if not bbox_array or len(bbox_array) != 4:
            raise AssertionError("BBox payload is missing bbox array with 4 values.")
        for index, expected_value in enumerate(expected_bbox):
            self._assert_close(float(bbox_array[index]), expected_value, 1e-6, f"BBox value at index {index} mismatch")

        centroid_result = await self.session.call_tool(
            "calculate_geojson_centroid",
            {"geojson": feature_collection},
        )
        centroid_payload = self._get_json_payload(centroid_result)

        expected_centroid_lon = (106.7009 + 105.8342 + 108.2022) / 3.0
        expected_centroid_lat = (10.7769 + 21.0278 + 16.0544) / 3.0
        self._assert_close(float(centroid_payload.get("centroid_lon")), expected_centroid_lon, 1e-6, "Centroid longitude mismatch")
        self._assert_close(float(centroid_payload.get("centroid_lat")), expected_centroid_lat, 1e-6, "Centroid latitude mismatch")

        polygon_geojson = {
            "type": "Polygon",
            "coordinates": [
                [
                    [0.0, 0.0],
                    [2.0, 0.0],
                    [2.0, 2.0],
                    [0.0, 2.0],
                    [0.0, 0.0],
                ]
            ],
        }

        polygon_centroid_result = await self.session.call_tool(
            "calculate_geojson_centroid",
            {"geojson": polygon_geojson},
        )
        polygon_centroid_payload = self._get_json_payload(polygon_centroid_result)
        self._assert_close(float(polygon_centroid_payload.get("centroid_lon")), 1.0, 1e-6, "Polygon centroid longitude mismatch")
        self._assert_close(float(polygon_centroid_payload.get("centroid_lat")), 1.0, 1e-6, "Polygon centroid latitude mismatch")

        area_result = await self.session.call_tool(
            "calculate_geojson_area",
            {"geojson": polygon_geojson},
        )
        area_payload = self._get_json_payload(area_result)
        area_m2 = float(area_payload.get("area_square_meters"))
        if not math.isfinite(area_m2) or area_m2 <= 0:
            raise AssertionError(f"Polygon area must be positive finite value, got {area_m2}")

        expected_area_m2 = 4.0 * (6371008.8 * math.pi / 180.0) ** 2
        relative_error = abs(area_m2 - expected_area_m2) / expected_area_m2
        if relative_error > 0.05:
            raise AssertionError(f"Polygon area relative error too high: {relative_error:.4f}")

        mixed_feature_collection = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "geometry": None,
                    "properties": {"name": "null-geometry"},
                },
                {
                    "type": "Feature",
                    "geometry": {
                        "type": "GeometryCollection",
                        "geometries": [
                            {"type": "Point", "coordinates": [100.0, 0.0]},
                            {"type": "LineString", "coordinates": [[101.0, 1.0], [102.0, 2.0]]},
                        ],
                    },
                    "properties": {},
                },
            ],
        }

        mixed_bbox_result = await self.session.call_tool(
            "calculate_geojson_bbox",
            {"geojson": mixed_feature_collection},
        )
        mixed_bbox_payload = self._get_json_payload(mixed_bbox_result)
        mixed_bbox = mixed_bbox_payload.get("bbox")
        expected_mixed_bbox = [100.0, 0.0, 102.0, 2.0]
        for index, expected_value in enumerate(expected_mixed_bbox):
            self._assert_close(float(mixed_bbox[index]), expected_value, 1e-6, f"Mixed bbox mismatch at index {index}")

        mixed_centroid_result = await self.session.call_tool(
            "calculate_geojson_centroid",
            {"geojson": mixed_feature_collection},
        )
        mixed_centroid_payload = self._get_json_payload(mixed_centroid_result)
        self._assert_close(float(mixed_centroid_payload.get("centroid_lon")), 101.0, 1e-6, "Mixed centroid longitude mismatch")
        self._assert_close(float(mixed_centroid_payload.get("centroid_lat")), 1.0, 1e-6, "Mixed centroid latitude mismatch")

        polyline_points = [
            {"lat": 10.7769, "lon": 106.7009},
            {"lat": 11.9404, "lon": 108.4583},
            {"lat": 16.0544, "lon": 108.2022},
        ]

        polyline_result = await self.session.call_tool(
            "calculate_wgs84_polyline_length",
            {"points": polyline_points},
        )
        polyline_payload = self._get_json_payload(polyline_result)

        length_meters = float(polyline_payload.get("length_meters"))
        if not math.isfinite(length_meters) or length_meters <= 0:
            raise AssertionError(f"Polyline length must be positive finite value, got {length_meters}")

        azimuth_result = await self.session.call_tool(
            "calculate_wgs84_azimuth",
            {
                "lat1": 0.0,
                "lon1": 0.0,
                "lat2": 0.0,
                "lon2": 1.0,
                "return_direction": True,
                "direction_sectors": 8,
            },
        )
        azimuth_payload = self._get_json_payload(azimuth_result)
        self._assert_close(float(azimuth_payload.get("azimuth_degrees")), 90.0, 1e-6, "Azimuth mismatch for eastward segment")
        if azimuth_payload.get("direction") != "E":
            raise AssertionError(f"Azimuth direction mismatch. Expected E, got {azimuth_payload.get('direction')}")

        reverse_azimuth_result = await self.session.call_tool(
            "calculate_wgs84_reverse_azimuth",
            {
                "lat1": 0.0,
                "lon1": 0.0,
                "lat2": 0.0,
                "lon2": 1.0,
                "return_direction": True,
                "direction_sectors": 8,
            },
        )
        reverse_azimuth_payload = self._get_json_payload(reverse_azimuth_result)
        self._assert_close(float(reverse_azimuth_payload.get("reverse_azimuth_degrees")), 270.0, 1e-6, "Reverse azimuth mismatch")
        if reverse_azimuth_payload.get("direction") != "W":
            raise AssertionError(f"Reverse azimuth direction mismatch. Expected W, got {reverse_azimuth_payload.get('direction')}")

        print("GIS tools test passed.")
        print(f"BBox: {bbox_payload.get('bbox')}")
        print(f"Centroid: {centroid_payload.get('centroid')}")
        print(f"Polygon area (m^2): {area_m2:.3f}")
        print(f"Mixed bbox: {mixed_bbox_payload.get('bbox')}")
        print(f"Polyline length (m): {length_meters:.3f}")
        print(f"Azimuth (deg): {float(azimuth_payload.get('azimuth_degrees')):.3f}")
        print(f"Reverse azimuth (deg): {float(reverse_azimuth_payload.get('reverse_azimuth_degrees')):.3f}")

    async def close(self):
        await self.exit_stack.aclose()


async def main():
    if len(sys.argv) < 2:
        print("Usage: python test-gis-tools-stdio.py <configuration.json>")
        sys.exit(1)

    tester = GisToolsTester()
    try:
        await tester.connect(sys.argv[1])
        await tester.run()
    finally:
        await tester.close()


if __name__ == "__main__":
    asyncio.run(main())
