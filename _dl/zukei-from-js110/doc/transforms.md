# パスの移動・回転・拡縮

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## パスの移動・回転・拡縮

```javascript
var a = path.rect(10, 20, 40, 30);

// 元パスを変更する。戻り値はa自身。
path.transform(a, { pos: { x: 10, y: 5 } });

// 変換後の新しいパスを作る。aは変更しない。
var b = path.transformed(a, {
    pos: { x: 60, y: 0 },
    rot: 30,
    scale: { x: 150, y: 80 },
    anchor: "center"
});
drawing.draw(a);
drawing.draw(b);
```

| パラメータ | 意味 | 省略時 |
|---|---|---|
| `pos: {x, y}` または `[x, y]` | 現在位置からの相対移動。図面の入力単位 | x=0、y=0 |
| `rot: r` | 度数による回転。正は数値座標のX軸からY軸へ回る方向 | 0度 |
| `scale: {x, y}` または `[x, y]` | X/Y方向の倍率（%）。100が等倍、負数は反転、0は不可 | x=100、y=100 |
| `anchor: 名前` または `{x, y}` / `[x, y]` | 回転・拡縮の中心。座標指定は図面の絶対座標 | `"center"` |

`pos`と`scale`のオブジェクトでは片方の成分だけでも指定できます。配列は必ず2要素です。任意座標のanchorはx/yの両方が必要です。パラメータオブジェクト自体も省略できます。

| アンカー | 指定名 |
|---|---|
| 左上 | `"topLeft"` |
| 上 | `"top"` |
| 右上 | `"topRight"` |
| 左中 | `"centerLeft"` |
| 中心 | `"center"` |
| 右中 | `"centerRight"` |
| 左下 | `"bottomLeft"` |
| 下 | `"bottomCenter"` |
| 右下 | `"bottomRight"` |
| 固定キャンバスの中心 | `"canvasCenter"` |

`right`は`centerRight`の別名です。以前のハイフン区切り・日本語名も互換用に残しています。タイプミスの別名は受け付けません。

`canvasCenter`は先に`drawing.setCanvas(width, height)`が必要です。中心は`(width/2, height/2)`。キャンバスが自動サイズの場合は変換後の全図形で中心が変わるため、指定するとエラーにします。

SVG固定キャンバスの原点は左上`(0,0)`で、Xは右、Yは下へ増加します。右下は`(width,height)`。単位はmm/pt/pxの設定に従います。自動サイズでは図形の外接矩形に余白を付けるため、viewBoxの左上座標は負数になることもあります。
DXFはXが右、Yが上の座標系で、setCanvasは表示範囲を制限しません。ただしcanvasCenterのアンカー計算には設定寸法を使用できます。

- canvasCenter以外の名前付きアンカーは変換前の外接矩形（線幅を除く）を使います。上はY最小、下はY最大です。SVGでは正のrotが時計回り、DXFでは反時計回りに見えます。
- 適用順序は、アンカー中心のスケール→同じアンカー中心の回転→posによる移動です。複数回呼ぶと、その時点のパスへ順次適用します。
- 線幅・色・塗りは変更しません。変換後も各輪郭には共通の指定線幅が適用されます。
- 合成パスの場合、元図形全体の外接矩形から共通アンカーを求め、全オペランドへ変換します。ANDの表示領域だけの中心を求める処理ではありません。
- `drawing.draw()`や合成作成時は図形をコピーします。後で元パスを変更しても、描画済み図形・作成済み合成・別コピーは変わりません。
- 円の等倍率拡縮は円、縦横別倍率は楕円として保持します。SVGはellipseまたは円弧パス、通常DXFはELLIPSEと楕円HATCHです。長方形を斜め回転した場合は4頂点の多角形になります。
- 変換された長方形・円・楕円のXORも、SVGでは元輪郭の複合パスと共通線幅を維持します。Clipper2による合成では曲線を最大偏差0.01mmで近似します。
- Fusion輪郭モードでは楕円を最大偏差0.01mmの閉じたLWPOLYLINEに変換し、従来の輪郭だけの出力を維持します。
- 入力数値は有限数で絶対値1e9以下。変換後の外接矩形もこの範囲に制限します。不正な指定・範囲超過の場合、元パスは変更しません。

サンプル：`examples/transforms.sjs` / `examples/transforms.djs`。
検証：`python tests/transforms.py`（DXF監査には`tests/.deps`のezdxfを使用）。
実装：`zfj/path_transform.cpp`、JS登録は`zfj/drawing_api.cpp`。
DXFの楕円形式はAutodeskの[ELLIPSE仕様](https://help.autodesk.com/cloudhelp/2018/ENU/AutoCAD-DXF/files/GUID-107CB04F-AD4D-4D2F-8EC9-AC90888063AB.htm)と[HATCH境界仕様](https://help.autodesk.com/cloudhelp/2024/ENU/AutoCAD-DXF/files/GUID-DC5215D6-E73F-4DFF-8BE9-01CA9610FAEE.htm)に準拠します。

## 開いた折れ線と移動専用関数

```javascript
// 直接描画。最後の頂点から最初の頂点へは接続しない。
drawing.polyline([[10, 10], [40, 10], [40, 40]], {
    stroke: "#003366", strokeWidth: 0.5
});

// 描画せず、パスとして作成する。
var p = path.polyline([[10, 60], [40, 60], [40, 90]]);
var q = p.moved(50, 0); // 新しいパス。pは変更しない。
p.move(100, 0);         // p自身を変更する。
drawing.draw(p);
drawing.draw(q);

// 共通APIの形式でも呼び出せる。
path.move(p, 10, 20);
var r = path.moved(p, 30, 0);
```

| 関数 | 内容 |
|---|---|
| `drawing.polyline([[x,y],...], style?)` | 開いた折れ線を直接描画 |
| `path.polyline([[x,y],...], style?)` | 開いた折れ線パスを作成 |
| `p.move(x,y)` / `path.move(p,x,y)` | 相対移動で元パスを変更し、同じパスを返す |
| `p.moved(x,y)` / `path.moved(p,x,y)` | 相対移動した新しいパスを返す |

- polylineは2～100000頂点。polygonは従来どおり3頂点以上の閉じた輪郭です。
- polylineの線色・線幅は既存のstyleと共通。塗り色指定はエラーです。fill:null / "none"は使用できます。閉じた領域を持たないため、Clipper2とSVGの合成対象にはできません。
- SVGはpolyline、DXFは閉じフラグ0のLWPOLYLINEとして出力します。Fusionモードでも閉じずに保持します。
- move/movedはx/y両方の指定が必要です。移動量は現在の入力単位で、負数・0も使用できます。図形の種類、線幅、色、塗りは変えません。
- パス値のメソッドはすべての生成関数・合成結果・コピーで利用できます。`path.circle(0,0,10).moved(30,40)`のような連結も可能です。
- draw済みの図形には移動を反映しません。範囲超過などのエラー時には元パスを変更しません。

サンプル：`examples/polyline-move.sjs` / `examples/polyline-move.djs`。
検証：`python tests/polyline_move.py`。
