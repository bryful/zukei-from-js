#pragma once
#include "drawing.h"

namespace zfj
{
/// UTF-8のJSを実行して図面を返す。filenameはエラーの位置表示に使用する。
/// 実行時間・メモリを制限し、JS例外はstd::runtime_errorとして呼び出し元へ伝える。
Drawing execute(const std::string &source, const std::string &filename);
} // namespace zfj
