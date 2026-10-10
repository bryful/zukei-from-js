# Windowsアイコン

| ファイル | 用途 | exe内のアイコン番号 |
|---|---|---|
| `zfj.ico` | zfj.exe | 0 |
| `sjs.ico` | .sjs（SVGソース） | 1 |
| `djs.ico` | .djs（DXFソース） | 2 |

ICOは透過背景で16 / 20 / 24 / 32 / 40 / 48 / 64 / 128 / 256pxを収録しています。
編集用SVG、256pxのPNG、一覧画像preview.pngも用意しています。MIT Licenseです。

zfj.rcで3種類をexeに埋め込みます。先頭はアプリのアイコンです。
`zfj.exe --install`で現在のユーザーにファイル関連付けとアイコンを登録し、
`zfj.exe --uninstall`で解除します。管理者権限は不要です。
DefaultIconには実行ファイルの絶対パスと番号
（例：`"C:\Tools\zfj\zfj.exe",1`）を使用できます。
SVG版の文字はArial系フォントを使用しています。
