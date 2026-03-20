#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def load_server_config(config_path: Path, server_name: str) -> tuple[list[str], dict]:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    servers = config.get("mcpServers", {})
    if server_name not in servers:
        available = ", ".join(sorted(servers.keys())) or "<none>"
        raise RuntimeError(
            f"Server '{server_name}' not found in {config_path}. Available: {available}"
        )

    server = servers[server_name]
    command = server.get("command")
    if not command:
        raise RuntimeError(f"Missing 'command' for server '{server_name}' in {config_path}.")

    args = server.get("args", [])
    if not isinstance(args, list):
        raise RuntimeError(f"Expected 'args' to be an array for server '{server_name}'.")

    env = dict(server.get("env", {}))
    launch = [command, *args]
    return launch, env


class McpStdioClient:
    def __init__(self, launch_cmd: list[str], extra_env: dict[str, str]):
        merged_env = {**os.environ, **extra_env}
        self.proc = subprocess.Popen(
            launch_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
            env=merged_env,
        )

    def request(self, payload: dict) -> dict:
        if self.proc.stdin is None or self.proc.stdout is None:
            raise RuntimeError("MCP process stdio pipes are not available.")

        self.proc.stdin.write(json.dumps(payload, ensure_ascii=False) + "\n")
        self.proc.stdin.flush()

        line = self.proc.stdout.readline()
        if not line:
            stderr_text = ""
            if self.proc.stderr is not None:
                stderr_text = self.proc.stderr.read()
            raise RuntimeError(
                "No response from MCP server. "
                f"Exit code: {self.proc.poll()}, stderr: {stderr_text.strip()}"
            )

        try:
            return json.loads(line)
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"Invalid JSON response from server: {line.strip()}") from exc

    def close(self) -> None:
        if self.proc.stdin is not None:
            try:
                self.proc.stdin.close()
            except Exception:
                pass

        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def parse_tool_data_text(call_result: dict) -> dict:
    content = call_result.get("content", [])
    if not isinstance(content, list) or len(content) < 2:
        raise AssertionError(f"Unexpected tool result content: {content}")

    data_text = content[1].get("text")
    if not isinstance(data_text, str):
        raise AssertionError("Second content item does not contain JSON text payload.")
    return json.loads(data_text)


def run_test(config_path: Path, server_name: str) -> None:
    launch_cmd, env = load_server_config(config_path, server_name)
    print(f"[INFO] Launching MCP server: {' '.join(launch_cmd)}")

    client = McpStdioClient(launch_cmd, env)
    try:
        init_req = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "classification-smoke-test", "version": "1.0.0"},
            },
        }
        init_res = client.request(init_req)
        assert "result" in init_res, f"initialize failed: {init_res}"

        list_req = {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}
        list_res = client.request(list_req)
        tools = list_res.get("result", {}).get("tools", [])
        tool_names = {tool.get("name") for tool in tools if isinstance(tool, dict)}
        assert "nearestPoint" in tool_names, f"nearestPoint not found in tools/list: {tool_names}"
        print("[PASS] tools/list contains nearestPoint")

        nearest_req = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/call",
            "params": {
                "name": "nearestPoint",
                "arguments": {
                    "target_point": [106.698, 10.773],
                    "points": {
                        "type": "FeatureCollection",
                        "features": [
                            {
                                "type": "Feature",
                                "geometry": {"type": "Point", "coordinates": [106.722, 10.795]},
                                "properties": {"name": "Landmark 81"},
                            },
                            {
                                "type": "Feature",
                                "geometry": {"type": "Point", "coordinates": [106.699, 10.78]},
                                "properties": {"name": "Nha tho Duc Ba"},
                            },
                            {
                                "type": "Feature",
                                "geometry": {"type": "Point", "coordinates": [106.652, 10.819]},
                                "properties": {"name": "Tan Son Nhat"},
                            },
                        ],
                    },
                },
            },
        }
        nearest_res = client.request(nearest_req)
        nearest_result = nearest_res.get("result", {})
        assert nearest_result.get("isError") is False, f"nearestPoint returned error: {nearest_result}"

        nearest_feature = parse_tool_data_text(nearest_result)
        coords = nearest_feature.get("geometry", {}).get("coordinates")
        assert coords == [106.699, 10.78], f"Unexpected nearest point coordinates: {coords}"

        distance = nearest_feature.get("properties", {}).get("distance")
        assert isinstance(distance, (int, float)), f"Distance is not numeric: {distance}"
        assert 0 <= distance < 2000, f"Distance out of expected range (meters): {distance}"
        print("[PASS] tools/call nearestPoint returns expected nearest feature and distance")

        invalid_req = {
            "jsonrpc": "2.0",
            "id": 4,
            "method": "tools/call",
            "params": {
                "name": "nearestPoint",
                "arguments": {
                    "target_point": [106.698, 10.773],
                    "points": {"type": "FeatureCollection", "features": []},
                },
            },
        }
        invalid_res = client.request(invalid_req)
        invalid_result = invalid_res.get("result", {})
        assert invalid_result.get("isError") is True, f"Expected error response: {invalid_result}"
        invalid_text = ""
        content = invalid_result.get("content", [])
        if isinstance(content, list) and content:
            invalid_text = str(content[0].get("text", ""))
        assert "non-empty array" in invalid_text, f"Unexpected validation message: {invalid_text}"
        print("[PASS] nearestPoint validation rejects empty features list")

    finally:
        client.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke test ClassificationTool nearestPoint over MCP stdio.")
    parser.add_argument(
        "--config",
        default="test/configuration-stdio-windows.json",
        help="Path to Claude Desktop style MCP config JSON file.",
    )
    parser.add_argument(
        "--server",
        default="mcp-server",
        help="Server key inside mcpServers object.",
    )
    args = parser.parse_args()

    config_path = Path(args.config)
    if not config_path.exists():
        raise RuntimeError(f"Config file not found: {config_path}")

    run_test(config_path, args.server)
    print("[DONE] ClassificationTool stdio smoke test passed.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(2)
