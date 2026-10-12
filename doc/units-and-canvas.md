# 単位・DPI・キャンバス・座標系

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## 単位指定

JSコードの先頭で図面全体の単位を設定します。

```javascript
drawing.setUnit("pt"); // "mm" / "pt" / "px"
drawing.rect(0, 0, 72, 36);
```

優先順位は **コマンド引数 `--unit` > JS の `drawing.setUnit()` > 既定の mm** です。
`--unit mm` を明示すれば、JSでptを設定していてもmmになります。
設定は図面全体に適用し、複数回呼ぶ場合は最後の設定を使用します（途中で図形ごとに単位を切り替える機能ではありません）。
不正な単位は、コマンド引数で上書きする場合もエラーとして検出します。

```powershell
.\x64\Release\zfj.exe examples\basic.sjs --unit pt --force
.\x64\Release\zfj.exe examples\basic.djs --unit px --force
```

- `--unit mm`：既定。1単位 = 1mm。
- `--unit pt`：1単位 = 1/72インチ。`--unit px`：1単位 = 1/DPIインチ（既定72dpi）。
- 座標、四角形の幅・高さ、円の半径、polygonの全頂点に適用します。
- SVG は pt モードで width / height を `pt` 表記にし、viewBox 内の座標は入力値を保持します。
- px の実寸は `drawing.setDpi()` で指定します。SVG/CSSの標準px（96dpi）とは異なるため、自動キャンバスでは `pt`、固定キャンバスでは実寸に合わせて換算したCSS `px` として出力します。
- SVG の基本余白1mmと既定線幅0.25mmは、単位指定にかかわらず実寸を維持します。明示した線幅は入力単位で解釈します。


### ピクセルのDPI

```javascript
drawing.setUnit("px");
drawing.setDpi(96);
drawing.rect(0, 0, 96, 48); // 25.4mm × 12.7mm
```

`setDpi()` は図面全体のpx単位に適用します。既定72、1～1000000の有限数（小数可）を指定できます。
mm / pt には影響せず、pt は常に1/72インチです。複数回設定した場合は最後の値を使用します。
`--unit px` で上書きした場合もJSで設定したDPIを使用します。`--unit mm` / `pt` ならDPIを使用しません。
SVGの自動キャンバスはpt表記、固定キャンバスはCSS px表記で実寸を保持し、座標は入力値のままです。DXFはmmへ換算します。

## SVGの固定キャンバス

```javascript
drawing.setUnit("px");
drawing.setDpi(96);
drawing.setCanvas(1920, 1080);
drawing.circle(960, 540, 200, { fill: "#3366cc" });
```

- `setCanvas(width, height)` の幅・高さは図面の入力単位です。正の有限数、最大1e9。
- SVGのviewBoxを `0 0 width height` に固定し、自動余白は付けません。範囲外はクリップします。
- 未指定時は従来の外接矩形＋余白による自動サイズです。複数回指定した場合は最後の値を使用します。
- pxの固定キャンバスではSVGのwidth / heightをCSS px表記にします。CSS pxは96dpiなので、出力寸法は `指定寸法 × 96 / 設定DPI` です。
- 1920×1080pxの画面素材には上記のとおり96dpiを指定してください。72dpiの1920×1080単位なら、実寸を保つため2560×1440pxになります。
- mm / ptはその単位で出力します。CLIの `--unit` がキャンバス寸法にも優先されます。
- `setCanvas` はDXFの表示範囲を制限しません。ただし、パス変換の`canvasCenter`には設定寸法を使用します。
- サンプル：`examples/canvas.sjs`。

## 座標の方向

PDFはSVGから変換するため、上記の単位・DPI・キャンバス・クリップ範囲を引き継ぎます。
`setCanvas`を指定すればその寸法がページサイズになり、省略時は外接矩形＋余白の自動サイズになります。

SVGはXが右、Yが下。固定キャンバスの左上は`(0,0)`、右下は`(width,height)`です。
DXFはXが右、Yが上で、SVGとの間でY座標の反転は行いません。
正の回転はSVGでは時計回り、DXFでは反時計回りに見えます。
自動サイズのSVGではviewBoxの開始座標が負になる場合があります。
`canvasCenter`を使用する場合は、変換より先に`setCanvas`を呼び出してください。
その後にキャンバスを変更しても実行済みの変換は再計算されません。
