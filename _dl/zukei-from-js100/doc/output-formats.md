# SVG・DXFの出力仕様

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## SVG

基本図形はline / rect / circle / polygon / polyline、変換後の楕円はellipseまたは円弧パスとして保存します。
線・塗りは図形ごとの属性で指定し、線の結合はroundです。
[固定キャンバスと実寸](units-and-canvas.md)、[合成データの構造](path-combine.md)も参照してください。

## 通常のDXF

AutoCAD 2004 ASCII（AC1018）。指定単位からmmへ換算し、`$INSUNITS=4`を設定します。
LINE / CIRCLE / ARC / ELLIPSE / LWPOLYLINE / ソリッドHATCHを使用し、テーブル・ブロック・ハンドルを出力します。
polygonは閉じたLWPOLYLINE、polylineは閉じフラグ0のLWPOLYLINEです。
色・線幅・塗りについては[スタイル仕様](api.md)を参照してください。
パスのAND / OR / XORは無効で、元図形と各図形の元スタイルを出力します。
合成全体への`drawing.draw`のスタイル指定はSVG用です。

## Fusion向け輪郭モード

JSで指定する方法：

```javascript
drawing.setDxfMode("fusion"); // "standard" で通常モードへ戻す
drawing.rect(0, 0, 100, 60);
drawing.circle(50, 30, 15, { stroke: null, fill: "#ff0000" });
```

コマンド引数で指定する方法：

```powershell
.\x64\Release\zfj.exe examples\styles.djs --fusion --force
```

- `--fusion` はJSの設定より優先し、`.djs`だけで使用できます。
- LINE / CIRCLE / ARC / 開いたまたは閉じたLWPOLYLINEだけを出力し、HATCH・図形の色・線幅指定は除きます。
- 塗りだけの図形も輪郭として出力します。線・塗りの両方がない不可視図形は省略します。
- 太い線を帯状の形状へ変換する機能ではありません。線の中心線を保持します。
- DXFの単位は通常モードと同じmm。Fusionでは「挿入 > DXFを挿入」で単位をmmに指定してください。
- 既定は通常モードで、従来の色・線幅・塗り出力を維持します。
- サンプル：`examples/fusion.djs`。

SVGの寸法・クリップ属性、DXFの独立リーダーによる読み込み・監査を検証しています。
Illustrator / After Effects / Fusionのアプリ本体での読み込みは未検証です。

Fusion輪郭モードでは楕円を最大偏差0.01mmの閉じた折れ線へ近似します。
ブラウザーでの表示確認やDXFリーダーによる監査は、各編集ソフトのインポート互換性の保証ではありません。
特にSVGのマスク・フィルターは、使用するソフトで表示と編集を確認してください。

部分円弧はSVGの円弧path、DXFのARC／部分ELLIPSEで保存します。
Fusionモードでは楕円弧を開いたLWPOLYLINEに近似します。
開始・終了角、方向、塗りの制限は[円・楕円・円弧API](api.md#円楕円円弧)を参照してください。

[オフセット](path-offset.md)はSVG／DXF両方で有効です。計算済みの複数輪郭は閉じたLWPOLYLINE、塗りは穴を含むHATCHとして出力します。
Fusionモードでは各輪郭だけを出力します。
