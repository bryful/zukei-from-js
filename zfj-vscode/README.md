# ZFJ VS Code拡張

`.sjs` / `.djs`の色分け、drawing / path APIの補完・説明、スニペット、SVG / DXF書き出し、SVGプレビュー、Windowsの拡張子・アイコン登録を提供します。

現在のバージョンは0.0.6です。コマンド名は英語表記です。

## インストール

現在はVSIXからのインストールのみです。Marketplaceでは公開していません。
GitHub Releasesから`zfj.exe`と`.vsix`を含む配布アーカイブをダウンロードして展開します。
VS Code 1.90以降で、Ctrl+Shift+P → `Extensions: Install from VSIX...`から
展開した`.vsix`ファイルを選択してください。
更新時も同じ手順です。旧版のアンインストールは不要で、実行ファイルの設定を引き継ぎます。

VSIXの内部に`zfj.exe`は含まれません。初回の書き出し・プレビュー・拡張子登録で、同じアーカイブから展開した`zfj.exe`を選択してください。
通常利用にNode.js・npm・F5での開発ホスト起動は不要です。

## 使い方

- `.sjs` / `.djs`を開き、`drawing.` / `path.`を入力して補完します。候補が出ないときはCtrl+Spaceを押します。
- `ZFJ: Export`で保存・実行し、SVGまたはDXFを生成します。
- `.sjs`では`ZFJ: SVG Preview`を開くと、保存時に表示を更新します。
- `ZFJ: Register File Associations`でWindowsに`.sjs` / `.djs`と専用アイコンを登録します。
- `ZFJ: Unregister File Associations`でZFJの関連付けを解除します。ファイルや拡張機能は削除しません。
- 実行ファイルを変更する場合は設定の`zfj.executablePath`を指定します。
- 実行ログは「出力」パネルの`ZFJ`で確認できます。

拡張子の登録・削除はファイルを開かずに実行できます。書き出しと共通のexe設定を使い、
未設定なら選択画面を開きます。`--install` / `--uninstall`対応のzfj.exeが必要です。
Windows上の信頼されたワークスペースから実行してください。
現在のWindowsユーザーだけが対象で、管理者権限は不要です。
登録後にZFJで関連付けられたファイルをダブルクリックするとJSを実行し、SVG / DXFを生成します。
他アプリの既定設定は維持します。必要に応じてWindowsの「プログラムから開く」でZFJを選択してください。
exeの場所を変えた場合は設定を更新して再登録し、exeを削除する場合は先に関連付けを解除してください。

[登録・解除の手順](../doc/vscode.md#登録する)も参照してください。

[利用ガイド](../doc/vscode.md)に設定・制限・スニペットをまとめています。
[開発・VSIX作成](../doc/vscode-development.md)、[変更履歴](CHANGELOG.md)、[zfjの使い方](../README.md)も参照してください。

識別子は`zfj-local.zfj-vscode`、ライセンスは[MIT](../LICENSE)です。
