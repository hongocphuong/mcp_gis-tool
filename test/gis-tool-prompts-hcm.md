# GIS Tool Test Prompts – TP. Hồ Chí Minh

## Tọa độ tham chiếu
- **Bến Thành**: [106.698, 10.773]
- **Landmark 81**: [106.722, 10.795]
- **Nhà thờ Đức Bà**: [106.699, 10.780]
- **Tân Sơn Nhất**: [106.652, 10.819]
- **Polygon Quận 1**: [[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]
- **Tuyến đường Q1→LM81**: [[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]

---

## Measurement Tools (21 tools)

**1. along** – Điểm cách 2 km dọc tuyến Bến Thành → Landmark 81
```
Dùng along: line={"type":"LineString","coordinates":[[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]}, distance=2, options={"units":"kilometers"}
```

**2. area** – Diện tích polygon Quận 1
```
Dùng area: polygon={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**3. bbox** – Bounding box của 4 địa điểm
```
Dùng bbox: geojson={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{}}}]}
```

**4. bboxPolygon** – Chuyển bbox thành Polygon
```
Dùng bboxPolygon: bbox=[106.652,10.760,106.722,10.819]
```

**5. bearing** – Góc phương vị Bến Thành → Landmark 81
```
Dùng bearing: point1=[106.698,10.773], point2=[106.722,10.795]
```

**6. center** – Tâm của 4 địa điểm
```
Dùng center: geojson={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{}}]}
```

**7. centerOfMass** – Trọng tâm polygon Quận 1
```
Dùng centerOfMass: geojson={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**8. centroid** – Centroid polygon Quận 1
```
Dùng centroid: geojson={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**9. destination** – Từ Bến Thành đi 5 km hướng 45°
```
Dùng destination: origin=[106.698,10.773], distance=5, bearing=45, options={"units":"kilometers"}
```

**10. distance** – Khoảng cách Bến Thành → Tân Sơn Nhất
```
Dùng distance: point1=[106.698,10.773], point2=[106.652,10.819], options={"units":"kilometers"}
```

**11. envelope** – Envelope của tuyến đường
```
Dùng envelope: geojson={"type":"LineString","coordinates":[[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]}
```

**12. length** – Chiều dài tuyến Bến Thành → Landmark 81
```
Dùng length: geojson={"type":"LineString","coordinates":[[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]}, options={"units":"kilometers"}
```

**13. midpoint** – Điểm giữa Bến Thành và Landmark 81
```
Dùng midpoint: point1=[106.698,10.773], point2=[106.722,10.795]
```

**14. pointOnFeature** – Điểm trên polygon Quận 1
```
Dùng pointOnFeature: geojson={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**15. pointToLineDistance** – Khoảng cách Nhà thờ Đức Bà → tuyến đường
```
Dùng pointToLineDistance: point=[106.699,10.780], line={"type":"LineString","coordinates":[[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]}, options={"units":"kilometers"}
```

**16. rhumbBearing** – Rhumb bearing Bến Thành → Landmark 81
```
Dùng rhumbBearing: point1=[106.698,10.773], point2=[106.722,10.795]
```

**17. rhumbDestination** – Từ Bến Thành đi 3 km rhumb hướng 60°
```
Dùng rhumbDestination: origin=[106.698,10.773], distance=3, bearing=60, options={"units":"kilometers"}
```

**18. rhumbDistance** – Rhumb distance Bến Thành → Tân Sơn Nhất
```
Dùng rhumbDistance: point1=[106.698,10.773], point2=[106.652,10.819], options={"units":"kilometers"}
```

**19. square** – Bbox vuông chứa khu vực TP.HCM
```
Dùng square: bbox=[106.652,10.760,106.722,10.819]
```

**20. greatCircle** – Great circle Bến Thành → Landmark 81
```
Call MCP tool "greatCircle": start=[106.698,10.773], end=[106.722,10.795], options={"npoints":20}
```

**21. polygonTangents** – Tiếp tuyến từ Tân Sơn Nhất đến polygon Quận 1
```
Call MCP tool "polygonTangents": point=[106.652,10.819], polygon={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

---

## Boolean Tools (10 tools)

**1. booleanClockwise** – Ring có chiều kim đồng hồ không?
```
Dùng booleanClockwise: ring=[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]
```

**2. booleanContains** – Polygon Quận 1 có chứa Bến Thành không?
```
Dùng booleanContains: geojson1={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}, geojson2={"type":"Point","coordinates":[106.698,10.773]}
```

**3. booleanCrosses** – Tuyến đường có cắt qua polygon Quận 1 không?
```
Dùng booleanCrosses: geojson1={"type":"LineString","coordinates":[[106.680,10.770],[106.720,10.780]]}, geojson2={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**4. booleanDisjoint** – Tân Sơn Nhất có tách biệt khỏi polygon Quận 1 không?
```
Dùng booleanDisjoint: geojson1={"type":"Point","coordinates":[106.652,10.819]}, geojson2={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**5. booleanEqual** – Hai điểm Bến Thành có bằng nhau không?
```
Dùng booleanEqual: geojson1={"type":"Point","coordinates":[106.698,10.773]}, geojson2={"type":"Point","coordinates":[106.698,10.773]}
```

**6. booleanOverlap** – Hai polygon có chồng nhau không?
```
Dùng booleanOverlap: geojson1={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}, geojson2={"type":"Polygon","coordinates":[[[106.700,10.775],[106.730,10.775],[106.730,10.805],[106.700,10.805],[106.700,10.775]]]}
```

**7. booleanParallel** – Hai tuyến đường có song song không?
```
Dùng booleanParallel: line1={"type":"LineString","coordinates":[[106.690,10.770],[106.720,10.770]]}, line2={"type":"LineString","coordinates":[[106.690,10.780],[106.720,10.780]]}
```

**8. booleanPointInPolygon** – Nhà thờ Đức Bà có nằm trong polygon Quận 1 không?
```
Dùng booleanPointInPolygon: point=[106.699,10.780], polygon={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

**9. booleanPointOnLine** – Điểm [106.705, 10.778] có nằm trên tuyến đường không?
```
Dùng booleanPointOnLine: point=[106.705,10.778], line={"type":"LineString","coordinates":[[106.698,10.773],[106.705,10.778],[106.715,10.785],[106.722,10.795]]}
```

**10. booleanWithin** – Điểm Bến Thành có nằm trong polygon Quận 1 không?
```
Dùng booleanWithin: geojson1={"type":"Point","coordinates":[106.698,10.773]}, geojson2={"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]}
```

---

## Classification Tools (1 tool) – Test trên Claude Desktop

### Cách test trên Claude Desktop
Gõ câu hỏi tự nhiên bằng tiếng Việt, Claude sẽ tự gọi MCP tool `nearestPoint`.

---

**1. nearestPoint** – Tìm điểm gần nhất với Bến Thành trong danh sách địa điểm

**Prompt đơn giản:**
```
Trong danh sách các địa điểm tại TP.HCM sau đây, địa điểm nào gần Bến Thành (106.698, 10.773) nhất?
- Landmark 81: [106.722, 10.795]
- Nhà thờ Đức Bà: [106.699, 10.780]
- Tân Sơn Nhất: [106.652, 10.819]
```

**Prompt trực tiếp gọi tool:**
```
Dùng nearestPoint:
target_point={"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{}}
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
```

**Kết quả mong đợi:** Nhà thờ Đức Bà (~775 m), gần Bến Thành nhất.

---

**Biến thể test:**

**a. Tìm điểm gần Landmark 81 nhất:**
```
Dùng nearestPoint:
target_point={"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{}}
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
```
**Kết quả mong đợi:** Nhà thờ Đức Bà (~1.74 km).

**b. Tìm trạm metro gần Tân Sơn Nhất nhất (prompt ngôn ngữ tự nhiên):**
```
Tôi đang ở sân bay Tân Sơn Nhất (tọa độ 106.652, 10.819).
Hãy tìm điểm gần tôi nhất trong danh sách:
- Ga Metro Bến Thành: [106.698, 10.773]
- Ga Metro Tân Cảng: [106.729, 10.800]
- Ga Metro Suối Tiên: [106.826, 10.875]
```
**Kết quả mong đợi:** Ga Metro Bến Thành (~6.5 km).
