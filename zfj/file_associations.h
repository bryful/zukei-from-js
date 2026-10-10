#pragma once

namespace zfj
{
/// 現在のユーザーの.sjs/.djs関連付けを登録・解除し、Explorerへ変更を通知する。
void configureFileAssociations(bool install);
} // namespace zfj
