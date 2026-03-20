# Classification Tool Test Prompts (Claude Desktop)

## 1) List tools and verify nearestPoint exists

```text
Please list all MCP tools from the server, then confirm that tool "nearestPoint" exists.
```

## 2) Happy path nearestPoint

```text
Call MCP tool "nearestPoint" with arguments:
{
  "target_point": [106.698, 10.773],
  "points": {
    "type": "FeatureCollection",
    "features": [
      {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [106.722, 10.795]},
        "properties": {"name": "Landmark 81"}
      },
      {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [106.699, 10.780]},
        "properties": {"name": "Nha tho Duc Ba"}
      },
      {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [106.652, 10.819]},
        "properties": {"name": "Tan Son Nhat"}
      }
    ]
  }
}

Expected output requirements:
- Return the nearest feature
- Include properties.distance (meters)
- Summarize which place is nearest by name.
```

Expected:
- `geometry.coordinates` = `[106.699, 10.78]`
- `properties.name` = `Nha tho Duc Ba`
- `properties.distance` > 0

## 3) Validation test (expected error)

```text
Call MCP tool "nearestPoint" with arguments:
{
  "target_point": [106.698, 10.773],
  "points": {
    "type": "FeatureCollection",
    "features": []
  }
}

Expected behavior: return a validation error for empty features array.
```

Expected:
- `isError` = `true`
- Error message contains `non-empty array`
