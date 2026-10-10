#include "clipper2/clipper.h"
#include "drawing.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace zfj
{
namespace
{
/// mmベースの座標で領域演算する。整数化は小数4桁、塗り規則はNonZero。
Clipper2Lib::PathsD booleanOperation(Clipper2Lib::ClipType operation,
                                     const Clipper2Lib::PathsD &subjects,
                                     const Clipper2Lib::PathsD &clips = {})
{
    Clipper2Lib::ClipperD engine(4);
    engine.AddSubject(subjects);
    engine.AddClip(clips);
    Clipper2Lib::PathsD result;
    if (!engine.Execute(operation, Clipper2Lib::FillRule::NonZero, result))
        throw std::runtime_error("SVG path boolean operation failed");
    return result;
}

/// 元の閉じた図形をmm単位の輪郭へ変換する。円の最大近似誤差は0.01mm。
Clipper2Lib::PathsD primitivePaths(const Shape &shape, double mmPerUnit)
{
    Clipper2Lib::PathD path;
    if (shape.kind == ShapeKind::Circle || shape.kind == ShapeKind::Ellipse)
    {
        for (auto point : sampleCurve(shape, mmPerUnit))
            path.push_back({point.x * mmPerUnit, point.y * mmPerUnit});
    }
    else
    {
        for (auto point : shape.points)
            path.push_back({point.x * mmPerUnit, point.y * mmPerUnit});
    }
    return booleanOperation(Clipper2Lib::ClipType::Union, {path});
}

/// 子グループを先に解決し、ORは和、ANDは全対象の共通部分、XORは順次排他的論理和にする。
Clipper2Lib::PathsD resolveGroup(const Shape &shape, double mmPerUnit)
{
    if (shape.kind == ShapeKind::SvgComposite)
        throw std::runtime_error("path.combine cannot calculate an SVG rendering composite; use "
                                 "path.combine for its operands too");
    if (shape.kind != ShapeKind::BooleanGroup)
        return primitivePaths(shape, mmPerUnit);
    Clipper2Lib::PathsD result;
    if (shape.operation == "or")
    {
        // ORは全輪郭を一度に処理し、多数の図形を順次結合するコストを避ける。
        for (const auto &child : shape.children)
        {
            auto paths = resolveGroup(child, mmPerUnit);
            result.insert(result.end(), std::make_move_iterator(paths.begin()),
                          std::make_move_iterator(paths.end()));
        }
        return booleanOperation(Clipper2Lib::ClipType::Union, result);
    }
    bool first = true;
    for (const auto &child : shape.children)
    {
        auto operand = resolveGroup(child, mmPerUnit);
        if (first)
        {
            result = std::move(operand);
            first = false;
        }
        else
            result = booleanOperation(shape.operation == "and" ? Clipper2Lib::ClipType::Intersection
                                                               : Clipper2Lib::ClipType::Xor,
                                      result, operand);
    }
    return result;
}

/// DXF用に元の図形を再帰的に取り出す。グループ指定のスタイルは適用しない。
void appendOriginal(const Shape &shape, std::vector<Shape> &output)
{
    if (shape.kind == ShapeKind::BooleanGroup || shape.kind == ShapeKind::SvgComposite)
    {
        for (const auto &child : shape.children)
            appendOriginal(child, output);
    }
    else
        output.push_back(shape);
}
} // namespace

/// SVG表示合成の子にあるClipper2演算だけを解決し、SVG表示合成自体は保持する。
void resolveShape(Shape &shape, double mmPerUnit)
{
    if (shape.kind == ShapeKind::SvgComposite)
    {
        for (auto &child : shape.children)
            resolveShape(child, mmPerUnit);
        return;
    }
    if (shape.kind != ShapeKind::BooleanGroup)
        return;
    auto paths = resolveGroup(shape, mmPerUnit);
    shape.kind = ShapeKind::Path;
    for (const auto &path : paths)
    {
        std::vector<Point> contour;
        for (auto point : path)
            contour.push_back({point.x / mmPerUnit, point.y / mmPerUnit});
        shape.paths.push_back(std::move(contour));
    }
    shape.children.clear();
}

/// SVGでだけ合成を実行する。穴を含む複数輪郭を元の入力単位へ戻して保持する。
Drawing resolveSvgPaths(Drawing drawing, double mmPerUnit)
{
    for (auto &shape : drawing.shapes)
        resolveShape(shape, mmPerUnit);
    return drawing;
}

/// DXF向けに論理演算グループを解消し、元の図形とスタイルの描画順を復元する。
Drawing expandOriginalShapes(Drawing drawing)
{
    std::vector<Shape> originals;
    for (const auto &shape : drawing.shapes)
        appendOriginal(shape, originals);
    drawing.shapes = std::move(originals);
    return drawing;
}
} // namespace zfj
