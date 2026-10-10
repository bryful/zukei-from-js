#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "file_associations.h"
#include <string>
#include <system_error>
#include <vector>
// clang-format off
// Windows本体をShellヘッダーより先に読み込む。
#include <windows.h>
#include <shlobj.h>
// clang-format on

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
/// レジストリキーの所有権を管理する。
struct Key
{
    HKEY value = nullptr;
    ~Key()
    {
        if (value)
            RegCloseKey(value);
    }
    Key() = default;
    Key(const Key &) = delete;
    Key &operator=(const Key &) = delete;
};

/// Win32の失敗を呼出元へ伝える。
void check(LSTATUS status)
{
    if (status != ERROR_SUCCESS)
        throw std::system_error(status, std::system_category(), "File association registry");
}

/// 現在実行中のexeの絶対パスをUnicodeで取得する。
std::wstring executablePath()
{
    std::vector<wchar_t> buffer(32768);
    const auto length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size())
        throw std::system_error(GetLastError(), std::system_category(), "Executable path");
    return {buffer.data(), length};
}

/// HKCU\Software\Classes以下の文字列値を設定する。
void set(const std::wstring &path, const wchar_t *name, const std::wstring &value)
{
    Key key;
    check(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                          &key.value, nullptr));
    check(RegSetValueExW(key.value, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                         static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))));
}

/// 未登録値は空文字列として取得し、他形式の値は既存設定として保護する。
std::wstring get(const std::wstring &path, const wchar_t *name)
{
    DWORD bytes = 0;
    auto status = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_SZ, nullptr,
                               nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
        return {};
    if (status == ERROR_UNSUPPORTED_TYPE)
        return L"<other value type>";
    check(status);
    std::vector<wchar_t> data(bytes / sizeof(wchar_t) + 1);
    check(RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_SZ, nullptr, data.data(),
                       &bytes));
    return data.data();
}

/// 値だけを削除し、他アプリのキー・値は残す。
void removeValue(const std::wstring &path, const wchar_t *name)
{
    const auto status = RegDeleteKeyValueW(HKEY_CURRENT_USER, path.c_str(), name);
    if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
        check(status);
}
} // namespace

namespace zfj
{
void configureFileAssociations(bool install)
{
    const std::wstring base = L"Software\\Classes\\";
    const auto exe = executablePath();
    for (const auto extension : {L"sjs", L"djs"})
    {
        const auto id = std::wstring(L"zfj.") + extension;
        const auto type = base + id;
        const auto ext = base + L"." + extension;
        if (install)
        {
            set(type, nullptr,
                extension == std::wstring(L"sjs") ? L"ZFJ SVG script" : L"ZFJ DXF script");
            set(type, L"ZFJ.Owner", L"zfj");
            set(type + L"\\DefaultIcon", nullptr,
                L"\"" + exe + L"\"," + (extension == std::wstring(L"sjs") ? L"1" : L"2"));
            set(type + L"\\shell\\open", nullptr, L"Generate image with ZFJ");
            set(type + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
            Key key;
            check(RegCreateKeyExW(HKEY_CURRENT_USER, (ext + L"\\OpenWithProgids").c_str(), 0,
                                  nullptr, 0, KEY_SET_VALUE, nullptr, &key.value, nullptr));
            check(RegSetValueExW(key.value, id.c_str(), 0, REG_NONE, nullptr, 0));
            // 既存アプリの既定値を奪わない。WindowsのUserChoiceも変更しない。
            if (get(ext, nullptr).empty())
                set(ext, nullptr, id);
        }
        else
        {
            if (get(ext, nullptr) == id)
                removeValue(ext, nullptr);
            removeValue(ext + L"\\OpenWithProgids", id.c_str());
            if (get(type, L"ZFJ.Owner") == L"zfj")
            {
                const auto status = RegDeleteTreeW(HKEY_CURRENT_USER, type.c_str());
                if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
                    check(status);
            }
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
} // namespace zfj
