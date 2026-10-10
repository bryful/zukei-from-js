#include "file_associations.h"
#include "script.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace
{

/// コマンド引数の解析結果。unit未指定時はJS側の設定を使用する。
struct Options
{
    fs::path input;
    fs::path output;
    std::optional<std::string> unit;
    bool force = false;
    bool check = false;
    bool help = false;
    bool version = false;
    bool fusion = false;
    bool install = false;
    bool uninstall = false;
};

/// CLIの基本構文と利用可能なオプションを標準出力へ表示する。
void printHelp()
{
    std::cout << "Usage: zfj <file.djs|file.sjs> [-o output] [--unit mm|pt|px] [--fusion] "
                 "[--force] [--check]\n"
                 "       zfj --install | --uninstall\n"
                 "  --install       Register .sjs/.djs for the current user\n"
                 "  --uninstall     Remove ZFJ file associations for the current user\n"
                 "  -o, --output    Output file\n"
                 "  --unit          Override the unit configured in JavaScript\n"
                 "  --fusion        DXF only: export sketch outlines without fill or styling\n"
                 "  --force         Overwrite an existing output file\n"
                 "  --check         Execute and validate without writing output\n"
                 "  -h, --help      Show help\n"
                 "  -v, --version   Show version\n";
}

/// Unicodeのコマンド引数を解析する。パスは変換せずfilesystem::pathに保持する。
Options parseOptions(int argc, wchar_t **argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help")
        {
            options.help = true;
            return options;
        }
        if (arg == L"-v" || arg == L"--version")
        {
            options.version = true;
            return options;
        }
        if (arg == L"--install")
        {
            options.install = true;
        }
        else if (arg == L"--uninstall")
        {
            options.uninstall = true;
        }
        else if (arg == L"--force")
        {
            options.force = true;
        }
        else if (arg == L"--fusion")
        {
            options.fusion = true;
        }
        else if (arg == L"--check")
        {
            options.check = true;
        }
        else if (arg == L"--unit")
        {
            if (++i >= argc)
                throw std::runtime_error("--unit requires mm, pt or px");
            const std::wstring unit = argv[i];
            if (unit == L"mm")
                options.unit = "mm";
            else if (unit == L"pt")
                options.unit = "pt";
            else if (unit == L"px")
                options.unit = "px";
            else
                throw std::runtime_error("Unit must be mm, pt or px");
        }
        else if (arg == L"-o" || arg == L"--output")
        {
            if (++i >= argc || !options.output.empty())
                throw std::runtime_error("Specify one output path");
            options.output = argv[i];
        }
        else if (!arg.empty() && arg[0] == L'-')
        {
            throw std::runtime_error("Unknown option");
        }
        else
        {
            if (!options.input.empty())
                throw std::runtime_error("Specify one input file");
            options.input = arg;
        }
    }
    if (options.install || options.uninstall)
    {
        if (argc != 2)
            throw std::runtime_error("Use --install or --uninstall alone");
        return options;
    }
    if (options.input.empty())
        throw std::runtime_error("Usage: zfj <file.djs|file.sjs> [-o output] [--unit mm|pt|px] "
                                 "[--fusion] [--force] [--check]");
    return options;
}

/// UTF-8の入力ソースを読み込む。先頭のBOMはJSへ渡す前に取り除く。
std::string readSource(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open input");
    std::string source((std::istreambuf_iterator<char>(input)), {});
    if (source.compare(0, 3, "\xef\xbb\xbf") == 0)
        source.erase(0, 3);
    return source;
}

/// 生成済みの文書を保存し、オープン・書き込み・flushの失敗を報告する。
void writeOutput(const fs::path &path, const std::string &data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output || !(output << data) || !output.flush())
        throw std::runtime_error("Cannot write output");
}

} // namespace

/// Windowsのエントリーポイント。引数解析、JS実行、形式選択、保存、終了コードを管理する。
int wmain(int argc, wchar_t **argv)
{
    try
    {
        auto options = parseOptions(argc, argv);
        if (options.help)
        {
            printHelp();
            return 0;
        }
        if (options.version)
        {
            std::cout << "zfj 0.1.0\n";
            return 0;
        }

        if (options.install || options.uninstall)
        {
            zfj::configureFileAssociations(options.install);
            std::cout << (options.install
                              ? "Registered .sjs/.djs for the current user.\n"
                              : "Removed ZFJ file associations for the current user.\n");
            return 0;
        }

        const bool svg = options.input.extension() == L".sjs";
        if (!svg && options.input.extension() != L".djs")
            throw std::runtime_error("Input must be .djs or .sjs");
        if (svg && options.fusion)
            throw std::runtime_error("--fusion is only valid for .djs input");
        const auto extension = svg ? L".svg" : L".dxf";
        if (options.output.empty())
        {
            options.output = options.input;
            options.output.replace_extension(extension);
        }
        if (options.output.extension() != extension)
            throw std::runtime_error("Output extension does not match input");
        if (!options.check && !options.force && fs::exists(options.output))
            throw std::runtime_error("Output exists; use --force");

        const auto source = readSource(options.input);
        const auto utf8Path = options.input.u8string();
        const std::string filename(utf8Path.begin(), utf8Path.end());
        const auto drawing = zfj::execute(source, filename);
        // --checkでもSVG合成まで検証する。明示したCLI単位はJSより優先する。
        const auto unit = options.unit.value_or(drawing.unit);
        const auto document = zfj::render(drawing, svg, unit, options.fusion);
        if (!options.check)
            writeOutput(options.output, document);
        std::cout << (options.check ? "Checked " : "Generated ") << drawing.shapes.size()
                  << " shapes.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "zfj: " << error.what() << '\n';
        return 1;
    }
}
