# 描画・パス作成API

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## 基本図形

以下の引数の`style?`は省略可能です。

| 直接描画 | パス作成 | 図形 |
|---|---|---|
| `drawing.line(x1,y1,x2,y2,style?)` | `path.line(x1,y1,x2,y2,style?)` | 線分 |
| `drawing.rect(x,y,width,height,style?)` | `path.rect(x,y,width,height,style?)` | 左上相当の座標と幅・高さによる長方形 |
| `drawing.circle(cx,cy,radiusOrOptions,style?)` | `path.circle(cx,cy,radiusOrOptions,style?)` | 円・楕円・部分円弧 |
| `drawing.polygon(points,style?)` | `path.polygon(points,style?)` | 自動的に閉じる多角形。3～100000頂点 |
| `drawing.polyline(points,style?)` | `path.polyline(points,style?)` | 開いた折れ線。2～100000頂点 |
| `drawing.regularPolygon(options?,style?)` | `path.regularPolygon(options?,style?)` | 正多角形 |
| `drawing.star(options?,style?)` | `path.star(options?,style?)` | 星型 |

`points`は`[[x,y], ...]`です。座標は有限数で絶対値1e9以下、幅・高さ・半径は正数です。
直接描画の戻り値は`undefined`。`path.*`は描画せず、アプリ専用のパス値を返します。
任意のJSオブジェクトをパスの代わりに渡すことはできません。

## 円・楕円・円弧

第3引数に数値を渡す従来形式はそのまま使用できます。
オブジェクト形式も`drawing.circle`／`path.circle`で共通です。スタイルは第4引数に指定します。

```javascript
// 従来の円
var circle = path.circle(30, 30, 20);
// オブジェクト形式の円
var same = path.circle(30, 30, { radius: 20 });
// 楕円（全周）
drawing.circle(85, 30, { radiusX: 30, radiusY: 15 }, { fill: "#66aaff" });
// 円弧：右端から下端までの90度
var arc = path.circle(30, 80, { radius: 20, start: 0, end: 90 });
drawing.draw(arc, { stroke: "#0066cc", strokeWidth: 1 });
// 楕円弧：逆方向
var ellipseArc = path.circle(85, 80, {
    radiusX: 30, radiusY: 15, start: 0, end: 270, clockwise: false
});
drawing.draw(ellipseArc);
```

| オプション | 意味 | 省略時 |
|---|---|---|
| `radius` | 円の半径。正の有限数 | radiusX / radiusYを指定しない場合は必須 |
| `radiusX`, `radiusY` | X / Y方向の半径。両方とも正の有限数 | 両方省略ならradiusを使用 |
| `start` | 開始角（度） | 0 |
| `end` | 終了角（度） | 360 |
| `clockwise` | trueで時計回り、falseで逆方向 | true |

- 半径は現在の入力単位、最大1e9です。`radiusX`／`radiusY`は必ず両方を指定します。両方あれば`radius`より優先します。
- 円弧の角度はSVG／DXFとも右方向が0°、画面上の下が90°です。`clockwise:true`は両形式で時計回りです。通常の座標系（SVGはYが下、DXFはYが上）は維持し、DXFでは角度から座標を求める際にY方向を反映します。これは`transform`の`rot`（座標系の正方向）とは別の角度規則です。
- 楕円の角度はパラメータ角です。軸平行なら`x=cx+radiusX*cos(t)`、SVGは`y=cy+radiusY*sin(t)`、DXFは`y=cy-radiusY*sin(t)`となり、中心から見た幾何学的な角度とは異なります。
- 指定方向にstartからendへ進みます。例えば`start:350,end:10`は既定方向で20°、`start:0,end:270,clockwise:false`は逆方向へ90°です。
- start / endの差の絶対値は360°以下、同値はエラーです。差が±360°なら全周です。角度は有限数で絶対値1e9以下です。
- 全周の円・楕円は閉じた図形で、従来どおり塗り・合成に対応します。
- 部分円弧は開いたパスです。始点と終点を結ばず、扇形にもなりません。色のあるfillやAND / OR / XORの入力に指定するとエラーです。`fill:null`／`"none"`は使用できます。
- 移動・コピー・回転・拡縮・鏡面反転に対応します。線幅は変更しません。
- SVGは全周をcircle / ellipse、部分円弧をAコマンドのpathで保持します。通常DXFは円弧をARC、楕円弧を部分ELLIPSEで保持します。Fusionでは楕円弧を最大偏差0.01mmの開いた折れ線へ近似します。

サンプル：[circle-arcs.sjs](../examples/circle-arcs.sjs) / [circle-arcs.djs](../examples/circle-arcs.djs)。

## パスを描画する

```javascript
var p = path.circle(30, 30, 15, { fill: "#66aaff" });
drawing.draw(p);
drawing.draw(p.moved(40, 0), { fill: "#ffcc66" });
```

`drawing.draw(pathValue, style?)`はパスをコピーして図面へ追加します。
スタイルは指定した項目だけを上書きし、元パスを変更しません。
後から元パスを移動しても描画済みの図形には反映しません。

## 設定・操作関数の参照

| 関数 | 詳細 |
|---|---|
| `drawing.setUnit(unit)` / `drawing.setDpi(dpi)` / `drawing.setCanvas(w,h)` | [単位・キャンバス](units-and-canvas.md) |
| `drawing.setDxfMode(mode)` | [DXF・Fusion出力](output-formats.md) |
| `path.offset(p,distance,options?)` | [輪郭の拡張・縮小、開いたパスの帯状化](path-offset.md) |
| `path.transform(p,params?)` / `path.transformed(p,params?)` | [変換](transforms.md) |
| `path.move(p,x,y)` / `path.moved(p,x,y)` / `p.move(x,y)` / `p.moved(x,y)` | [移動](transforms.md) |
| `path.svgcombine(op,a,b,...)` / `path.combine(op,a,b,...)` | [合成](path-combine.md) |
| `drawing.combine(op,callback,style?)` | [互換用の合成](path-combine.md) |

## 線幅・色・塗り

各描画関数の最後に省略可能なスタイルオブジェクトを指定します。

```javascript
drawing.setUnit("mm");
drawing.line(0, 0, 100, 0, { stroke: "#0066cc", strokeWidth: 0.5 });
drawing.rect(5, 10, 40, 25, {
    stroke: "#cc0000", strokeWidth: 1, fill: "#ffcc00"
});
drawing.circle(70, 25, 15, { stroke: null, fill: "#00aa66" });
drawing.polygon([[10, 50], [45, 45], [30, 75]], { fill: "#ccccff" });
```

| 指定 | 内容 |
|---|---|
| `stroke` | 線色。既定は黒。`#RRGGBB`、`null`、`"none"`を指定可能 |
| `strokeWidth` | 線幅。正の有限数。図面の入力単位を使用。省略時は実寸0.25mm |
| `fill` | 塗り色。既定はなし。`#RRGGBB`、`null`、`"none"`を指定可能 |

- `line`・`polyline`・部分円弧には色のあるfillを指定できません。`fill: null` / `"none"`は許容します。
- CSSの色名、透明度、グラデーション、破線は未対応です。
- SVGは図形ごとの属性で反映し、角はround結合です。
- DXFはTrueColorでRGB色を指定し、線幅は0～2.11mmの規定値のうち最も近い値（同距離では細い方）へ丸めます。
- DXFのfillは独立したソリッドHATCHで出力し、strokeがあれば別の輪郭エンティティを追加します。
- 円のHATCH境界は円弧で保持し、折れ線近似は行いません。塗りは単一輪郭のみで、穴は未対応です。
- CAD側の表示・印刷設定によって、線幅や塗りの見え方が変わる場合があります。

サンプル：`examples/styles.sjs` / `examples/styles.djs`。

スタイルの検証はPython + ezdxfで行います。テスト用の依存だけを `tests/.deps` に配置します。

```powershell
python -m pip install --target tests/.deps ezdxf
python tests/styles.py
```

## 正多角形・星型のパス

`path.regularPolygon(options?, style?)`と`path.star(options?, style?)`は、描画せずパスを返します。既存の`path.polygon([[x,y],...], style?)`は任意の頂点を指定する関数として変更しません。

`drawing.regularPolygon(options?, style?)`と`drawing.star(options?, style?)`は同じ引数・既定値で直接描画します。戻り値はundefinedです。

```javascript
drawing.regularPolygon({ center: [50, 50], sides: 6, radiusMode: "edge" }, {
    stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5
});
drawing.star({ center: [130, 50], points: 5 }, {
    stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5
});
```

```javascript
var hexagon = path.regularPolygon({
    center: { x: 50, y: 50 },
    radius: 30,
    radiusMode: "edge",
    sides: 6,
    rot: 0
});

var star = path.star({
    center: [130, 50],
    outerRadius: 30,
    innerRadius: 15,
    points: 5,
    rot: 0
});

drawing.draw(hexagon, { stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5 });
drawing.draw(star, { stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5 });
```

正多角形：

| オプション | 意味 | 省略時 |
|---|---|---|
| `center: {x,y}` / `[x,y]` | 中心座標 | `(0,0)` |
| `radius` | 中心から頂点、または辺の中心までの距離 | 30 |
| `radiusMode` | `"vertex"`は頂点まで、`"edge"`は辺の中心まで | `"vertex"` |
| `sides` | 辺の数。3～100000の整数 | 3 |
| `rot` | 中心を基準とする度数回転 | 0 |

星型：

| オプション | 意味 | 省略時 |
|---|---|---|
| `center: {x,y}` / `[x,y]` | 中心座標 | `(0,0)` |
| `outerRadius` | 外側の頂点までの距離 | 30 |
| `innerRadius` | 内側の谷までの距離 | 15 |
| `points` | 外側の頂点数。3～50000の整数 | 5 |
| `rot` | 中心を基準とする度数回転 | 0 |

- 半径の既定値30／15は現在の入力単位です。mmなら30mm／15mm、ptなら30pt／15pt、pxなら30px／15px。CLIによる単位上書きも適用します。
- 正多角形のradiusは正数。星型は`0 < innerRadius < outerRadius`が必要です。星型の輪郭の頂点数はpointsの2倍です。
- 回転0のvertexモードと星型は最初の頂点が上（Y負方向）を向きます。edgeモードは最初の辺の中心が上を向き、上辺が水平になります。SVGでは正のrotが時計回り、DXFでは反時計回りです。
- オプションはすべて省略可能。`path.regularPolygon()`は半径30の三角形、`path.star()`は外半径30・内半径15の5頂点星型です。styleは省略時に既存関数と同じ黒線・実寸0.25mm・塗りなしになります。
- パスの移動・コピー・回転・拡縮と、Clipper2合成に対応します。生成された正多角形・星型は自己交差のない輪郭なので、これらのSVG XORも偶奇規則の複合パスで保持し、通常strokeで共通線幅を適用します。
- SVGでは編集可能なpolygon、DXFでは閉じたLWPOLYLINEと必要に応じたHATCHとして保存します。Fusionモードにも対応します。

サンプル：`examples/radial-shapes.sjs` / `examples/radial-shapes.djs`。
検証：`python tests/radial_shapes.py`。実装：`zfj/path_shapes.cpp`。
