# 開発・ビルド・検証

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## ビルド

`zukei-from-js.slnx` を Visual Studio 2026 で開き、x64 の Debug または Release をビルドします。
実行ファイルは `x64/Debug/zfj.exe` または `x64/Release/zfj.exe` です。
QuickJS-NG のソースを third_party/quickjs に同梱しています。追加インストール不要です。

## ビルド出力フォルダ

Visual Studio標準の、ソリューション側に最終成果物、プロジェクト側に中間ファイルを置く構成です。
ソリューション経由と `.vcxproj` 単体のビルドで出力先が変わらないよう、プロジェクトに明示しています。

| 構成 | 最終成果物（exe / pdb） | 中間ファイル（obj等） |
|---|---|---|
| x64 Debug | `x64/Debug/` | `zfj/x64/Debug/` |
| x64 Release | `x64/Release/` | `zfj/x64/Release/` |
| Win32 Debug | `Debug/` | `zfj/Debug/` |
| Win32 Release | `Release/` | `zfj/Release/` |

## ソース構成

| ファイル（zfj/以下） | 役割 |
|---|---|
| `file_associations.h` / `file_associations.cpp` | HKCUの拡張子・アイコン・起動コマンド登録と解除 |
| `zfj.rc` | exe / .sjs / .djsのアイコンリソース |
| `zfj.cpp` | CLI、入力読込、設定の優先順位、保存・終了コード |
| `script.h` / `script.cpp` | QuickJSの生成・実行・制限・例外取得 |
| `drawing_api.h` / `drawing_api.cpp` | JS API、引数検証、パス値の所有・メソッド登録 |
| `drawing.h` / `drawing.cpp` | 図形データ、単位換算、描画の振り分け、SVGキャンバス |
| `dxf.cpp` | DXFのテーブル・エンティティ・スタイル・HATCH |
| `path_offset.cpp` | オフセット、入力・出力輪郭の検証、破綻検出 |
| `path_ops.cpp` | Clipper2演算、合成の解決、DXF用の元図形展開 |
| `svg_composite.cpp` | SVGの複合パスXOR・マスク・表示効果 |
| `path_transform.cpp` | 移動・回転・拡縮、外接矩形、楕円・曲線処理 |
| `path_shapes.cpp` | 正多角形・星型の頂点生成 |

C++20、QuickJS側はC11。整形規則は`.clang-format`です。
図形・パスはコピーして保持し、変換時の検証失敗では元パスを変更しません。

## 実行環境の制限

QuickJSの割当メモリは64MiB、JS実行の時間制限は約10秒です。
C++側の図形データやネイティブ演算全体に対する一律のメモリ・時間上限ではありません。
Node.js、DOM、モジュールimportは提供しません。入力はUTF-8（BOM可）。
頂点・図形数には100000の制限、合成のネストには32段の制限があります。

## 検証

リポジトリのルートで実行します。Pythonテストは通常`x64/Release/zfj.exe`を使用します。
DXF監査用の依存はテスト専用フォルダへ導入します。

```powershell
python -m pip install --target tests/.deps ezdxf
powershell -ExecutionPolicy Bypass -File tests/units.ps1
python tests/styles.py
python tests/paths.py
python tests/transforms.py
python tests/radial_shapes.py
python tests/polyline_move.py
python tests/circle_arcs.py
python tests/offsets.py
python tests/pdf.py
.\x64\Release\zfj.exe tests\infinity.sjs --check
```

| テスト | 対象 |
|---|---|
| `units.ps1` | 単位、DPI、CLIの優先順位 |
| `styles.py` | 色・線幅・塗り、キャンバス、DXF・Fusion |
| `paths.py` | 合成の領域・穴・ネスト、SVG構造、DXF元図形 |
| `transforms.py` | 変換・アンカー・コピー・楕円、DXF監査 |
| `radial_shapes.py` | 正多角形・星型、直接描画、入力検証 |
| `polyline_move.py` | 開いた折れ線、移動・コピー・エラー時の保持 |
| `offsets.py` | 開閉パスの拡張・縮小、cap・join、穴、DXF・Fusion、エラー時の停止 |
| `circle_arcs.py` | 円の互換性、楕円・円弧、方向・変換・入力検証、DXF監査・Fusion |
| `pdf.py` | PDFの形式選択・失敗時の既存出力保持・Unicodeパス。引数にinkscape.exeを渡すと実変換も検証 |
| `infinity.sjs` | QuickJSのInfinity |

[VS Code拡張の開発手順](vscode-development.md)、[依存ライブラリ](dependencies.md)も参照してください。

## PDF変換の検証

PDFの実変換も検証する場合は、Inkscapeの実行ファイルを指定します。

```powershell
python tests/pdf.py "C:\Program Files\Inkscape\bin\inkscape.exe"
```

引数なしの`pdf.py`はInkscape不要で、形式の選択、`--check`、失敗時の既存ファイル保持、Unicodeパスを検証します。

## ファイル関連付けの検証

x64 Native Tools Command Promptでリポジトリのルートから実行します。

```bat
cl /nologo /EHsc /std:c++20 /utf-8 tests\file_associations.cpp /Fo:%TEMP%\zfj-associations-test.obj /Fe:%TEMP%\zfj-associations-test.exe
%TEMP%\zfj-associations-test.exe
```

テストプロセス内でHKCUを専用の一時レジストリキーに差し替えます。
実際のファイル関連付けを変更せず、登録・再登録・解除、アイコン番号、空白・日本語パス、既存設定の保護を検証し、一時キーを削除します。
