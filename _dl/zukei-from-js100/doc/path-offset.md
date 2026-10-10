# パスのオフセット（拡張・縮小）

[ドキュメント一覧](README.md) / [描画API](api.md)

## 呼び出し

```javascript
var expanded = path.offset(p, distance, { cap: "butt", join: "miter" });
drawing.draw(expanded, { stroke: "#003366", fill: "#66aaff" });
```

`path.offset(pathValue, distance, options?)`は、独立した新しいパスを返します。
元のパス・描画済みの図形は変更しません。結果を表示するには`drawing.draw()`を呼びます。
パス値の`.offset()`メソッドは提供しません。

## 開いたパス

line、polyline、部分円弧・楕円弧が対象です。
指定距離だけ中心線の両側へ広げ、閉じた輪郭を作ります。
距離5なら全幅は10です。距離は現在の入力単位で指定し、正数が必要です。
元パスのstrokeWidthは拡張距離に加算しません。

| cap | 始点・終点の形状 |
|---|---|
| `"butt"`（既定） | 元の端点から接線方向へ延長しない |
| `"square"` | 接線方向へdistanceだけ延長した四角形 |
| `"round"` | 半径distanceの半円で延長 |

同じcapを始点・終点へ適用します。

```javascript
var line = path.line(20, 30, 80, 30);
var band = path.offset(line, 5, { cap: "round" });
drawing.draw(band, { stroke: null, fill: "#66aaff" });
```

## 閉じたパス

長方形、多角形、正多角形、星型、全周の円・楕円、計算済みの輪郭が対象です。
正の距離で外へ拡張し、負の距離で内へ縮小します。0は独立したパスを返します。
穴がある場合、正数では穴が小さくなり、負数では穴が大きくなります。
頂点順や鏡面反転で拡張・縮小の意味は変わりません。

```javascript
var original = path.rect(20, 20, 60, 40);
var outer = path.offset(original, 5);
var inner = path.offset(original, -5);
drawing.draw(outer, { fill: "#cceeff" });
drawing.draw(original);
drawing.draw(inner);
```

`path.combine`の結果は、呼び出し時点で実際の輪郭を計算してオフセットします。
既存の`path.combine`単体は引き続きSVG出力時の演算で、DXFでは元図形を出力します。
`path.svgcombine`は表示合成なので、オフセット対象にはできません。暗黙に輪郭へ変換しません。

## 角の結合

| join | 結合 |
|---|---|
| `"miter"`（既定） | 辺を延長して結合。miter limitは2で、それ以上の鋭角はClipper2の処理で切り詰める |
| `"round"` | 丸い結合 |
| `"bevel"` | 斜めに切り落とした結合 |

capは開いたパスの端点に適用し、閉じたパスでは使用しません。
結果は元パスのスタイルを保持します。輪郭線の太さを変更するAPIではありません。

## エラーで停止する条件

不正な形状を勝手に修復せず、JS例外を発生させます。
捕捉しなければ終了コード1で停止し、出力ファイルを保存・上書きしません。

- 開いたパスに0以下の距離を指定した場合。
- 有限数でない距離、絶対値1e9を超える距離、不正なcap／join。
- ゼロ長のパス、面積0の閉じた輪郭、自己交差・重複辺・輪郭同士の接触。
- 縮小で輪郭が消滅・分裂した場合、拡張で別の輪郭が統合した場合。
- 穴が消滅するなど、輪郭数・包含深さの構成が変わった場合。
- 0.0001mmの計算精度では扱えない距離・形状、数値変換の失敗。
- 結果の座標が絶対値1e9を超える場合、入出力の合計頂点数が100000を超える場合。
- 交差・包含の検証や丸角の生成で処理量の上限を超える場合。

複数の独立した輪郭や穴を持つ入力は使用できますが、処理前後でその構成が変わる結果はエラーです。
隣接した重複点・閉じたパスの末尾に重ねた始点は整理してから検証します。

## 精度・単位・出力

Clipper2を使い、mm換算で小数4桁（0.0001mm）の格子上で計算します。
円・楕円の近似と丸端・丸角の近似は、それぞれ最大偏差0.01mmを使用します。
全周の円は解析的に半径を加減し、円として保持します。それ以外は複数輪郭・穴を持てる直線パスになります。

単位とDPIはオフセットを呼ぶ前に設定してください。
近似精度は呼び出し時点の単位・DPIを使用し、CLIの`--unit`がJS設定より優先します。
後から単位を変更しても計算済みの輪郭を再生成しません。最終出力時の単位換算は図面全体に適用します。

結果は移動・回転・拡縮・鏡面反転・再オフセットと、`path.combine`／`path.svgcombine`に使用できます。
SVGは穴を含む複合path、DXFは閉じたLWPOLYLINEと必要に応じて穴を含むHATCHで出力します。
円を保持した結果はcircle／CIRCLEです。Fusionモードでは輪郭だけを出力します。

サンプル：[offsets.sjs](../examples/offsets.sjs) / [offsets.djs](../examples/offsets.djs)。
実装：`zfj/path_offset.cpp`。検証：`python tests/offsets.py`。
