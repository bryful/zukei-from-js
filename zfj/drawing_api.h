#pragma once
#include "drawing.h"
#include "quickjs.h"

namespace zfj
{
/// drawing・pathオブジェクトをJSのグローバルへ登録する。図面はコンテキストのopaqueに保持する。
/// drawingの寿命はJSコンテキスト以上でなければならない。
void registerDrawingApi(JSContext *context, Drawing &drawing);
} // namespace zfj
