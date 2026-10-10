# ZFJ VS Code拡張

`.sjs` / `.djs`の色分け、drawing / path APIの補完・説明、スニペット、SVG / DXF書き出し、SVGプレビュー、Windowsの拡張子・アイコン登録を提供します。

## インストール

GitHub Releasesからzfj.exeとVSIXを含む配布アーカイブをダウンロードして展開します。
VS CodeでCtrl+Shift+P → `Extensions: Install from VSIX...`を選び、展開したVSIXを指定してください。
現在はVSIXによる提供のみです。Marketplaceには公開していません。

## 初回設定

設定画面で `zfj.executablePath` を検索し、zfj.exeの絶対パスを指定してください。
初回のコマンド実行時に選択することもできます。zfj.exeはVSIXに含まれません。

## コマンド

Ctrl+Shift+Pから実行します。

- **ZFJ: Export** — 開いている.sjs / .djsを保存し、SVG / DXFへ書き出します。
- **ZFJ: SVG Preview** — .sjsの図形を横に表示し、保存時に更新します。
- **ZFJ: Register File Associations** — zfj.exe --installで現在のWindowsユーザーに.sjs / .djsと専用アイコンを登録します。
- **ZFJ: Unregister File Associations** — zfj.exe --uninstallでZFJの関連付けを解除します。

登録・解除はWindows上の信頼されたワークスペースから、ファイルを開かずに実行できます。
管理者権限は不要です。exeが未設定なら選択画面が開きます。
登録後、ZFJに関連付けられたファイルをダブルクリックするとJSを実行してSVG / DXFを生成します。
他アプリの既定設定は維持します。必要に応じてWindowsの「プログラムから開く」でZFJを選択してください。exeを移動したら設定を変更して再登録してください。
登録の削除はexeを削除する前に実行してください。ソース・画像・exe・拡張機能は削除しません。
登録機能には--install / --uninstall対応の新しいzfj.exeが必要です。

## 編集

`drawing.` / `path.` を入力すると関数の候補を表示します。Ctrl+Spaceでも補完できます。
関数名にマウスを重ねると説明を表示します。`zrect`・`zstar`・`zpolyline`などのスニペットも利用できます。

- zellipse・zarc・zellipsearcで楕円・円弧・楕円弧を直接描画します。
- zpathellipse・zpatharc・zpathellipsearcで対応するパスを作成します。
- zoffsetでパスを拡張・縮小、または開いたパスを帯状化して描画します。閉じたパスの縮小は負の距離を指定します。

円弧の角度は0が右、90が下。部分円弧には塗り色を指定できません。path.offsetは独立したパスを返し、パス値の.offset()は提供しません。新API対応のzfj.exeが必要です。

実行ログやエラーは「出力」パネルのZFJに表示します。

VS Code 1.90以降が必要です。通常利用にNode.js・npm・F5は不要です。
更新は新しいVSIXをインストールしてください。exeの設定は引き継ぎます。
詳細は配布アーカイブ内のdoc/vscode.mdを参照してください。ライセンスはMITです。
