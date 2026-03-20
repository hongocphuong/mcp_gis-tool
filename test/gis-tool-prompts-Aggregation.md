# Aggregation Tool – Test Prompts trên Claude Desktop (TP. Hồ Chí Minh)

## Tọa độ tham chiếu
| Địa điểm | Tọa độ [lon, lat] | Khu vực |
|---|---|---|
| Bến Thành | [106.698, 10.773] | Quận 1 |
| Nhà thờ Đức Bà | [106.699, 10.780] | Quận 1 |
| Dinh Độc Lập | [106.695, 10.777] | Quận 1 |
| Hồ Con Rùa | [106.696, 10.791] | Quận 3 |
| Võ Thị Sáu (Q3) | [106.690, 10.797] | Quận 3 |
| Landmark 81 | [106.722, 10.795] | Bình Thạnh |
| Tân Sơn Nhất | [106.652, 10.819] | Tân Bình |
| Phú Mỹ Hưng A | [106.720, 10.730] | Quận 7 |
| Phú Mỹ Hưng B | [106.723, 10.728] | Quận 7 |
| Phú Mỹ Hưng C | [106.718, 10.732] | Quận 7 |

**Polygon Quận 1 (simplified):**
`[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]`

**Polygon Quận 3 (simplified):**
`[[[106.685,10.790],[106.715,10.790],[106.715,10.815],[106.685,10.815],[106.685,10.790]]]`

---

## Tool 1: collect
> Thu thập giá trị thuộc tính của các điểm rơi vào trong từng polygon.
> Tham số: `polygons`, `points`, `in_field` (field lấy từ point), `out_field` (field ghi vào polygon).

---

### Test 1.1 – Thu thập tên địa điểm theo quận

**Prompt tự nhiên:**
```
Tôi có 5 địa điểm tại TP.HCM và 2 vùng polygon (Quận 1 và Quận 3).
Hãy dùng tool "collect" để tổng hợp xem mỗi quận chứa những địa điểm nào (field "name").
```

**Prompt trực tiếp:**
```
Dùng collect:
polygons={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"district":"Quận 1"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.790],[106.715,10.790],[106.715,10.815],[106.685,10.815],[106.685,10.790]]]},"properties":{"district":"Quận 3"}}]}
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.696,10.791]},"properties":{"name":"Hồ Con Rùa"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.690,10.797]},"properties":{"name":"Võ Thị Sáu"}}]}
in_field="name"
out_field="places"
```

**Kết quả mong đợi:**
- Quận 1 → `places: ["Bến Thành", "Nhà thờ Đức Bà", "Dinh Độc Lập"]`
- Quận 3 → `places: ["Hồ Con Rùa", "Võ Thị Sáu"]`

---

### Test 1.2 – Thu thập loại hình địa điểm (category)

**Prompt trực tiếp:**
```
Dùng collect:
polygons={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"district":"Quận 1"}}]}
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành","category":"chợ"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà","category":"di tích"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập","category":"di tích"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81","category":"cao ốc"}}]}
in_field="category"
out_field="categories"
```

**Kết quả mong đợi:**
- Quận 1 → `categories: ["chợ", "di tích", "di tích"]`
- Landmark 81 nằm ngoài polygon → không được thu thập

---

## Tool 2: clustersDbscan
> Phân cụm điểm theo thuật toán DBSCAN (Density-Based Spatial Clustering).
> Tham số: `points`, `max_distance` (bán kính epsilon), `options.units`, `options.minPoints`.
> Kết quả: mỗi point có thêm `properties.cluster` (số cụm) hoặc `-1` nếu là noise.

---

### Test 2.1 – Phân cụm 3 vùng: Trung tâm, Phú Mỹ Hưng, Ngoại ô

**Prompt tự nhiên:**
```
Tôi có 7 địa điểm tại TP.HCM. Hãy dùng clustersDbscan để phân nhóm các điểm gần nhau
trong bán kính 2 km (units: kilometers, minPoints: 2).
Địa điểm: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (trung tâm Q1),
Phú Mỹ Hưng A/B/C (Quận 7), Tân Sơn Nhất (Tân Bình).
```

**Prompt trực tiếp:**
```
Dùng clustersDbscan:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng A"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.723,10.728]},"properties":{"name":"Phú Mỹ Hưng B"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.718,10.732]},"properties":{"name":"Phú Mỹ Hưng C"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
max_distance=2
options={"units":"kilometers","minPoints":2}
```

**Kết quả mong đợi:**
- Cụm 0: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (trong vòng ~1 km)
- Cụm 1: Phú Mỹ Hưng A, B, C (trong vòng ~600 m)
- Noise (-1): Tân Sơn Nhất (cô lập)

---

### Test 2.2 – Bán kính nhỏ hơn (0.5 km), minPoints = 2

**Prompt trực tiếp:**
```
Dùng clustersDbscan:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng A"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.723,10.728]},"properties":{"name":"Phú Mỹ Hưng B"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.718,10.732]},"properties":{"name":"Phú Mỹ Hưng C"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
max_distance=0.5
options={"units":"kilometers","minPoints":2}
```

**Kết quả mong đợi:**
- Cụm 0: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (vẫn trong 500 m)
- Cụm 1: Phú Mỹ Hưng A, B, C
- Noise (-1): Tân Sơn Nhất

---

### Test 2.3 – minPoints = 3 (tăng ngưỡng mật độ)

**Prompt trực tiếp:**
```
Dùng clustersDbscan:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng A"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.723,10.728]},"properties":{"name":"Phú Mỹ Hưng B"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
max_distance=2
options={"units":"kilometers","minPoints":3}
```

**Kết quả mong đợi:**
- Cụm 0: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (≥3 điểm trong 2 km)
- Noise (-1): Phú Mỹ Hưng A, Phú Mỹ Hưng B (chỉ 2 điểm, không đủ minPoints=3)
- Noise (-1): Tân Sơn Nhất

---

## Tool 3: clustersKmeans
> Phân cụm điểm theo thuật toán K-means.
> Tham số: `points`, `number_of_clusters` (số cụm K).
> Kết quả: mỗi point có thêm `properties.cluster` (0 đến K-1).

---

### Test 3.1 – Chia 9 địa điểm thành 3 cụm

**Prompt tự nhiên:**
```
Tôi có 9 địa điểm tại TP.HCM trải dài từ trung tâm ra ngoại ô.
Hãy dùng clustersKmeans với number_of_clusters=3 để chia thành 3 nhóm địa lý.
```

**Prompt trực tiếp:**
```
Dùng clustersKmeans:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.696,10.791]},"properties":{"name":"Hồ Con Rùa"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.690,10.797]},"properties":{"name":"Võ Thị Sáu"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng A"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.718,10.732]},"properties":{"name":"Phú Mỹ Hưng B"}}]}
number_of_clusters=3
```

**Kết quả mong đợi (có thể thay đổi vì K-means phụ thuộc vị trí centroid ban đầu):**
- Cụm 0: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (trung tâm Q1)
- Cụm 1: Hồ Con Rùa, Võ Thị Sáu, Landmark 81, Tân Sơn Nhất (phía bắc/đông)
- Cụm 2: Phú Mỹ Hưng A, B (phía nam)

---

### Test 3.2 – K=2 (chia đôi: trung tâm vs ngoại ô)

**Prompt trực tiếp:**
```
Dùng clustersKmeans:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng A"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.718,10.732]},"properties":{"name":"Phú Mỹ Hưng B"}}]}
number_of_clusters=2
```

**Kết quả mong đợi:**
- Cụm 0: Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập (trung tâm)
- Cụm 1: Tân Sơn Nhất, Phú Mỹ Hưng A, Phú Mỹ Hưng B (ngoại ô)

---

### Test 3.3 – K=1 (edge case: tất cả một cụm)

**Prompt trực tiếp:**
```
Dùng clustersKmeans:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
number_of_clusters=1
```

**Kết quả mong đợi:** Tất cả 3 điểm → `cluster: 0`

---

### Test 3.4 – K > số điểm (edge case: K tự động giảm)

**Prompt trực tiếp:**
```
Dùng clustersKmeans:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}}]}
number_of_clusters=5
```

**Kết quả mong đợi:** K tự động giảm về 2 (= số điểm), mỗi điểm một cụm riêng.
