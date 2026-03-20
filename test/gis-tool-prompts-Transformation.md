# Transformation Tool – Test Prompts trên Claude Desktop (TP. Hồ Chí Minh)

## Tọa độ tham chiếu
| Địa điểm | Tọa độ [lon, lat] | Khu vực |
|---|---|---|
| Bến Thành | [106.698, 10.773] | Quận 1 |
| Nhà thờ Đức Bà | [106.699, 10.780] | Quận 1 |
| Dinh Độc Lập | [106.695, 10.777] | Quận 1 |
| Hồ Con Rùa | [106.696, 10.791] | Quận 3 |
| Landmark 81 | [106.722, 10.795] | Bình Thạnh |
| Tân Sơn Nhất | [106.652, 10.819] | Tân Bình |
| Phú Mỹ Hưng | [106.720, 10.730] | Quận 7 |
| Chợ Lớn | [106.662, 10.751] | Quận 5 |

**Polygon Quận 1 (simplified):**
`[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]`

**Polygon Quận 3 (simplified):**
`[[[106.685,10.790],[106.715,10.790],[106.715,10.815],[106.685,10.815],[106.685,10.790]]]`

**Polygon vùng chồng lấp (Quận 1 mở rộng):**
`[[[106.690,10.770],[106.720,10.770],[106.720,10.800],[106.690,10.800],[106.690,10.770]]]`

---

## Tool 1: bboxClip
> Cắt một Feature theo bounding box cho trước.
> Tham số: `feature` (Feature), `bbox` ([minX, minY, maxX, maxY]).

---

### Test 1.1 – Cắt đường Nguyễn Huệ theo bbox Quận 1

**Prompt tự nhiên:**
```
Tôi có một đường LineString dọc theo trục Nguyễn Huệ từ Bến Nhà Rồng đến Hồ Con Rùa.
Hãy dùng bboxClip để chỉ giữ lại phần đường nằm trong bbox của khu vực trung tâm Quận 1.
```

**Prompt trực tiếp:**
```
Dùng bboxClip:
feature={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.700,10.763],[106.699,10.773],[106.698,10.780],[106.697,10.791]]},"properties":{"name":"Nguyễn Huệ"}}
bbox=[106.695,10.768,106.705,10.785]
```

**Kết quả mong đợi:** LineString bị cắt, chỉ còn đoạn nằm trong bbox [106.695, 10.768, 106.705, 10.785].

---

### Test 1.2 – Cắt polygon Quận 1 theo bbox nhỏ hơn

**Prompt trực tiếp:**
```
Dùng bboxClip:
feature={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
bbox=[106.690,10.765,106.710,10.785]
```

**Kết quả mong đợi:** Polygon Quận 1 được cắt bớt, chỉ còn vùng nằm trong bbox đã cho.

---

## Tool 2: bezierSpline
> Chuyển đổi một đường LineString thành đường cong Bezier mượt mà.
> Tham số: `line` (LineString/Feature<LineString>), `options` (resolution, sharpness).

---

### Test 2.1 – Làm mượt tuyến đường từ Bến Thành đến Landmark 81

**Prompt tự nhiên:**
```
Tôi có một tuyến đường gấp khúc từ Bến Thành qua Nhà thờ Đức Bà đến Landmark 81.
Hãy dùng bezierSpline để tạo ra đường cong mượt hơn.
```

**Prompt trực tiếp:**
```
Dùng bezierSpline:
line={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.698,10.773],[106.699,10.780],[106.707,10.787],[106.722,10.795]]},"properties":{"name":"Bến Thành → Landmark 81"}}
options={"resolution":10000,"sharpness":0.85}
```

**Kết quả mong đợi:** LineString với nhiều điểm hơn, tạo đường cong mượt giữa 4 điểm gốc.

---

### Test 2.2 – Bezier mặc định (không options)

**Prompt trực tiếp:**
```
Dùng bezierSpline:
line={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.652,10.819],[106.680,10.800],[106.698,10.773]]},"properties":{"name":"Tân Sơn Nhất → Bến Thành"}}
```

**Kết quả mong đợi:** Đường cong Bezier với tham số mặc định.

---

## Tool 3: buffer
> Tạo vùng đệm (buffer) xung quanh một Feature.
> Tham số: `geojson` (Feature/FeatureCollection/Geometry), `radius` (số), `options` (units: meters/kilometers/miles).

---

### Test 3.1 – Buffer 500m quanh Bến Thành

**Prompt tự nhiên:**
```
Tạo vùng đệm 500 mét xung quanh khu vực Bến Thành tại TP.HCM.
```

**Prompt trực tiếp:**
```
Dùng buffer:
geojson={"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}}
radius=500
options={"units":"meters"}
```

**Kết quả mong đợi:** Polygon xấp xỉ hình tròn, bán kính ~500m, tâm tại Bến Thành.

---

### Test 3.2 – Buffer 1km quanh đường Nguyễn Huệ

**Prompt trực tiếp:**
```
Dùng buffer:
geojson={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.700,10.763],[106.699,10.773],[106.698,10.780]]},"properties":{"name":"Nguyễn Huệ"}}
radius=1
options={"units":"kilometers"}
```

**Kết quả mong đợi:** Polygon dọc theo đường Nguyễn Huệ, rộng 1km mỗi bên.

---

### Test 3.3 – Buffer âm (thu nhỏ polygon)

**Prompt trực tiếp:**
```
Dùng buffer để thu nhỏ polygon Quận 1 vào trong 500 mét:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
radius=-500
options={"units":"meters"}
```

**Kết quả mong đợi:** Polygon nhỏ hơn polygon gốc, thu vào ~500m mỗi chiều.

---

## Tool 4: circle
> Tạo polygon hình tròn từ một điểm trung tâm và bán kính.
> Tham số: `center` ([lon,lat] hoặc Point), `radius` (số), `options` (steps, units).

---

### Test 4.1 – Tạo vòng tròn 1km quanh Nhà thờ Đức Bà

**Prompt tự nhiên:**
```
Vẽ một vòng tròn bán kính 1 km lấy tâm là Nhà thờ Đức Bà tại TP.HCM.
```

**Prompt trực tiếp:**
```
Dùng circle:
center=[106.699,10.780]
radius=1
options={"steps":64,"units":"kilometers"}
```

**Kết quả mong đợi:** Polygon 64 cạnh, bán kính ~1km, tâm tại Nhà thờ Đức Bà.

---

### Test 4.2 – Vòng tròn 2km quanh Tân Sơn Nhất (ít điểm)

**Prompt trực tiếp:**
```
Dùng circle:
center={"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}
radius=2
options={"steps":16,"units":"kilometers"}
```

**Kết quả mong đợi:** Polygon 16 cạnh, bán kính ~2km quanh sân bay.

---

## Tool 5: clone
> Tạo bản sao sâu (deep copy) của một GeoJSON object.
> Tham số: `geojson`.

---

### Test 5.1 – Sao chép Feature Bến Thành

**Prompt tự nhiên:**
```
Hãy clone Feature điểm Bến Thành để tôi có một bản sao độc lập.
```

**Prompt trực tiếp:**
```
Dùng clone:
geojson={"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành","district":"Quận 1","type":"landmark"}}
```

**Kết quả mong đợi:** Feature giống hệt input, là bản sao độc lập.

---

## Tool 6: concave
> Tính concave hull (bao lồi lõm) của tập điểm.
> Tham số: `points` (FeatureCollection<Point>), `options` (maxEdge, units).

---

### Test 6.1 – Concave hull 5 địa điểm trung tâm TP.HCM

**Prompt tự nhiên:**
```
Tôi có 5 địa điểm nổi tiếng tại TP.HCM (Bến Thành, Nhà thờ Đức Bà, Dinh Độc Lập, Hồ Con Rùa, Landmark 81).
Hãy tính concave hull của nhóm điểm này.
```

**Prompt trực tiếp:**
```
Dùng concave:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.696,10.791]},"properties":{"name":"Hồ Con Rùa"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}}]}
options={"maxEdge":3,"units":"kilometers"}
```

**Kết quả mong đợi:** Polygon bao sát quanh 5 điểm, ôm sát hơn convex hull.

---

## Tool 7: convex
> Tính convex hull (bao lồi) của tập điểm.
> Tham số: `points` (FeatureCollection<Point>).

---

### Test 7.1 – Convex hull 6 địa điểm TP.HCM

**Prompt tự nhiên:**
```
Hãy vẽ convex hull (bao lồi) của 6 địa điểm sau tại TP.HCM:
Bến Thành, Nhà thờ Đức Bà, Landmark 81, Tân Sơn Nhất, Phú Mỹ Hưng, Chợ Lớn.
```

**Prompt trực tiếp:**
```
Dùng convex:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.662,10.751]},"properties":{"name":"Chợ Lớn"}}]}
```

**Kết quả mong đợi:** Polygon lồi bao quanh toàn bộ 6 điểm.

---

## Tool 8: difference
> Tính phần còn lại của polygon thứ nhất sau khi trừ đi phần giao với polygon thứ hai.
> Tham số: `featureCollection` (FeatureCollection gồm 2 Polygon, poly đầu trừ đi poly sau).

---

### Test 8.1 – Quận 1 trừ vùng chồng lấp

**Prompt tự nhiên:**
```
Tôi có polygon Quận 1 và một vùng chồng lấp ở phía đông Quận 1.
Hãy dùng difference để lấy phần Quận 1 không bị chồng lấp.
```

**Prompt trực tiếp:**
```
Dùng difference:
featureCollection={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.705,10.760],[106.720,10.760],[106.720,10.790],[106.705,10.790],[106.705,10.760]]]},"properties":{"name":"Vùng chồng lấp"}}]}
```

**Kết quả mong đợi:** Polygon Quận 1 với phần phía đông đã bị cắt bỏ.

---

## Tool 9: dissolve
> Hòa tan (merge) các polygon liền kề trong một FeatureCollection.
> Tham số: `featureCollection`, `options` (propertyName để nhóm).

---

### Test 9.1 – Gộp Quận 1 và Quận 3 thành một vùng liên tục

**Prompt tự nhiên:**
```
Tôi có hai polygon Quận 1 và Quận 3 nằm liền kề nhau.
Hãy dùng dissolve để gộp chúng thành một vùng duy nhất.
```

**Prompt trực tiếp:**
```
Dùng dissolve:
featureCollection={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"district":"Quận 1","region":"Trung tâm"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.790],[106.715,10.790],[106.715,10.815],[106.685,10.815],[106.685,10.790]]]},"properties":{"district":"Quận 3","region":"Trung tâm"}}]}
options={"propertyName":"region"}
```

**Kết quả mong đợi:** Một polygon duy nhất bao gồm cả Quận 1 và Quận 3.

---

## Tool 10: intersect
> Tính vùng giao nhau của hai polygon.
> Tham số: `featureCollection` (FeatureCollection gồm 2 Polygon).

---

### Test 10.1 – Giao của Quận 1 và vùng mở rộng

**Prompt tự nhiên:**
```
Tôi có polygon Quận 1 và một polygon vùng trung tâm mở rộng. Hãy tính vùng giao nhau của chúng.
```

**Prompt trực tiếp:**
```
Dùng intersect:
featureCollection={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.690,10.770],[106.720,10.770],[106.720,10.800],[106.690,10.800],[106.690,10.770]]]},"properties":{"name":"Vùng mở rộng"}}]}
```

**Kết quả mong đợi:** Polygon vùng giao nhau giữa hai polygon, khoảng [106.690,10.770] → [106.715,10.790].

---

## Tool 11: lineOffset
> Tạo đường song song cách đường gốc một khoảng cố định.
> Tham số: `line` (LineString/Feature<LineString>), `distance` (số, dương=trái, âm=phải), `options` (units).

---

### Test 11.1 – Tạo làn đường song song cách 50m

**Prompt tự nhiên:**
```
Tôi có đường Nguyễn Huệ theo hướng Bắc-Nam. Hãy tạo một đường song song cách 50 mét về phía trái để mô phỏng làn đường ngược chiều.
```

**Prompt trực tiếp:**
```
Dùng lineOffset:
line={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.700,10.763],[106.699,10.773],[106.698,10.780],[106.697,10.791]]},"properties":{"name":"Nguyễn Huệ"}}
distance=50
options={"units":"meters"}
```

**Kết quả mong đợi:** LineString song song, dịch sang trái 50m so với đường gốc.

---

### Test 11.2 – Offset âm (dịch sang phải)

**Prompt trực tiếp:**
```
Dùng lineOffset:
line={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.700,10.763],[106.699,10.773],[106.698,10.780]]},"properties":{"name":"Đường mẫu"}}
distance=-100
options={"units":"meters"}
```

**Kết quả mong đợi:** LineString dịch sang phải 100m.

---

## Tool 12: simplify
> Đơn giản hóa geometry, giảm số điểm.
> Tham số: `geojson`, `options` (tolerance, highQuality).

---

### Test 12.1 – Đơn giản hóa polygon phức tạp

**Prompt tự nhiên:**
```
Tôi có một polygon nhiều điểm mô phỏng đường bờ sông Sài Gòn. Hãy simplify để giảm bớt điểm mà vẫn giữ hình dạng tổng thể.
```

**Prompt trực tiếp:**
```
Dùng simplify:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.698,10.773],[106.700,10.774],[106.702,10.776],[106.703,10.779],[106.702,10.782],[106.700,10.785],[106.698,10.787],[106.695,10.786],[106.693,10.783],[106.692,10.780],[106.693,10.777],[106.695,10.774],[106.698,10.773]]]},"properties":{"name":"Khu vực Quận 1 chi tiết"}}
options={"tolerance":0.001,"highQuality":false}
```

**Kết quả mong đợi:** Polygon ít điểm hơn, giữ hình dạng tổng thể.

---

### Test 12.2 – Simplify chất lượng cao

**Prompt trực tiếp:**
```
Dùng simplify:
geojson={"type":"Feature","geometry":{"type":"LineString","coordinates":[[106.652,10.819],[106.660,10.812],[106.668,10.806],[106.675,10.800],[106.681,10.795],[106.688,10.788],[106.694,10.782],[106.698,10.773]]},"properties":{"name":"Tuyến bus Tân Sơn Nhất - Bến Thành"}}
options={"tolerance":0.0005,"highQuality":true}
```

**Kết quả mong đợi:** LineString ít điểm hơn, giữ hướng tổng thể của tuyến đường.

---

## Tool 13: tesselate
> Chia polygon thành các tam giác (triangulation).
> Tham số: `polygon` (Polygon Feature).

---

### Test 13.1 – Chia polygon Quận 1 thành tam giác

**Prompt tự nhiên:**
```
Hãy chia polygon Quận 1 thành các tam giác nhỏ bằng tesselate.
```

**Prompt trực tiếp:**
```
Dùng tesselate:
polygon={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
```

**Kết quả mong đợi:** FeatureCollection gồm các tam giác lấp đầy polygon Quận 1 (hình vuông → 2 tam giác).

---

## Tool 14: transformRotate
> Xoay một GeoJSON object quanh một tâm điểm.
> Tham số: `geojson`, `angle` (độ, dương=ngược chiều kim đồng hồ), `options` (pivot, mutate).

---

### Test 14.1 – Xoay polygon 45 độ quanh tâm

**Prompt tự nhiên:**
```
Xoay polygon Quận 1 đi 45 độ ngược chiều kim đồng hồ quanh tâm của nó.
```

**Prompt trực tiếp:**
```
Dùng transformRotate:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
angle=45
```

**Kết quả mong đợi:** Polygon được xoay 45° ngược chiều kim đồng hồ quanh tâm của nó.

---

### Test 14.2 – Xoay 90 độ quanh Bến Thành

**Prompt trực tiếp:**
```
Dùng transformRotate:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Vùng test"}}
angle=90
options={"pivot":[106.698,10.773]}
```

**Kết quả mong đợi:** Polygon được xoay 90° quanh điểm Bến Thành [106.698, 10.773].

---

## Tool 15: transformTranslate
> Di chuyển một GeoJSON object theo hướng và khoảng cách cho trước.
> Tham số: `geojson`, `distance` (số), `direction` (góc bearing, 0=Bắc), `options` (units, zTranslation, mutate).

---

### Test 15.1 – Dịch chuyển Bến Thành 1km về phía Bắc

**Prompt tự nhiên:**
```
Di chuyển Feature điểm Bến Thành về phía Bắc 1 km.
```

**Prompt trực tiếp:**
```
Dùng transformTranslate:
geojson={"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}}
distance=1
direction=0
options={"units":"kilometers"}
```

**Kết quả mong đợi:** Điểm dịch chuyển ~1km về phía Bắc, tọa độ lat tăng thêm ~0.009°.

---

### Test 15.2 – Dịch chuyển polygon 500m về phía Đông Nam

**Prompt trực tiếp:**
```
Dùng transformTranslate:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
distance=500
direction=135
options={"units":"meters"}
```

**Kết quả mong đợi:** Polygon dịch ~500m về hướng Đông Nam (bearing 135°).

---

## Tool 16: transformScale
> Phóng to hoặc thu nhỏ một GeoJSON object theo hệ số.
> Tham số: `geojson`, `factor` (>1=phóng to, <1=thu nhỏ), `options` (origin, mutate).

---

### Test 16.1 – Phóng to polygon Quận 1 gấp đôi

**Prompt tự nhiên:**
```
Phóng to polygon Quận 1 lên gấp đôi kích thước hiện tại.
```

**Prompt trực tiếp:**
```
Dùng transformScale:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
factor=2
```

**Kết quả mong đợi:** Polygon rộng gấp đôi, tâm giữ nguyên.

---

### Test 16.2 – Thu nhỏ xuống 50%

**Prompt trực tiếp:**
```
Dùng transformScale:
geojson={"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}}
factor=0.5
options={"origin":"centroid"}
```

**Kết quả mong đợi:** Polygon nhỏ bằng 50% gốc, co vào từ tâm.

---

## Tool 17: union
> Hợp nhất (union) hai polygon thành một.
> Tham số: `featureCollection` (FeatureCollection gồm 2 Polygon).

---

### Test 17.1 – Union Quận 1 và Quận 3

**Prompt tự nhiên:**
```
Hợp nhất hai polygon Quận 1 và Quận 3 thành một vùng duy nhất bằng union.
```

**Prompt trực tiếp:**
```
Dùng union:
featureCollection={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.790],[106.715,10.790],[106.715,10.815],[106.685,10.815],[106.685,10.790]]]},"properties":{"name":"Quận 3"}}]}
```

**Kết quả mong đợi:** Polygon hình chữ nhật lớn bao gồm cả Quận 1 lẫn Quận 3, đường biên chung được xóa bỏ.

---

### Test 17.2 – Union hai polygon chồng lấp

**Prompt trực tiếp:**
```
Dùng union:
featureCollection={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.685,10.760],[106.715,10.760],[106.715,10.790],[106.685,10.790],[106.685,10.760]]]},"properties":{"name":"Quận 1"}},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[106.690,10.770],[106.720,10.770],[106.720,10.800],[106.690,10.800],[106.690,10.770]]]},"properties":{"name":"Vùng mở rộng"}}]}
```

**Kết quả mong đợi:** Polygon hợp nhất của cả hai vùng, không có phần trùng lặp.

---

## Tool 18: voronoi
> Tạo các polygon Voronoi từ tập điểm.
> Tham số: `points` (FeatureCollection<Point>), `options` (bbox).

---

### Test 18.1 – Voronoi 5 địa điểm TP.HCM

**Prompt tự nhiên:**
```
Tôi có 5 địa điểm tại TP.HCM (Bến Thành, Nhà thờ Đức Bà, Landmark 81, Phú Mỹ Hưng, Tân Sơn Nhất).
Hãy tạo biểu đồ Voronoi để xem vùng ảnh hưởng của mỗi địa điểm.
```

**Prompt trực tiếp:**
```
Dùng voronoi:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.699,10.780]},"properties":{"name":"Nhà thờ Đức Bà"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.722,10.795]},"properties":{"name":"Landmark 81"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.720,10.730]},"properties":{"name":"Phú Mỹ Hưng"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.652,10.819]},"properties":{"name":"Tân Sơn Nhất"}}]}
options={"bbox":[106.60,10.70,106.80,10.90]}
```

**Kết quả mong đợi:** FeatureCollection gồm 5 polygon Voronoi, mỗi polygon bao quanh một địa điểm.

---

### Test 18.2 – Voronoi mà không chỉ định bbox

**Prompt trực tiếp:**
```
Dùng voronoi:
points={"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[106.698,10.773]},"properties":{"name":"Bến Thành"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.662,10.751]},"properties":{"name":"Chợ Lớn"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.695,10.777]},"properties":{"name":"Dinh Độc Lập"}},{"type":"Feature","geometry":{"type":"Point","coordinates":[106.696,10.791]},"properties":{"name":"Hồ Con Rùa"}}]}
```

**Kết quả mong đợi:** FeatureCollection 4 polygon Voronoi với bbox mặc định (-180,-90,180,90).

---

## Tổng hợp – Test nâng cao (kết hợp nhiều tool)

### Test A – Pipeline: buffer → intersect → simplify
```
1. Tạo buffer 800m quanh Bến Thành
2. Intersect với polygon Quận 1
3. Simplify kết quả với tolerance 0.0005
```

### Test B – Pipeline: convex → bboxClip
```
Tạo convex hull của 6 địa điểm TP.HCM,
sau đó clip kết quả vào bbox [106.65, 10.75, 106.73, 10.82].
```

### Test C – Pipeline: circle → difference
```
1. Tạo circle 1.5km quanh Nhà thờ Đức Bà
2. Tính difference: Quận 1 trừ đi circle đó
→ Kết quả là vùng Quận 1 ngoại trừ khu vực 1.5km quanh nhà thờ.
```
