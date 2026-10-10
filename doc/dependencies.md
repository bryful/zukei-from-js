# 依存ライブラリ・ライセンス

[ドキュメント一覧](README.md) / [利用ガイド](../README.md)

## zfj

本リポジトリとzfj.exeは[MIT License](../LICENSE)です。
同梱ライブラリには以下の各ライセンスが適用されます。

## QuickJS-NG

- バージョン：v0.10.1
- リビジョン：`3c9afc9943323ee9c7dbd123c0cd991448f4b6c2`
- 配置：`third_party/quickjs`
- ライセンス：[MIT](../third_party/quickjs/LICENSE)
- ローカル修正：[QUICKJS-PATCHES.md](../third_party/QUICKJS-PATCHES.md)

JavaScriptの実行に使用します。Infinityの定義など、同梱ソースへの修正は上記の記録を参照してください。

## Clipper2

- リビジョン：`f9c5eb6e14a59f6f5d65fbfb3564519a561cf4fd`
- 配置：`third_party/clipper2`
- ライセンス：[Boost Software License 1.0](../third_party/clipper2/LICENSE)

SVG用の輪郭合成に使用します。`clipper.engine.cpp`をビルド対象としています。
両ライブラリともソースを同梱しており、zfjのビルド時に別途インストールする必要はありません。

## その他

PythonのezdxfはDXF検証用で、zfj実行時には不要です。
VS Code拡張の開発依存は[zfj-vscode/package.json](../zfj-vscode/package.json)を参照してください。
VS Code拡張も本リポジトリの[MIT License](../LICENSE)に従います。
