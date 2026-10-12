# コマンドライン

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

```text
zfj <file.djs|file.sjs> [-o output] [--unit mm|pt|px] [--fusion] [--force] [--check]
zfj --install
zfj --uninstall
```

| 引数 | 動作 |
|---|---|
| `-o`, `--output` | 出力ファイルを指定 |
| `--inkscape <path>` | PDF変換に使うinkscape.exeのパスを指定 |
| `--unit mm / pt / px` | JSの単位設定を上書き |
| `--fusion` | DXFを輪郭のみで出力。`.djs`専用 |
| `--force` | 既存の出力ファイルを上書き |
| `--check` | スクリプト実行と出力生成の検証のみ。保存しない |
| `--install` | 現在のユーザーに.sjs / .djsの関連付けとアイコンを登録 |
| `--uninstall` | ZFJの関連付けを解除 |
| `-h`, `--help` | ヘルプを表示 |
| `-v`, `--version` | バージョンを表示 |

```powershell
.\x64\Debug\zfj.exe examples\basic.sjs
.\x64\Debug\zfj.exe examples\basic.djs
.\x64\Debug\zfj.exe examples\basic.sjs -o result.svg --force
.\x64\Debug\zfj.exe examples\basic.sjs --check
```

- `.sjs` → SVG、`.djs` → DXF。拡張子は小文字。
- 出力先省略時は入力と同じフォルダ・同じ名前。`.sjs`は`-o result.pdf`でPDFも指定できます。それ以外の出力拡張子の不一致はエラー。
- 既存ファイルは `--force` で上書き。出力先フォルダは事前に作成してください。
- `--check` は実行・検証のみ。`--help` / `--version` 対応。
- 成功は終了コード 0、失敗は 1。エラーは標準エラーへ出力。
- 入力は UTF-8（BOM 可）。Node.js / DOM / import は提供しません。

`--check`もSVG用のClipper2計算・SVG表示合成データの生成まで実行します。
ビューアーや編集ソフトによる実際の表示・読み込みを確認する機能ではありません。
DPI・キャンバスの指定は[JSの設定関数](units-and-canvas.md)で行います。

## PDF出力

```powershell
.\x64\Release\zfj.exe examples\basic.sjs -o result.pdf
.\x64\Release\zfj.exe examples\basic.sjs -o result.pdf --inkscape "C:\Tools\Inkscape\bin\inkscape.exe" --force
```

Inkscapeの別途インストールが必要です。PATH、Program Files、ユーザーのPrograms/Inkscapeから自動検索します。見つからない場合は`--inkscape`で指定してください。
SVGを一時ファイルに保存してInkscapeを非表示で呼び出し、キャンバスを1ページのPDFに変換します。変換の制限時間は20秒です。失敗時は既存の出力を保持し、一時ファイルを削除します。
`--check`はSVG生成までの検証で、Inkscapeの起動・PDF変換は行いません。`.djs`からのPDF出力には対応していません。
VS Codeの`ZFJ: Export`でも`.sjs`の保存形式にPDFを選べます。

## 拡張子の登録・解除

```powershell
C:\Tools\zfj\zfj.exe --install
C:\Tools\zfj\zfj.exe --uninstall
```

どちらも入力ファイルなしで単独指定します。他の実行オプションとの併用はエラーです。
管理者権限は不要で、現在のユーザーの`HKEY_CURRENT_USER\Software\Classes`へ登録します。
登録するProgIDは`zfj.sjs`／`zfj.djs`。専用アイコンと、exe・入力ファイルのパスを引用符で囲んだ起動コマンドを設定します。
ダブルクリックは`zfj.exe "入力ファイル"`相当でJSを実行します。出力先は入力と同じ場所で、既存の出力は上書きしません。
コンソールが閉じてエラーを読めない場合はPowerShellから実行してください。

既存の拡張子の既定ProgIDは変更せず、「プログラムから開く」の候補へ追加します。
ユーザー側の既定ProgIDが未設定ならZFJを設定します。ただしWindowsの`UserChoice`は変更しません。
既定アプリや表示アイコンが変わらない場合は、Windowsの「プログラムから開く」や「既定のアプリ」から選択してください。

再登録すると現在のexeの場所へ更新します。移動・削除前には関連付けを解除するか、移動後に再登録してください。
解除時はZFJのProgID・候補登録と、ZFJを指す拡張子の既定値だけを削除します。
他のアプリの関連付け、Windowsの`UserChoice`、ソース、出力ファイル、VS Code拡張は削除しません。
Windows側でZFJを既定に選択していた場合は、解除後に必要な既定アプリを選び直してください。
登録・解除とも繰り返し実行できます。

VS Code拡張からも`ZFJ: Register File Associations`／`ZFJ: Unregister File Associations`で同じ操作を実行できます。
[VS Codeでの登録・解除手順](vscode.md#登録する)を参照してください。
