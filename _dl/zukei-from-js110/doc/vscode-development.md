# VS Code拡張の開発

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## VSIXを作り直す

`zfj-vscode`フォルダで次を実行します。Node.jsとnpmが必要です。

```powershell
npm install
npm run package
```

コンパイル後、バージョン番号を含むVSIXがこのフォルダに生成されます。
実行時の外部npm依存はありません。開発用ツール・設定フォルダ・バックアップは同梱しません。
更新版を配布する場合はpackage.jsonのversionを上げて作り直し、同じ手順でインストールしてください。
VSIX内の説明文は`README.vsix.md`を使用します。リポジトリ向けの`README.md`とあわせて更新してください。

## 初回の準備

Node.jsのLTS版（npmを含む）とVS Codeをインストールしてください。
この `zfj-vscode` フォルダをVS Codeで開き、ターミナルで実行します。

```powershell
npm install
npm run compile
```

## 動作確認

1. このフォルダを開いたVS CodeでF5を押します。
2. 「ZFJ拡張機能を実行」を選択します（選択が求められた場合）。
3. 開いたExtension Development HostでCtrl+Shift+Pを押します。
4. `ZFJ: Hello World` を実行します。
5. `Hello World! ZFJ拡張機能が動作しています。` と表示されれば成功です。

F5ではTypeScriptを一度コンパイルし、ローカルの開発用ウィンドウを通常起動した後、
デバッガーを接続します。VS CodeのextensionHost launchで起きるクラッシュを避けるための方式です。
再度F5を押すときは、デバッグを停止し、前の開発用ウィンドウを閉じてください。
ソース変更後も同じ手順で再起動できます。
ブレークポイントは `src/extension.ts` に設定できます。

開発用ウィンドウでは `--disable-extensions` を指定し、通常インストールしている
拡張機能との競合を避けています。開発対象のZFJ拡張機能は有効になります。
普段使うVS Codeの拡張機能の設定には影響しません。
開発用ウィンドウのユーザーデータは `.dev-profile` に分離しています。
以前のリモート接続や通常ウィンドウの復元状態を引き継ぎません。
初回は開発用ウィンドウでzfj.exeをもう一度選択してください。

### F5で開発用ホストがクラッシュする場合

VS Codeのデバッガーから起動する経路を切り分けるため、このフォルダの
ターミナルで次を実行してください。

```powershell
node ./node_modules/typescript/bin/tsc -p .
code --new-window --disable-extensions --user-data-dir "C:\Users\bryfu\source\repos\zukei-from-js\zfj-vscode\.dev-profile" --extensionDevelopmentPath="C:\Users\bryfu\source\repos\zukei-from-js\zfj-vscode" --inspect-extensions=9333 --folder-uri "file:///C:/Users/bryfu/source/repos/zukei-from-js/zfj-vscode"
```

開いたウィンドウで `ZFJ: Hello World` を実行できます。
ブレークポイントを使う場合は、元のウィンドウの「実行とデバッグ」で
`ZFJ: 手動起動したホストに接続` を選択してF5を押します。
変更後は再コンパイルし、開発用ウィンドウを再読み込みします。

## ファイルの役割

- `package.json`: 拡張機能の情報、コマンド登録、依存パッケージ、ビルド手順。
- `src/extension.ts`: コマンド登録、exeの選択、書き出し。
- `src/file-associations.ts`: exeの`--install`／`--uninstall`呼び出し、結果通知、二重実行防止。
- `src/preview.ts`: SVGプレビューと保存時の更新。
- `src/language.ts`: API補完・説明。
- `README.vsix.md`: VSIXに収録する利用者向け説明。
- `tsconfig.json`: TypeScriptのコンパイル設定。
- `.vscode/launch.json`: F5で起動するデバッグ設定。
- `.vscode/tasks.json`: デバッグ前のコンパイルとローカル起動の設定。
- `scripts/start-dev-host.cjs`: 開発用ホストを通常起動し、接続準備を待つ処理。
- `out/`: コンパイル後のJavaScript。自動生成されるので編集不要です。

`package.json` の `command` と `registerCommand()` の識別子は一致させます。
VS Code 1.90以降を対象としており、登録したコマンドから自動的に有効になるため、
コマンド用の `activationEvents` の明示は不要です。補完用には言語の起動イベントを登録しています。

## コマンド

```powershell
npm run compile # コンパイル
npm run watch   # 変更時に自動コンパイル
npm run check   # 型チェックのみ
npm test        # 新APIの補完・スニペット、SVG/DXF書き出し、プレビュー生成・拡張子登録コマンドの検証
```

`npm test` は既定でリポジトリの `x64/Release/zfj.exe` を使用します。
別のexeを検証する場合はコンパイル後に `node scripts/test-api.cjs "exeの絶対パス"` を実行してください。
VS Code APIは模擬し、zfj.exeは実際に実行します。VS Codeの画面上での見た目は手動で確認してください。
`scripts/test-file-associations.cjs`は登録・解除コマンドのプロセス起動も模擬し、Windowsの関連付けを変更しません。
入力ファイルを渡さない呼び出し、exe選択、キャンセル、未信頼ワークスペース、失敗後の再実行、二重実行防止を検証します。

更新内容は[CHANGELOG](../zfj-vscode/CHANGELOG.md)を参照してください。
