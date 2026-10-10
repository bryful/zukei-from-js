# QuickJS-NGへのローカル変更

基準: v0.10.1 / 3c9afc9943323ee9c7dbd123c0cd991448f4b6c2

- quickjs.c の js_global_funcs: Infinityの定義を `1.0 / 0.0` から
  `JS_PROP_U2D_DEF("Infinity", 0x7FF0ull<<48, 0)` に変更。
  同じ表のNaNと同じ方法でIEEE 754 binary64の正の無限大を定義し、
  Visual Studio IntelliSenseのE0039（0で除算）を避ける。
  JavaScriptの値・プロパティ属性は変更しない。
- 回帰確認: `zfj.exe tests/infinity.sjs --check`。

QuickJSを更新する際は、この差分の必要性を確認してください。
