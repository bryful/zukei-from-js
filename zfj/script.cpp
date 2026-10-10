#include "script.h"
#include "drawing_api.h"
#include <chrono>
#include <stdexcept>
namespace zfj
{
/// QuickJSの生成・実行・例外取得・解放を行う。描画処理はdrawing APIに委譲する。
Drawing execute(const std::string &source, const std::string &filename, bool svg,
                std::optional<std::string> unitOverride)
{
    auto runtime = JS_NewRuntime();
    if (!runtime)
        throw std::runtime_error("Runtime allocation failed");
    JS_SetMemoryLimit(runtime, 64 * 1024 * 1024);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    JS_SetInterruptHandler(
        runtime,
        [](JSRuntime *, void *p) -> int
        {
            return std::chrono::steady_clock::now() >
                   *static_cast<std::chrono::steady_clock::time_point *>(p);
        },
        &deadline);
    auto context = JS_NewContext(runtime);
    if (!context)
    {
        JS_FreeRuntime(runtime);
        throw std::runtime_error("Context allocation failed");
    }
    Drawing drawing;
    drawing.svgCoordinates = svg;
    drawing.unitOverride = std::move(unitOverride);
    registerDrawingApi(context, drawing);
    auto result =
        JS_Eval(context, source.data(), source.size(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    std::string error;
    if (JS_IsException(result))
    {
        auto exception = JS_GetException(context);
        const char *message = JS_ToCString(context, exception);
        error = message ? message : "JavaScript error";
        JS_FreeCString(context, message);
        auto stack = JS_GetPropertyStr(context, exception, "stack");
        if (!JS_IsUndefined(stack))
        {
            const char *trace = JS_ToCString(context, stack);
            if (trace)
                error += std::string("\n") + trace;
            JS_FreeCString(context, trace);
        }
        JS_FreeValue(context, stack);
        JS_FreeValue(context, exception);
    }
    JS_FreeValue(context, result);
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    if (!error.empty())
        throw std::runtime_error(error);
    return drawing;
}

} // namespace zfj
