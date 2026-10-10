# VS Code拡張の利用

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

現在の拡張バージョンは0.0.5です。コマンド名は英語表記です。
現在はVSIXによるインストールのみです。Marketplaceでは公開していません。
VS Code 1.90以降と、GitHub Releasesの配布アーカイブに含まれる`zfj.exe`を使用します。
通常利用にNode.js・npmやF5での開発ホスト起動は不要です。

## 普段のVS Codeにインストール

1. GitHub Releasesから`zfj.exe`と`.vsix`を含む配布アーカイブをダウンロードし、任意のフォルダへ展開します。
2. 通常のVS CodeでCtrl+Shift+Pを押します。
3. `Extensions: Install from VSIX...`（拡張機能: VSIXからのインストール）を選びます。
4. 展開した`.vsix`ファイルを選択します。
5. 再読み込みを求められた場合は実行します。
6. `.sjs`・`.djs` を開いて補完・書き出し・SVGプレビューを使用できます。F5は不要です。

`zfj.exe`はVSIXの内部には含まれません。同じ配布アーカイブから展開した`zfj.exe`を、初回の書き出し・プレビュー・拡張子登録のいずれかで選択してください。
設定画面の`zfj.executablePath`に絶対パスを指定することもできます。
拡張機能の識別子は `zfj-local.zfj-vscode` です。`zfj-local` はローカル配布用の識別子で、
Marketplaceへの登録・公開は行っていません。ライセンスは[MIT](../LICENSE)です。

## SVG / DXFの書き出し

1. VS Codeでローカルの `.sjs` または `.djs` ファイルを開きます。
2. Ctrl+Shift+Pから `ZFJ: Export` を実行します。
3. 初回は `zfj.exe` を選びます。配布アーカイブから展開した実行ファイルを指定します。
4. 出力先を選びます。`.sjs` はSVG、`.djs` はDXFになります。
5. 既存の出力先の場合は上書きを確認します。

編集内容は実行前に保存されます。実行ログやエラーは「出力」パネルの `ZFJ` に表示します。
exeの場所はユーザー設定に保存します。変更する場合は設定画面で `zfj.executablePath` を検索してください。
未信頼のワークスペース、リモートファイル、未保存の新規ファイルからは実行できません。
コマンド引数はシェルを経由せずに渡します。プロセスの待機上限は30秒です。

## Windowsの拡張子登録・解除

ファイルを開かずに、コマンドパレットから実行できます。Windows上で、信頼されたワークスペースから実行してください。

### 登録する

1. Ctrl+Shift+Pで`ZFJ: Register File Associations`を実行します。
2. exeが未設定の場合は、配布アーカイブから展開した`zfj.exe`を選択します。
3. 完了通知を確認します。実行結果は「出力」パネルの`ZFJ`にも表示されます。

書き出し・プレビューと共通の`zfj.executablePath`設定を使用し、exeの`--install`を入力ファイルなしで呼び出します。
現在のWindowsユーザーに`.sjs`／`.djs`と専用アイコンを登録します。管理者権限は不要です。

ZFJに関連付けたファイルをダブルクリックするとJSを実行し、同じフォルダへSVG／DXFを生成します。
既存の出力ファイルは上書きしません。上書きしたい場合は`ZFJ: Export`から出力先を指定してください。
他アプリの既定設定は維持します。登録後も別のアプリで開く場合は、Windowsの「プログラムから開く」や「既定のアプリ」でZFJを選択してください。

### 解除する

1. exeを削除する前に、Ctrl+Shift+Pで`ZFJ: Unregister File Associations`を実行します。
2. 完了通知を確認します。

exeの`--uninstall`を呼び出し、ZFJの関連付けを解除します。
ソース・生成画像・exe・VS Code拡張は削除しません。拡張機能自体の削除はVS Codeの拡張機能画面から行います。
Windows側でZFJを既定に選択していた場合は、解除後に必要な既定アプリを選び直してください。

### exeを更新・移動したとき

`zfj.executablePath`を新しいexeの絶対パスに変更し、`ZFJ: Register File Associations`を実行し直してください。
これらのコマンドには`--install`／`--uninstall`対応のexeが必要です。
「Unknown option」などのエラーが出る場合は、設定したexeを確認して新しい配布版に切り替えます。
実行エラーは通知と「出力」パネルの`ZFJ`に表示されます。

CLIからの登録・解除については[コマンドライン仕様](cli.md)を参照してください。

## SVGプレビュー

1. VS Codeでローカルの `.sjs` を開きます。
2. Ctrl+Shift+Pから `ZFJ: SVG Preview` を実行します。
3. 未保存の変更を保存してzfjを実行し、右側に図形を表示します。
4. ソースを変更して保存すると、開いているプレビューを自動更新します。

初回の実行ファイル選択は書き出しと共通です。既存のSVGは上書きせず、
一時フォルダに生成して読み込み後に削除します。エラーが発生した場合は、
パネルにエラーと前回成功した画像を表示します。ログは「出力」の `ZFJ` に記録します。
プレビューを閉じると保存時の更新対象から外れます。複数ファイルのプレビューも開けます。
SVGは白い背景にパネル幅に収まる大きさで表示します。DXF、ズーム操作、
未保存の編集内容のリアルタイム表示には未対応です。

## 編集支援

VS Codeで `.sjs`・`.djs` を開くと、右下の言語モードが
`ZFJ JavaScript` になります。JavaScriptの文法を使って色分けし、括弧・引用符を補完します。

- `drawing.` または `path.` を入力すると、対応する関数の候補が出ます。出ない場合はCtrl+Spaceを押してください。
- 候補を選ぶと引数のひな形が挿入されます。Tabで次の引数へ移動できます。
- `drawing.draw`・`path.combine`・`path.star` などの関数名へマウスを重ねると、引数と日本語の説明を表示します。
- `zline`・`zrect`・`zcircle`・`zpolygon`・`zunit`・`zstyle`・`zcanvas`・`zfusion` を入力して
  Ctrl+Spaceを押すと、対応するスニペットを選べます。
- `zstyle` は描画関数の最後の引数に渡すオブジェクトです。
- `zpathline`・`zpathrect`・`zpathcircle`・`zpathpolygon` は描画せずパスを作成します。
- `zdraw` で作成済みパスを描画します。
- `zcombine`・`zsvgcombine` は既存のパス `a` と `b` を合成して描画します。
- `ztransform` は元パスを変更し、`ztransformed` は元パスを変えずコピーを変換します。
- `zregularpolygon`・`zstar` は正多角形・星型を作成して描画します。
- `zpolyline` は開いた折れ線を直接描画し、`zpathpolyline` はパスを作成します。塗り色は指定できません。
- `zdrawregularpolygon`・`zdrawstar` はパス変数を作らず直接描画します。
- `zmove`・`zmoved` はpath API形式、`zmethodmove`・`zmethodmoved` はパス値のメソッド形式で相対移動します。

`var p = path.circle(0, 0, 10);` のように宣言した変数では `p.` にmove/movedを補完します。
`var q = p.moved(30, 0);` のコピーや、同一行の `path.circle(0,0,10).moved(30,40).` も対象です。
簡易な構文照合のため、別名への代入・関数の戻り値・変数の再代入などの型推論は行いません。

`path`で作成した図形は `drawing.draw` を呼ぶまで表示されません。合成はSVG用で、
DXFでは元図形を出力します。`scale`は100が等倍の百分率、回転は度数です。
`canvasCenter`アンカーを使う場合は先に `drawing.setCanvas` を指定してください。

以前 `files.associations` に `.sjs`・`.djs` をJavaScriptとして登録した場合は、
その2つの設定を削除するか、値を `zfj-js` に変更してください。
JavaScriptモードでもdrawing / path APIの補完と説明は利用できますが、ZFJ用スニペットは
`ZFJ JavaScript` モードで提供します。

この段階ではJavaScript全般の型チェック、変数の補完、実行前の構文エラー表示、
引数入力中のシグネチャ表示は提供しません。APIの補完・説明は行内の記述を照合する簡易方式で、
コメントや文字列内でも候補が出る場合があります。

[開発・VSIXの作成](vscode-development.md) / [変更履歴](../zfj-vscode/CHANGELOG.md)

## Windowsの関連付けとVS Codeの設定

拡張の登録・解除コマンドは`zfj.exe --install`／`--uninstall`を呼び出し、Explorer用の関連付け・アイコンを操作します。
VSIXのインストール・削除や、VS Code内の言語モード設定は変更しません。
`files.associations`はVS Code内の言語指定であり、Windowsの既定アプリの設定とは別です。
