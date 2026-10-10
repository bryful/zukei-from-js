// x64 Native Tools Command Promptでコンパイルする、HKCUを分離した関連付けテスト。
#include "../zfj/file_associations.cpp"
#include <iostream>

/// assert無効化構成でも検証を実行する。
void require(bool condition)
{
    if (!condition)
        throw std::runtime_error("Association test failed");
}

int main()
{
    const auto path = L"Software\\ZFJ.AssociationTests\\" + std::to_wstring(GetCurrentProcessId());
    HKEY sandbox = nullptr;
    try
    {
        check(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS,
                              nullptr, &sandbox, nullptr));
        check(RegOverridePredefKey(HKEY_CURRENT_USER, sandbox));
        const std::wstring base = L"Software\\Classes\\";
        // 他アプリの既定値と候補がある状態から登録する。
        set(base + L".djs", nullptr, L"Other.Editor");
        set(base + L".djs\\OpenWithProgids", L"Other.Editor", L"");
        zfj::configureFileAssociations(true);
        zfj::configureFileAssociations(true);
        require(get(base + L".sjs", nullptr) == L"zfj.sjs");
        require(get(base + L".djs", nullptr) == L"Other.Editor");
        const auto exe = executablePath();
        require(get(base + L"zfj.sjs\\DefaultIcon", nullptr) == L"\"" + exe + L"\",1");
        require(get(base + L"zfj.djs\\DefaultIcon", nullptr) == L"\"" + exe + L"\",2");
        require(get(base + L"zfj.sjs\\shell\\open\\command", nullptr) ==
                L"\"" + exe + L"\" \"%1\"");
        DWORD size = 0;
        check(RegGetValueW(HKEY_CURRENT_USER, (base + L".djs\\OpenWithProgids").c_str(), L"zfj.djs",
                           RRF_RT_REG_NONE, nullptr, nullptr, &size));
        zfj::configureFileAssociations(false);
        zfj::configureFileAssociations(false);
        require(get(base + L".sjs", nullptr).empty());
        require(get(base + L".djs", nullptr) == L"Other.Editor");
        require(get(base + L"zfj.sjs", L"ZFJ.Owner").empty());
        require(get(base + L"zfj.djs", L"ZFJ.Owner").empty());
        check(RegGetValueW(HKEY_CURRENT_USER, (base + L".djs\\OpenWithProgids").c_str(),
                           L"Other.Editor", RRF_RT_REG_SZ, nullptr, nullptr, &size));
        require(RegGetValueW(HKEY_CURRENT_USER, (base + L".djs\\OpenWithProgids").c_str(),
                             L"zfj.djs", RRF_RT_ANY, nullptr, nullptr,
                             &size) == ERROR_FILE_NOT_FOUND);
        check(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr));
        RegCloseKey(sandbox);
        sandbox = nullptr;
        check(RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()));
        std::cout
            << "File associations: install, reinstall, uninstall, icons and preservation OK.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        if (sandbox)
            RegCloseKey(sandbox);
        RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
        std::cerr << e.what() << '\n';
        return 1;
    }
}
