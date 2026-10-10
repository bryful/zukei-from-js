#include "drawing_api.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace zfj
{
namespace
{
/// このJSコンテキスト専用のパスクラスIDを取得する。可変グローバル状態は使わない。
JSClassID pathClassId(JSContext *context)
{
    return static_cast<Drawing *>(JS_GetContextOpaque(context))->pathClassId;
}
/// JSオブジェクトの寿命が終わった時に、そのオブジェクトが所有する図形を解放する。
void finalizePath(JSRuntime *, JSValue value)
{
    delete static_cast<Shape *>(JS_GetOpaque(value, JS_GetClassID(value)));
}

/// 図形を不透明なJSオブジェクトとして返す。図面へはまだ追加しない。
JSValue createPath(JSContext *context, Shape shape)
{
    JSValue object = JS_NewObjectClass(context, pathClassId(context));
    if (JS_IsException(object))
        return object;
    try
    {
        JS_SetOpaque(object, new Shape(std::move(shape)));
    }
    catch (...)
    {
        JS_FreeValue(context, object);
        throw;
    }
    return object;
}

/// path APIが作成したオブジェクトだけを受け付ける。任意のJSオブジェクトは拒否する。
const Shape &readPath(JSContext *context, JSValueConst value)
{
    const auto *shape = static_cast<const Shape *>(JS_GetOpaque(value, pathClassId(context)));
    if (!shape)
        throw std::runtime_error("Expected a path created by the path API");
    return *shape;
}

/// 値として保持する合成パスの深さと図形数を検証し、過大なコピーを防ぐ。
size_t pathSize(const Shape &shape, unsigned depth = 0)
{
    if (depth > 32)
        throw std::runtime_error("Path nesting limit is 32");
    size_t count = 1;
    for (const auto &child : shape.children)
    {
        count += pathSize(child, depth + 1);
        if (count > 100000)
            throw std::runtime_error("Path contains too many shapes");
    }
    return count;
}

/// JSプロパティの参照を保持し、例外が発生してもQuickJSの値を解放する。
class Property
{
  public:
    /// 指定プロパティを取得する。getterのJS例外は呼び出し元へそのまま返す。
    Property(JSContext *context, JSValueConst object, const char *name)
        : context_(context), value_(JS_GetPropertyStr(context, object, name))
    {
    }
    /// 所有する参照を解放する。
    ~Property()
    {
        JS_FreeValue(context_, value_);
    }
    Property(const Property &) = delete;
    Property &operator=(const Property &) = delete;
    /// 値を借用する。所有権は移動しない。
    JSValueConst get() const
    {
        return value_;
    }

  private:
    JSContext *context_;
    JSValue value_;
};

/// #RRGGBBまたはnull/"none"をRGB値へ変換する。任意のSVG/CSS文字列は受け付けない。
std::optional<uint32_t> readColor(JSContext *context, JSValueConst value)
{
    if (JS_IsNull(value))
        return std::nullopt;
    if (!JS_IsString(value))
        throw std::runtime_error("Color must be #RRGGBB, none or null");
    const char *chars = JS_ToCString(context, value);
    if (!chars)
        throw std::runtime_error("Cannot read color");
    const std::string color(chars);
    JS_FreeCString(context, chars);
    if (color == "none")
        return std::nullopt;
    if (color.size() != 7 || color[0] != '#')
        throw std::runtime_error("Color must be #RRGGBB, none or null");
    uint32_t rgb = 0;
    for (size_t i = 1; i < color.size(); ++i)
    {
        const char ch = color[i];
        int digit = ch >= '0' && ch <= '9'   ? ch - '0'
                    : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                    : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                             : -1;
        if (digit < 0)
            throw std::runtime_error("Invalid hexadecimal color");
        rgb = (rgb << 4) | digit;
    }
    return rgb;
}

/// 省略可能なスタイルを検証する。JSのgetter例外はfalseを返して保持する。
bool readStyle(JSContext *context, JSValueConst object, Shape &shape)
{
    if (JS_IsUndefined(object))
        return true;
    if (!JS_IsObject(object) || JS_IsArray(object) || JS_IsFunction(context, object))
        throw std::runtime_error("Style must be an object");
    Property stroke(context, object, "stroke");
    if (JS_IsException(stroke.get()))
        return false;
    if (!JS_IsUndefined(stroke.get()))
        shape.style.stroke = readColor(context, stroke.get());
    Property fill(context, object, "fill");
    if (JS_IsException(fill.get()))
        return false;
    if (!JS_IsUndefined(fill.get()))
        shape.style.fill = readColor(context, fill.get());
    if ((shape.kind == ShapeKind::Line || shape.kind == ShapeKind::Polyline || shape.arc) &&
        shape.style.fill)
        throw std::runtime_error("Open line/polyline/arc does not support fill");
    Property width(context, object, "strokeWidth");
    if (JS_IsException(width.get()))
        return false;
    if (!JS_IsUndefined(width.get()))
    {
        double number = 0;
        if (!JS_IsNumber(width.get()) || JS_ToFloat64(context, &number, width.get()) < 0 ||
            !std::isfinite(number) || number <= 0 || number > 1e9)
            throw std::runtime_error("strokeWidth must be a finite positive number within 1e9");
        shape.style.strokeWidth = number;
    }
    return true;
}

/// JSから図面全体の単位を設定する。CLIによる上書きは実行後に解決する。
JSValue setUnit(JSContext *context, JSValueConst, int argumentCount, JSValueConst *arguments)
{
    if (argumentCount != 1 || !JS_IsString(arguments[0]))
        return JS_ThrowTypeError(context, "setUnit expects one string: mm, pt or px");
    const char *text = JS_ToCString(context, arguments[0]);
    if (!text)
        return JS_EXCEPTION;
    std::string unit(text);
    JS_FreeCString(context, text);
    if (unit != "mm" && unit != "pt" && unit != "px")
        return JS_ThrowTypeError(context, "Unit must be mm, pt or px");
    static_cast<Drawing *>(JS_GetContextOpaque(context))->unit = unit;
    return JS_UNDEFINED;
}
/// pxを実寸へ換算するDPIを設定する。mmとptには影響しない。
JSValue setDpi(JSContext *context, JSValueConst, int argumentCount, JSValueConst *arguments)
{
    double dpi = 0;
    if (argumentCount != 1 || !JS_IsNumber(arguments[0]) ||
        JS_ToFloat64(context, &dpi, arguments[0]) < 0 || !std::isfinite(dpi) || dpi < 1 ||
        dpi > 1000000)
        return JS_ThrowTypeError(context, "setDpi expects a finite number from 1 to 1000000");
    static_cast<Drawing *>(JS_GetContextOpaque(context))->dpi = dpi;
    return JS_UNDEFINED;
}
/// JSの数値を有限な座標値として検証し、変換する。
double readCoordinate(JSContext *context, JSValueConst value)
{
    double coordinate;
    if (!JS_IsNumber(value) || JS_ToFloat64(context, &coordinate, value) < 0 ||
        !std::isfinite(coordinate) || std::abs(coordinate) > 1e9)
        throw std::runtime_error("Expected finite number within +/-1e9");
    return coordinate;
}
/// pos/scale/anchorを{x,y}または[x,y]から読む。省略成分は既定値を使う。
Point readPair(JSContext *context, JSValueConst value, Point defaults, bool requireBoth = false)
{
    if (JS_IsUndefined(value))
        return defaults;
    if (!JS_IsObject(value) || JS_IsNull(value) || JS_IsFunction(context, value))
        throw std::runtime_error("pos, scale and coordinate anchor must be {x,y} or [x,y]");
    const bool array = JS_IsArray(value);
    if (array)
    {
        Property length(context, value, "length");
        if (readCoordinate(context, length.get()) != 2)
            throw std::runtime_error("Coordinate arrays require exactly two elements");
    }
    Property x(context, value, array ? "0" : "x"), y(context, value, array ? "1" : "y");
    if (JS_IsException(x.get()) || JS_IsException(y.get()))
        throw std::runtime_error("Cannot read transform coordinates");
    if ((requireBoth || array) && (JS_IsUndefined(x.get()) || JS_IsUndefined(y.get())))
        throw std::runtime_error("Coordinate anchor requires both x and y");
    return {JS_IsUndefined(x.get()) ? defaults.x : readCoordinate(context, x.get()),
            JS_IsUndefined(y.get()) ? defaults.y : readCoordinate(context, y.get())};
}

/// 名前付きアンカーを変換前の外接矩形上の座標へ変換する。
Point readAnchor(JSContext *context, JSValueConst value, const Shape &shape)
{
    const auto bounds = pathBounds(shape);
    if (!bounds.valid)
        throw std::runtime_error("Cannot determine anchor of an empty path");
    const Point center{(bounds.minX + bounds.maxX) / 2, (bounds.minY + bounds.maxY) / 2};
    if (JS_IsUndefined(value))
        return center;
    if (!JS_IsString(value))
        return readPair(context, value, center, true);
    const char *chars = JS_ToCString(context, value);
    if (!chars)
        throw std::runtime_error("Cannot read anchor");
    const std::string name(chars);
    JS_FreeCString(context, chars);
    if (name == "canvasCenter")
    {
        const auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
        if (!drawing.canvas)
            throw std::runtime_error(
                "canvasCenter requires drawing.setCanvas(width, height) first");
        return {drawing.canvas->width / 2, drawing.canvas->height / 2};
    }
    const std::array<const char *, 9> canonical = {"topLeft",    "top",          "topRight",
                                                   "centerLeft", "center",       "centerRight",
                                                   "bottomLeft", "bottomCenter", "bottomRight"};
    const std::array<const char *, 9> english = {"top-left",    "top",    "top-right",
                                                 "left",        "center", "right",
                                                 "bottom-left", "bottom", "bottom-right"};
    const std::array<const char *, 9> japanese = {"左上", "上",   "右上", "左中", "中心",
                                                  "右中", "左下", "下",   "右下"};
    for (size_t i = 0; i < english.size(); ++i)
        if (name == canonical[i] || name == english[i] || name == japanese[i] ||
            (i == 1 && name == "topCenter") || (i == 3 && name == "middle-left") ||
            (i == 5 && name == "middle-right") || (i == 1 && name == "top-center") ||
            (i == 7 && name == "bottom-center"))
            return {bounds.minX + (bounds.maxX - bounds.minX) * static_cast<double>(i % 3) / 2,
                    bounds.minY + (bounds.maxY - bounds.minY) * static_cast<double>(i / 3) / 2};
    throw std::runtime_error("Unknown anchor name");
}

/// パスを変更、または変更後のコピーを返す。エラー時は元パスを変更しない。
JSValue transformValue(JSContext *context, JSValueConst, int count, JSValueConst *args, int copy)
{
    try
    {
        if (count != 1 && count != 2)
            throw std::runtime_error(
                "transform/transformed expects a path and optional parameters");
        const Shape &source = readPath(context, args[0]);
        Shape result = source;
        if (count == 2 && !JS_IsUndefined(args[1]))
        {
            if (!JS_IsObject(args[1]) || JS_IsNull(args[1]) || JS_IsArray(args[1]) ||
                JS_IsFunction(context, args[1]))
                throw std::runtime_error("Transform parameters must be an object");
            Property pos(context, args[1], "pos"), rot(context, args[1], "rot"),
                scale(context, args[1], "scale"), anchor(context, args[1], "anchor");
            if (JS_IsException(pos.get()) || JS_IsException(rot.get()) ||
                JS_IsException(scale.get()) || JS_IsException(anchor.get()))
                return JS_EXCEPTION;
            const Point translation = readPair(context, pos.get(), {0, 0});
            const Point percentages = readPair(context, scale.get(), {100, 100});
            if (percentages.x == 0 || percentages.y == 0)
                throw std::runtime_error("Scale percentages must be nonzero");
            const double degrees =
                JS_IsUndefined(rot.get()) ? 0 : readCoordinate(context, rot.get());
            const Point pivot = readAnchor(context, anchor.get(), source);
            if (translation.x != 0 || translation.y != 0 || degrees != 0 || percentages.x != 100 ||
                percentages.y != 100)
                transformPath(result, translation, degrees,
                              {percentages.x / 100, percentages.y / 100}, pivot);
        }
        if (copy)
            return createPath(context, std::move(result));
        auto *target = static_cast<Shape *>(JS_GetOpaque(args[0], pathClassId(context)));
        *target = std::move(result);
        return JS_DupValue(context, args[0]);
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// 相対移動を変更・コピーで実行する。mode=0/1は共通API、2/3はパス値のメソッド。
JSValue moveValue(JSContext *context, JSValueConst self, int count, JSValueConst *args, int mode)
{
    try
    {
        const bool method = mode >= 2;
        const bool copy = (mode % 2) != 0;
        if (count != (method ? 2 : 3))
            throw std::runtime_error(
                "move/moved expects (x,y) on a path or (path,x,y) on the path API");
        const JSValueConst target = method ? self : args[0];
        Shape result = readPath(context, target);
        const int first = method ? 0 : 1;
        movePath(result,
                 {readCoordinate(context, args[first]), readCoordinate(context, args[first + 1])});
        if (copy)
            return createPath(context, std::move(result));
        *static_cast<Shape *>(JS_GetOpaque(target, pathClassId(context))) = std::move(result);
        return JS_DupValue(context, target);
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// 省略可能な数値オプションを読む。長さ・角度は既存の有限数制限に従う。
double optionNumber(JSContext *context, JSValueConst object, const char *key, double fallback)
{
    if (JS_IsUndefined(object))
        return fallback;
    Property property(context, object, key);
    if (JS_IsException(property.get()))
        throw std::runtime_error("Cannot read shape parameter");
    return JS_IsUndefined(property.get()) ? fallback : readCoordinate(context, property.get());
}

/// 正多角形・星型を共通生成する。modeの0/1はパス値、2/3は直接描画。
JSValue createRadialPath(JSContext *context, JSValueConst, int count, JSValueConst *args, int mode)
{
    try
    {
        const bool star = (mode % 2) != 0;
        if (count > 2)
            throw std::runtime_error("regularPolygon/star expects optional parameters and style");
        const JSValueConst options = count == 0 ? JS_UNDEFINED : args[0];
        if (!JS_IsUndefined(options) && (!JS_IsObject(options) || JS_IsNull(options) ||
                                         JS_IsArray(options) || JS_IsFunction(context, options)))
            throw std::runtime_error("Shape parameters must be an object");
        Point center{0, 0};
        if (!JS_IsUndefined(options))
        {
            Property value(context, options, "center");
            if (JS_IsException(value.get()))
                return JS_EXCEPTION;
            center = readPair(context, value.get(), {0, 0});
        }
        const double rot = optionNumber(context, options, "rot", 0);
        const double vertices =
            optionNumber(context, options, star ? "points" : "sides", star ? 5 : 3);
        const double maximum = star ? 50000 : 100000;
        if (vertices < 3 || vertices > maximum || std::floor(vertices) != vertices)
            throw std::runtime_error(star ? "points must be an integer from 3 to 50000"
                                          : "sides must be an integer from 3 to 100000");
        Shape shape{ShapeKind::Polygon, {}};
        if (star)
            shape = makeStar(center, optionNumber(context, options, "outerRadius", 30),
                             optionNumber(context, options, "innerRadius", 15),
                             static_cast<unsigned>(vertices), rot);
        else
        {
            bool edgeRadius = false;
            if (!JS_IsUndefined(options))
            {
                Property mode(context, options, "radiusMode");
                if (JS_IsException(mode.get()))
                    return JS_EXCEPTION;
                if (!JS_IsUndefined(mode.get()))
                {
                    if (!JS_IsString(mode.get()))
                        throw std::runtime_error("radiusMode must be vertex or edge");
                    const char *chars = JS_ToCString(context, mode.get());
                    if (!chars)
                        return JS_EXCEPTION;
                    const std::string name(chars);
                    JS_FreeCString(context, chars);
                    if (name != "vertex" && name != "edge")
                        throw std::runtime_error("radiusMode must be vertex or edge");
                    edgeRadius = name == "edge";
                }
            }
            shape = makeRegularPolygon(center, optionNumber(context, options, "radius", 30),
                                       static_cast<unsigned>(vertices), edgeRadius, rot);
        }
        if (count == 2 && !readStyle(context, args[1], shape))
            return JS_EXCEPTION;
        if (mode >= 2)
        {
            auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
            if (drawing.shapes.size() >= 100000)
                throw std::runtime_error("Too many shapes");
            drawing.shapes.push_back(std::move(shape));
            return JS_UNDEFINED;
        }
        return createPath(context, std::move(shape));
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// SVGの固定キャンバスを設定する。単位は図面全体の最終設定を使用する。
JSValue setCanvas(JSContext *context, JSValueConst, int count, JSValueConst *args)
{
    try
    {
        if (count != 2)
            throw std::runtime_error("setCanvas expects width and height");
        const double width = readCoordinate(context, args[0]);
        const double height = readCoordinate(context, args[1]);
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Canvas dimensions must be positive");
        static_cast<Drawing *>(JS_GetContextOpaque(context))->canvas = Canvas{width, height};
        return JS_UNDEFINED;
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// DXFの出力用途を選ぶ。CLIの--fusionがある場合はその指定が優先される。
JSValue setDxfMode(JSContext *context, JSValueConst, int count, JSValueConst *args)
{
    if (count != 1 || !JS_IsString(args[0]))
        return JS_ThrowTypeError(context, "setDxfMode expects standard or fusion");
    const char *chars = JS_ToCString(context, args[0]);
    if (!chars)
        return JS_EXCEPTION;
    const std::string mode(chars);
    JS_FreeCString(context, chars);
    if (mode != "standard" && mode != "fusion")
        return JS_ThrowTypeError(context, "setDxfMode expects standard or fusion");
    static_cast<Drawing *>(JS_GetContextOpaque(context))->fusion = mode == "fusion";
    return JS_UNDEFINED;
}

/// combineコールバック内の図形を1つの論理演算グループとして記録する。
/// 演算自体はSVG出力時に行い、DXF用には元図形と元スタイルを保持する。
JSValue combine(JSContext *context, JSValueConst, int count, JSValueConst *args)
{
    auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
    const size_t start = drawing.shapes.size();
    bool entered = false;
    try
    {
        if ((count != 2 && count != 3) || !JS_IsString(args[0]) || !JS_IsFunction(context, args[1]))
            throw std::runtime_error(
                "combine expects and/or/xor, a synchronous callback and optional style");
        const char *chars = JS_ToCString(context, args[0]);
        if (!chars)
            return JS_EXCEPTION;
        const std::string operation(chars);
        JS_FreeCString(context, chars);
        if (operation != "and" && operation != "or" && operation != "xor")
            throw std::runtime_error("combine operation must be and, or or xor");
        Shape group{ShapeKind::BooleanGroup, {}};
        group.operation = operation;
        if (count == 3 && !readStyle(context, args[2], group))
            return JS_EXCEPTION;
        if (drawing.combineDepth >= 32)
            throw std::runtime_error("combine nesting limit is 32");
        ++drawing.combineDepth;
        entered = true;
        JSValue result = JS_Call(context, args[1], JS_UNDEFINED, 0, nullptr);
        --drawing.combineDepth;
        entered = false;
        if (JS_IsException(result))
        {
            drawing.shapes.resize(start);
            return JS_EXCEPTION;
        }
        const bool async = JS_IsPromise(result);
        JS_FreeValue(context, result);
        if (async)
            throw std::runtime_error("combine callback must be synchronous");
        if (drawing.shapes.size() - start < 2)
            throw std::runtime_error("combine requires at least two shapes");
        for (size_t i = start; i < drawing.shapes.size(); ++i)
        {
            if (drawing.shapes[i].kind == ShapeKind::Line ||
                drawing.shapes[i].kind == ShapeKind::Polyline || drawing.shapes[i].arc)
                throw std::runtime_error(
                    "combine requires closed shapes; open line/polyline/arc is not supported");
            group.children.push_back(std::move(drawing.shapes[i]));
        }
        drawing.shapes.resize(start);
        drawing.shapes.push_back(std::move(group));
        return JS_UNDEFINED;
    }
    catch (const std::exception &error)
    {
        if (entered)
            --drawing.combineDepth;
        drawing.shapes.resize(start);
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// 既存のパスを図面へコピーする。任意のスタイル上書きは元のパスを変更しない。
JSValue drawPath(JSContext *context, JSValueConst, int count, JSValueConst *args)
{
    try
    {
        if (count != 1 && count != 2)
            throw std::runtime_error("draw expects a path and optional style");
        Shape shape = readPath(context, args[0]);
        if (count == 2 && !readStyle(context, args[1], shape))
            return JS_EXCEPTION;
        auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
        if (drawing.shapes.size() >= 100000)
            throw std::runtime_error("Too many shapes");
        drawing.shapes.push_back(std::move(shape));
        return JS_UNDEFINED;
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// AND/OR/XORを指定する値を作る。svgOnlyはSVG表示合成、それ以外はClipper2輪郭計算。
JSValue combinePaths(JSContext *context, JSValueConst, int count, JSValueConst *args, int svgOnly)
{
    try
    {
        if (count < 3 || !JS_IsString(args[0]))
            throw std::runtime_error(
                "combine/svgcombine expects and/or/xor and at least two paths");
        const char *chars = JS_ToCString(context, args[0]);
        if (!chars)
            return JS_EXCEPTION;
        const std::string operation(chars);
        JS_FreeCString(context, chars);
        if (operation != "and" && operation != "or" && operation != "xor")
            throw std::runtime_error("combine/svgcombine operation must be and, or or xor");
        Shape group{svgOnly ? ShapeKind::SvgComposite : ShapeKind::BooleanGroup, {}};
        group.operation = operation;
        size_t total = 1;
        for (int i = 1; i < count; ++i)
        {
            const auto &operand = readPath(context, args[i]);
            if (operand.kind == ShapeKind::Line || operand.kind == ShapeKind::Polyline ||
                operand.arc)
                throw std::runtime_error("combine/svgcombine requires closed paths; open "
                                         "line/polyline/arc is not supported");
            total += pathSize(operand, 1);
            if (total > 100000)
                throw std::runtime_error("Path contains too many shapes");
            group.children.push_back(operand);
        }
        return createPath(context, std::move(group));
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// オフセット距離と端点・結合を検証して独立したパスを返す。
JSValue offsetValue(JSContext *context, JSValueConst, int count, JSValueConst *args)
{
    try
    {
        if (count != 2 && count != 3)
            throw std::runtime_error("offset expects a path, distance and optional options");
        const auto &source = readPath(context, args[0]);
        const double distance = readCoordinate(context, args[1]);
        std::string cap = "butt", join = "miter";
        if (count == 3 && !JS_IsUndefined(args[2]))
        {
            if (!JS_IsObject(args[2]) || JS_IsNull(args[2]) || JS_IsArray(args[2]) ||
                JS_IsFunction(context, args[2]))
                throw std::runtime_error("offset options must be an object");
            for (const auto name : {"cap", "join"})
            {
                Property value(context, args[2], name);
                if (JS_IsException(value.get()))
                    return JS_EXCEPTION;
                if (JS_IsUndefined(value.get()))
                    continue;
                if (!JS_IsString(value.get()))
                    throw std::runtime_error("offset cap/join must be a string");
                const char *text = JS_ToCString(context, value.get());
                if (!text)
                    return JS_EXCEPTION;
                (std::string(name) == "cap" ? cap : join) = text;
                JS_FreeCString(context, text);
            }
        }
        const auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
        const auto unit = drawing.unitOverride.value_or(drawing.unit);
        const double mmPerUnit = unit == "px" ? 25.4 / drawing.dpi : unit == "pt" ? 25.4 / 72 : 1;
        return createPath(context, offsetPath(source, distance, cap, join, mmPerUnit));
    }
    catch (const std::exception &error)
    {
        return JS_ThrowTypeError(context, "%s", error.what());
    }
}

/// 数値半径または円・楕円・円弧のオプションを読み取る。従来の呼び出しは保持する。
bool readCircle(JSContext *context, JSValueConst value, Shape &shape)
{
    const double pi = std::acos(-1.0);
    double rx = 0, ry = 0, start = 0, end = 360;
    bool clockwise = true;
    if (JS_IsNumber(value))
        rx = ry = readCoordinate(context, value);
    else
    {
        if (!JS_IsObject(value) || JS_IsNull(value) || JS_IsArray(value) ||
            JS_IsFunction(context, value))
            throw std::runtime_error("circle expects a radius or options object");
        Property radius(context, value, "radius"), x(context, value, "radiusX"),
            y(context, value, "radiusY"), from(context, value, "start"), to(context, value, "end"),
            direction(context, value, "clockwise");
        for (auto v : {radius.get(), x.get(), y.get(), from.get(), to.get(), direction.get()})
            if (JS_IsException(v))
                return false;
        const bool ellipse = !JS_IsUndefined(x.get()) || !JS_IsUndefined(y.get());
        if (ellipse)
        {
            if (JS_IsUndefined(x.get()) || JS_IsUndefined(y.get()))
                throw std::runtime_error("Specify both radiusX and radiusY");
            rx = readCoordinate(context, x.get());
            ry = readCoordinate(context, y.get());
            // radiusが併記されている場合も、不正値は見逃さない。
            if (!JS_IsUndefined(radius.get()) && readCoordinate(context, radius.get()) <= 0)
                throw std::runtime_error("Radius must be positive");
        }
        else
            rx = ry = readCoordinate(context, radius.get());
        if (!JS_IsUndefined(from.get()))
            start = readCoordinate(context, from.get());
        if (!JS_IsUndefined(to.get()))
            end = readCoordinate(context, to.get());
        if (!JS_IsUndefined(direction.get()))
        {
            if (!JS_IsBool(direction.get()))
                throw std::runtime_error("clockwise must be a boolean");
            clockwise = JS_ToBool(context, direction.get()) != 0;
        }
    }
    if (rx <= 0 || ry <= 0)
        throw std::runtime_error("Radii must be positive");
    const double difference = end - start;
    if (difference == 0 || std::abs(difference) > 360)
        throw std::runtime_error("Arc angles must span more than 0 and at most 360 degrees");
    shape.radius = std::max(rx, ry);
    shape.minorRadius = std::min(rx, ry);
    shape.kind = rx == ry ? ShapeKind::Circle : ShapeKind::Ellipse;
    shape.angle = ry > rx ? pi / 2 : 0;
    shape.arc = std::abs(difference) < 360;
    const double ySign =
        static_cast<Drawing *>(JS_GetContextOpaque(context))->svgCoordinates ? 1 : -1;
    shape.curveStart = ySign * std::remainder(start, 360.0) * pi / 180 - shape.angle;
    double sweep = std::fmod(clockwise ? difference : -difference, 360.0);
    if (sweep < 0)
        sweep += 360;
    shape.curveSweep = ySign * (clockwise ? 1 : -1) * (shape.arc ? sweep : 360) * pi / 180;
    return true;
}

/// 基本図形の共通コールバック。magicの100番台は図面へ追加せずパスを返す。
JSValue drawShape(JSContext *context, JSValueConst, int argumentCount, JSValueConst *arguments,
                  int shapeCode)
{
    try
    {
        const bool valueOnly = shapeCode >= 100;
        const auto kind = static_cast<ShapeKind>(valueOnly ? shapeCode - 100 : shapeCode);
        const int geometryArguments = (kind == ShapeKind::Polygon || kind == ShapeKind::Polyline)
                                          ? 1
                                      : kind == ShapeKind::Circle ? 3
                                                                  : 4;
        if (argumentCount != geometryArguments && argumentCount != geometryArguments + 1)
            throw std::runtime_error("Invalid argument count");
        Shape shape{kind, {}};
        if (kind == ShapeKind::Polygon || kind == ShapeKind::Polyline)
        {
            if (!JS_IsArray(arguments[0]))
                throw std::runtime_error("polygon/polyline expects [[x,y], ...]");
            auto len = JS_GetPropertyStr(context, arguments[0], "length");
            uint32_t count = 0;
            JS_ToUint32(context, &count, len);
            JS_FreeValue(context, len);
            if (count < (kind == ShapeKind::Polyline ? 2u : 3u) || count > 100000)
                throw std::runtime_error(
                    "polygon requires 3 to 100000 points; polyline requires 2 to 100000 points");
            for (uint32_t i = 0; i < count; i++)
            {
                auto pair = JS_GetPropertyUint32(context, arguments[0], i),
                     x = JS_GetPropertyUint32(context, pair, 0),
                     y = JS_GetPropertyUint32(context, pair, 1);
                try
                {
                    shape.points.push_back(
                        {readCoordinate(context, x), readCoordinate(context, y)});
                }
                catch (...)
                {
                    JS_FreeValue(context, x);
                    JS_FreeValue(context, y);
                    JS_FreeValue(context, pair);
                    throw;
                }
                JS_FreeValue(context, x);
                JS_FreeValue(context, y);
                JS_FreeValue(context, pair);
            }
        }
        else
        {
            double x = readCoordinate(context, arguments[0]),
                   y = readCoordinate(context, arguments[1]);
            if (kind == ShapeKind::Circle)
            {
                shape.points = {{x, y}};
                if (!readCircle(context, arguments[2], shape))
                    return JS_EXCEPTION;
            }
            else
            {
                double value = readCoordinate(context, arguments[2]);
                double w = readCoordinate(context, arguments[3]);
                if (kind == ShapeKind::Rectangle)
                {
                    if (value <= 0 || w <= 0)
                        throw std::runtime_error("Width and height must be positive");
                    shape.points = {{x, y}, {x + value, y}, {x + value, y + w}, {x, y + w}};
                }
                else
                    shape.points = {{x, y}, {value, w}};
            }
        }
        if (argumentCount > geometryArguments &&
            !readStyle(context, arguments[geometryArguments], shape))
            return JS_EXCEPTION;
        if (valueOnly)
            return createPath(context, std::move(shape));
        auto &drawing = *static_cast<Drawing *>(JS_GetContextOpaque(context));
        if (drawing.shapes.size() >= 100000)
            throw std::runtime_error("Too many shapes");
        drawing.shapes.push_back(std::move(shape));
        return JS_UNDEFINED;
    }
    catch (const std::exception &e)
    {
        return JS_ThrowTypeError(context, "%s", e.what());
    }
}

} // namespace
/// 描画用JS関数をdrawingオブジェクトへまとめて登録する。
void registerDrawingApi(JSContext *context, Drawing &drawing)
{
    JS_SetContextOpaque(context, &drawing);
    JS_NewClassID(JS_GetRuntime(context), &drawing.pathClassId);
    JSClassDef pathClass{};
    pathClass.class_name = "ZfjPath";
    pathClass.finalizer = finalizePath;
    if (JS_NewClass(JS_GetRuntime(context), pathClassId(context), &pathClass) < 0)
        throw std::runtime_error("Cannot register path class");
    auto prototype = JS_NewObject(context);
    JS_SetPropertyStr(
        context, prototype, "move",
        JS_NewCFunctionMagic(context, moveValue, "move", 2, JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(
        context, prototype, "moved",
        JS_NewCFunctionMagic(context, moveValue, "moved", 2, JS_CFUNC_generic_magic, 3));
    JS_SetClassProto(context, pathClassId(context), prototype);
    auto g = JS_GetGlobalObject(context), api = JS_NewObject(context);
    auto pathApi = JS_NewObject(context);
    const char *names[] = {"line", "rect", "circle", "polygon"};
    for (int i = 0; i < 4; i++)
    {
        JS_SetPropertyStr(context, api, names[i],
                          JS_NewCFunctionMagic(context, drawShape, names[i],
                                               i == 3   ? 1
                                               : i == 2 ? 3
                                                        : 4,
                                               JS_CFUNC_generic_magic, i));
        JS_SetPropertyStr(context, pathApi, names[i],
                          JS_NewCFunctionMagic(context, drawShape, names[i],
                                               i == 3   ? 1
                                               : i == 2 ? 3
                                                        : 4,
                                               JS_CFUNC_generic_magic, 100 + i));
    }
    const int polylineCode = static_cast<int>(ShapeKind::Polyline);
    JS_SetPropertyStr(context, api, "polyline",
                      JS_NewCFunctionMagic(context, drawShape, "polyline", 1,
                                           JS_CFUNC_generic_magic, polylineCode));
    JS_SetPropertyStr(context, pathApi, "polyline",
                      JS_NewCFunctionMagic(context, drawShape, "polyline", 1,
                                           JS_CFUNC_generic_magic, 100 + polylineCode));
    JS_SetPropertyStr(
        context, pathApi, "move",
        JS_NewCFunctionMagic(context, moveValue, "move", 3, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(
        context, pathApi, "moved",
        JS_NewCFunctionMagic(context, moveValue, "moved", 3, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(
        context, pathApi, "svgcombine",
        JS_NewCFunctionMagic(context, combinePaths, "svgcombine", 3, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(
        context, pathApi, "combine",
        JS_NewCFunctionMagic(context, combinePaths, "combine", 3, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(
        context, pathApi, "transform",
        JS_NewCFunctionMagic(context, transformValue, "transform", 1, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(
        context, pathApi, "transformed",
        JS_NewCFunctionMagic(context, transformValue, "transformed", 1, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(context, pathApi, "offset",
                      JS_NewCFunction(context, offsetValue, "offset", 2));
    JS_SetPropertyStr(context, pathApi, "regularPolygon",
                      JS_NewCFunctionMagic(context, createRadialPath, "regularPolygon", 0,
                                           JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(
        context, pathApi, "star",
        JS_NewCFunctionMagic(context, createRadialPath, "star", 0, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(context, api, "regularPolygon",
                      JS_NewCFunctionMagic(context, createRadialPath, "regularPolygon", 0,
                                           JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(
        context, api, "star",
        JS_NewCFunctionMagic(context, createRadialPath, "star", 0, JS_CFUNC_generic_magic, 3));
    JS_SetPropertyStr(context, api, "draw", JS_NewCFunction(context, drawPath, "draw", 1));
    JS_SetPropertyStr(context, api, "setUnit", JS_NewCFunction(context, setUnit, "setUnit", 1));
    JS_SetPropertyStr(context, api, "setDpi", JS_NewCFunction(context, setDpi, "setDpi", 1));
    JS_SetPropertyStr(context, api, "setCanvas",
                      JS_NewCFunction(context, setCanvas, "setCanvas", 2));
    JS_SetPropertyStr(context, api, "setDxfMode",
                      JS_NewCFunction(context, setDxfMode, "setDxfMode", 1));
    JS_SetPropertyStr(context, api, "combine", JS_NewCFunction(context, combine, "combine", 2));
    JS_SetPropertyStr(context, g, "drawing", api);
    JS_SetPropertyStr(context, g, "path", pathApi);
    JS_FreeValue(context, g);
}
} // namespace zfj
