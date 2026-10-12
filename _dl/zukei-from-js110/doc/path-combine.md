# SVG表示合成とClipper2の輪郭合成

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## SVGの表示合成とClipper2のパス合成（AND / OR / XOR）

```javascript
var a = path.rect(0, 0, 100, 100);
var b = path.rect(25, 25, 75, 75);
// 元輪郭を残し、SVGの偶奇塗りまたはマスクで表示合成します。
var display = path.svgcombine("xor", a, b);
drawing.draw(display, { stroke: null, fill: "#66aaff" });

// 別の図面で使う場合：Clipper2で実際の輪郭を計算します。
// var geometry = path.combine("xor", a, b);
// drawing.draw(geometry, { stroke: "#003366", strokeWidth: 0.5, fill: "#66aaff" });
```

| API | SVGに保存するデータ | Illustratorでの確認対象 |
|---|---|---|
| `path.svgcombine(op, a, b, ...)` | 元輪郭の複合パス（対応するXOR）、または元図形・マスク・表示効果 | 元図形、マスク、表示効果 |
| `path.combine(op, a, b, ...)` | Clipper2で計算した輪郭のpath | 分割された輪郭、穴を含む複合パス |

Illustrator本体での取り込みは未検証です。マスク・フィルターの保持や変換はアプリ側に依存します。

- `path.line` / `path.rect` / `path.circle` / `path.polygon` は、既存のdrawing関数と同じ引数で図形を返します。この時点では描画されません。末尾にスタイルも指定できます。
- 両方の合成APIは図形を2つ以上受け取り、drawing.drawへ渡す値を返します。
- `drawing.draw(path, style?)` で図面に追加します。同じパスを再利用でき、drawのスタイル指定は元のパスを変更しません。
- `"or"` は和集合、`"and"` は全図形の共通部分、`"xor"` は排他的論理和です。
- rect / circle / polygonと、同じ方式の合成結果を対象にできます。svgcombineの中にcombineの結果を置くこともできます。
- combineの入力にsvgcombineの結果は指定できません。表示効果を輪郭計算へ暗黙に変換せず、エラーにします。
- line・polyline・部分円弧は閉じた領域がないため合成の対象にできません。線幅を領域へ変換する機能もありません。
- svgcombineはSVGの塗り規則・マスクによる表示合成です。Clipper2による輪郭計算は行わず、円・楕円の曲線を保持します。
- 円・楕円・長方形・生成した正多角形・星型・回転した長方形によるXOR（XORだけのネストを含む）は、元輪郭を1個の複合パスにまとめ、fill-rule="evenodd"で塗ります。円は円弧のまま保持し、共通のstroke-widthで全サブパスの輪郭を描画します。マスク・フィルターは使用しません。線は元の輪郭すべてに付き、重複して相殺された輪郭の線も残ります。
- OR / AND、任意頂点のpath.polygonや他の演算を含むXORは元図形とマスクによる既存方式です。以下のマスクの説明はこの方式に適用します。
- XORの補集合は白い背景と黒く塗った元図形で直接マスクに記述します。色変換フィルターは使用しません。塗りだけのsvgcombineではSVGフィルターを出力しません。
- svgcombineの描画対象は元のrect / circle / polygonです。全面の塗り用長方形には置換せず、OR / XORでは各元図形を通常の描画グループ内に出力します。ANDで切り抜く側の図形はマスクの定義内に残ります。
- 編集ソフトではグループ内の元図形を選択して編集します。マスク用の図形は別コピーのため、描画図形を変更してもマスクが自動更新されるライブ合成ではありません。必要に応じてマスク側も編集してください。アプリ側が読み込み時にマスクやフィルターを統合する場合の編集性は未検証です。
- マスク方式のsvgcombineでは塗りを元図形・マスクで表し、線はfeMorphologyによる表示効果です。線は計算済みパスのstrokeとは異なり、角や曲線の見え方、拡大時の品質に差が出ます。正確なベクター輪郭と線が必要ならcombineを使用してください。
- combineはSVG書き出し時にClipper2で輪郭を計算します。通常の描画関数で追加した図形は合成しません。
- 合成結果のスタイルはdrawing.drawの第2引数で指定します。省略時は黒線・実寸0.25mm・塗りなし。
- 合成内の各図形のスタイルはSVGの演算結果へ引き継ぎません。ネストした場合は外側のスタイルを使用します。
- combineの複数輪郭や穴は1つのSVG pathとNonZeroの塗り規則で保持します。ANDが空の場合は何も出力しません。
- combineでは円・楕円を最大偏差0.01mmの折れ線へ変換します。Clipper2の数値丸めは0.0001mm単位です。
- 円の近似が100000頂点を超える場合はエラーになります。ネストは最大32段。
- 単位・DPI・固定キャンバス・CLIによる単位上書きは合成にも適用します。
- DXFでは合成を無効にし、元の図形と各図形の元スタイルをそのまま出力します。Fusionモードも利用可能です。
- `--check` はcombineの輪郭計算とsvgcombineのSVG生成を検証します。SVGビューアーによるマスク描画は実行しません。ファイルは保存しません。
- 既存の `drawing.combine(operation, callback, style?)` は互換用のClipper2方式です。新規コードではpath.combineを使用してください。

サンプル：`examples/path-values.sjs` / `examples/path-values.djs`、`examples/combine.sjs` / `examples/combine.djs`。
Illustratorでデータの違いを比較するサンプル：`examples/svg-render-combine.sjs` と `examples/path-combine.sjs`。同じ長方形と円を同じ塗りでXORし、元図形を持つSVGと計算済み輪郭のSVGを書き出します。
検証：`python tests/paths.py`（Python標準ライブラリだけで実行可能）。
実装：`zfj/path_ops.cpp`（Clipper2）、`zfj/svg_composite.cpp`（SVG表示合成）。

Clipper2はBoost Software License 1.0です。`third_party/clipper2/LICENSE`を参照。
同梱リビジョン：`f9c5eb6e14a59f6f5d65fbfb3564519a561cf4fd`。

## 編集時の制約

複合パスXORの円は円弧のサブパスです。編集ソフトで必ず円オブジェクトとして扱われるわけではありません。
ANDの切り抜き側はマスク定義に残るため、通常の図形と同じ選択・編集ができるとは限りません。
オペランドの順序を入れ替えると、表示側に残る図形を変更できます。
マスク・フィルターの読み込みや編集性はソフトに依存します。
