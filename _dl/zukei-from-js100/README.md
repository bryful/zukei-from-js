# zfj

JavaScriptで図形を記述して、SVG・DXFを作成するWindows用コンソールアプリです。
入力ファイルの拡張子で出力形式を選びます。

- `.sjs` → SVG。イラストや画面素材向け。
- `.djs` → DXF。CAD向け。Fusion用の輪郭出力にも対応。

線分・長方形・円・楕円・円弧・多角形・折れ線・正多角形・星型を描けます。
色・線幅・塗り、パスの移動・回転・拡縮・オフセット、SVG用のAND / OR / XORにも対応しています。

## インストール

1. このリポジトリのGitHubページで **Releases** を開きます。
2. リリースの **Assets** から、`zfj.exe`とVS Code拡張の`.vsix`をまとめた配布アーカイブをダウンロードします。`Source code`は選びません。
3. アーカイブを任意のフォルダへ展開します。以降は`C:\Tools\zfj`に展開した例で説明します。

`zfj.exe`は展開した場所から使用できます。VS Code拡張を使用する場合は、続けて次の手順でインストールします。

### 拡張子とアイコンを登録する

展開先を決めたら、PowerShellで次を実行します。VS Code拡張を使う場合は、インストール後にコマンドパレットからも登録・解除できます。

```powershell
C:\Tools\zfj\zfj.exe --install
```

現在のユーザーに`.sjs`／`.djs`と専用アイコンを登録します。管理者権限は不要です。
関連付けられたファイルをダブルクリックするとJSを実行し、同じフォルダへSVG／DXFを書き出します。
既存の出力ファイルは上書きしません。エラーの確認や上書きはコマンドから実行してください。
別のアプリが既定になっている場合はその設定を維持するため、必要に応じてWindowsの「プログラムから開く」でZFJを選択してください。

exeの場所を変更した場合は、新しい場所の`zfj.exe --install`を実行し直します。
削除する場合は、exeを削除する前に次を実行してください。

```powershell
C:\Tools\zfj\zfj.exe --uninstall
```

解除するのはZFJのファイル関連付けです。ソース・生成画像・VS Code拡張は削除しません。

### VS Code拡張をインストールする

1. VS CodeでCtrl+Shift+Pを押し、`Extensions: Install from VSIX...`（拡張機能: VSIXからのインストール）を選びます。
2. 配布アーカイブから展開した`.vsix`ファイルを選択します。
3. 再読み込みを求められた場合は実行します。
4. VS Codeの設定で`zfj.executablePath`を検索し、展開した`zfj.exe`の絶対パス（例：`C:\Tools\zfj\zfj.exe`）を指定します。初回の書き出し時に選ぶこともできます。

インストール後、Ctrl+Shift+Pから`ZFJ: Register File Associations`を実行すると、`.sjs`／`.djs`とアイコンを登録できます。
解除は`ZFJ: Unregister File Associations`です。ファイルを開いていなくても実行できます。
初回にexeが未設定なら選択画面が開きます。`--install`／`--uninstall`対応の`zfj.exe`を指定してください。

拡張は現在VSIXでのみ提供しています。Marketplaceには公開していません。
通常利用にVisual Studio・Node.js・npmは不要です。

## 最初の図形を作る

以下をUTF-8の`sample.sjs`として保存します。

```javascript
drawing.setUnit("mm");
drawing.setCanvas(120, 80);

drawing.rect(10, 10, 50, 30, {
    stroke: "#003366", strokeWidth: 0.5, fill: "#66aaff"
});
drawing.circle(85, 30, 15, { fill: "#ffcc66" });
drawing.line(10, 60, 100, 60);
```

`sample.sjs`を保存したフォルダでPowerShellを開き、展開した`zfj.exe`を指定して実行します。

```powershell
C:\Tools\zfj\zfj.exe sample.sjs
```

同じ場所に`sample.svg`を生成します。DXFを作る場合はソースを`sample.djs`として保存して実行します。
SVGはYが下、DXFはYが上へ増えます。DXFではキャンバスによる切り抜きを行いません。

## 書き出しの指定

```powershell
C:\Tools\zfj\zfj.exe sample.sjs -o result.svg --force
C:\Tools\zfj\zfj.exe sample.sjs --check
C:\Tools\zfj\zfj.exe sample.djs --fusion
```

出力先の省略時は入力と同じフォルダ・同じ名前になります。
既存ファイルを上書きするときは`--force`を指定してください。`--check`は保存せずに実行・検証します。
Fusion向けには`--fusion`で色・塗り・線幅を除いた輪郭を出力できます。
[すべての引数](doc/cli.md)と[出力仕様](doc/output-formats.md)を参照してください。

## 単位と画面サイズ

既定はmmです。JSの先頭で`drawing.setUnit("pt")`または`drawing.setUnit("px")`に変更できます。
コマンドの`--unit`指定がJSより優先します。
画面用の1920×1080 SVGなら次の設定を使用します。

```javascript
drawing.setUnit("px");
drawing.setDpi(96);
drawing.setCanvas(1920, 1080);
drawing.circle(960, 540, 200, { fill: "#66aaff" });
```

キャンバス省略時は図形に合わせてサイズを決めます。
[単位・DPI・座標系の詳細](doc/units-and-canvas.md)を参照してください。

## パスを再利用する

`drawing.*`は直接描画し、`path.*`は描画前の図形を作ります。
`drawing.draw()`でパスを図面へ追加します。

```javascript
var p = path.star({ center: [40, 40], outerRadius: 20, innerRadius: 10 });
drawing.draw(p, { fill: "#ffcc66" });
drawing.draw(p.moved(50, 0), { fill: "#66aaff" });
```

[描画関数一覧](doc/api.md)、[移動・回転・拡縮](doc/transforms.md)、[SVGのパス合成](doc/path-combine.md)を参照してください。
SVG表示合成とClipper2の輪郭合成は、保存するデータと編集性が異なります。DXFでは両方とも元図形を出力します。

## VS Codeから使う

`zfj-vscode`は、色分け・API補完・スニペット・SVG/DXFの書き出し・SVGプレビュー・Windowsの拡張子登録を提供します。
**現在はVSIXでのインストールのみ**で、Marketplaceには公開していません。

インストール後、次の手順で書き出せます。

1. UTF-8で保存した`.sjs`または`.djs`をVS Codeで開きます。
2. Ctrl+Shift+Pから`ZFJ: Export`を実行します。
3. `zfj.exe`が未設定なら、アーカイブから展開した実行ファイルを選びます。
4. 出力先を指定します。

`.sjs`では`ZFJ: SVG Preview`で表示し、保存すると更新できます。
`drawing.`や`path.`を入力するとAPIの補完が利用できます。
コマンド名は英語表記です。登録・解除を含む実行には、信頼されたワークスペースが必要です。
[VS Codeでの使い方の詳細](doc/vscode.md)を参照してください。

## サンプルと詳しい資料

- [基本のSVG](examples/basic.sjs) / [DXF](examples/basic.djs)
- [色・線幅・塗り](examples/styles.sjs)
- [円・楕円・円弧](examples/circle-arcs.sjs)
- [正多角形・星型](examples/radial-shapes.sjs)
- [パスのオフセット](examples/offsets.sjs)（[仕様](doc/path-offset.md)）
- [折れ線・パス移動](examples/polyline-move.sjs)
- [SVG表示合成](examples/svg-render-combine.sjs) / [Clipper2の輪郭合成](examples/path-combine.sjs)
- [ドキュメント一覧](doc/README.md)：API仕様、出力形式などの詳細

## ライセンス・作者

本リポジトリは[MIT License](LICENSE)です。同梱ライブラリにはそれぞれのライセンスが適用されます。

bry-ful（Hiroshi Furuhashi） — [bryful](https://twitter.com/bryful)

[同梱ライブラリのライセンス](doc/dependencies.md)
