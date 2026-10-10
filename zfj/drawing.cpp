#include "drawing.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>

namespace zfj
{
namespace
{
/// 24bit RGBを安全なSVG属性文字列へ変換する。描画なしはnoneで表す。
std::string colorText(const std::optional<uint32_t> &color)
{
    if (!color)
        return "none";
    std::ostringstream text;
    text << '#' << std::hex << std::setfill('0') << std::setw(6) << *color;
    return text.str();
}

/// 1図形のスタイルをSVG属性へ変換する。線幅の既定は実寸0.25mm。
void writeSvgStyle(std::ostream &output, const Shape &shape, double mmPerUnit)
{
    output << " stroke=\"" << colorText(shape.style.stroke) << "\" fill=\""
           << colorText(shape.style.fill) << "\" stroke-width=\""
           << shape.style.strokeWidth.value_or(0.25 / mmPerUnit) << "\"";
}

/// 入力座標を保持してSVGを生成する。pxの物理サイズはpt表記でDPIを反映する。
std::string renderSvg(const Drawing &drawing, const std::string &unit, double mmPerUnit)
{
    const bool points = unit != "mm";
    const bool pixelCanvas = drawing.canvas && unit == "px";
    const double physicalScale = pixelCanvas ? 96.0 / drawing.dpi
                                 : points    ? mmPerUnit * 72.0 / 25.4
                                             : 1.0;
    double margin = 1.0 / mmPerUnit;
    double minX = 0, minY = 0, maxX = 1, maxY = 1;
    bool first = true;
    // 図形の外接矩形へ座標を追加する。空の図面は1単位四方とする。
    auto includePoint = [&](double x, double y)
    {
        if (first)
        {
            minX = maxX = x;
            minY = maxY = y;
            first = false;
        }
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    };
    std::function<void(const Shape &)> includeShape = [&](const Shape &shape)
    {
        const auto bounds = pathBounds(shape);
        if (bounds.valid)
        {
            includePoint(bounds.minX, bounds.minY);
            includePoint(bounds.maxX, bounds.maxY);
        }
        // 丸い角のストロークが太くても表示範囲で切れないよう余白を広げる。
        if (shape.style.stroke)
            margin = std::max(margin, shape.style.strokeWidth.value_or(0.25 / mmPerUnit) / 2);
        for (const auto &child : shape.children)
            includeShape(child);
    };
    for (const auto &shape : drawing.shapes)
        includeShape(shape);
    if (drawing.canvas)
    {
        // 固定キャンバスは自動余白を付けず、原点と寸法をそのまま保持する。
        minX = minY = 0;
        maxX = drawing.canvas->width;
        maxY = drawing.canvas->height;
        margin = 0;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(15);
    const char *suffix = pixelCanvas ? "px" : points ? "pt" : "mm";
    output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg "
              "xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
              "width=\""
           << (maxX - minX + 2 * margin) * physicalScale << suffix << "\" height=\""
           << (maxY - minY + 2 * margin) * physicalScale << suffix << "\" viewBox=\""
           << minX - margin << ' ' << minY - margin << ' ' << maxX - minX + 2 * margin << ' '
           << maxY - minY + 2 * margin
           << "\" overflow=\"hidden\">\n<g fill=\"none\" stroke=\"black\" stroke-width=\""
           << 0.25 / mmPerUnit << "\" stroke-linejoin=\"round\">\n";
    unsigned nextId = 0;
    for (const auto &shape : drawing.shapes)
    {
        if (shape.kind == ShapeKind::SvgComposite)
        {
            writeSvgComposite(output, shape, mmPerUnit, minX - margin, minY - margin,
                              maxX - minX + 2 * margin, maxY - minY + 2 * margin, nextId);
            continue;
        }
        if (shape.kind == ShapeKind::Path)
        {
            if (shape.paths.empty())
                continue;
            output << "<path d=\"";
            for (const auto &contour : shape.paths)
            {
                if (contour.empty())
                    continue;
                output << 'M' << contour[0].x << ' ' << contour[0].y;
                for (size_t i = 1; i < contour.size(); ++i)
                    output << 'L' << contour[i].x << ' ' << contour[i].y;
                output << 'Z';
            }
            output << "\" fill-rule=\"nonzero\"";
            writeSvgStyle(output, shape, mmPerUnit);
            output << "/>\n";
            continue;
        }
        const auto point = shape.points.front();
        if (shape.kind == ShapeKind::Line)
        {
            output << "<line x1=\"" << point.x << "\" y1=\"" << point.y << "\" x2=\""
                   << shape.points[1].x << "\" y2=\"" << shape.points[1].y << "\"";
        }
        else if (shape.arc)
        {
            output << "<path d=\"";
            writeSvgCurveContour(output, shape);
            output << "\"";
        }
        else if (shape.kind == ShapeKind::Circle)
        {
            output << "<circle cx=\"" << point.x << "\" cy=\"" << point.y << "\" r=\""
                   << shape.radius << "\"";
        }
        else if (shape.kind == ShapeKind::Ellipse)
            writeSvgEllipse(output, shape);
        else
        {
            output << (shape.kind == ShapeKind::Polyline ? "<polyline points=\""
                                                         : "<polygon points=\"");
            for (auto vertex : shape.points)
                output << vertex.x << ',' << vertex.y << ' ';
            output << "\"";
        }
        writeSvgStyle(output, shape, mmPerUnit);
        output << "/>\n";
    }
    output << "</g>\n</svg>\n";
    return output.str();
}
} // namespace

/// 単位を解決し、SVGまたはDXFの出力処理へ委譲する。コピー上で換算して元の図面を保つ。
std::string render(Drawing drawing, bool svg, const std::string &unit, bool fusion)
{
    const double mmPerUnit = unit == "px" ? 25.4 / drawing.dpi : unit == "pt" ? 25.4 / 72.0 : 1.0;
    if (svg)
        return renderSvg(resolveSvgPaths(std::move(drawing), mmPerUnit), unit, mmPerUnit);
    drawing = expandOriginalShapes(std::move(drawing));
    for (auto &shape : drawing.shapes)
    {
        if ((fusion || drawing.fusion) && shape.kind == ShapeKind::Ellipse)
        {
            shape.points = sampleCurve(shape, mmPerUnit);
            shape.kind = shape.arc ? ShapeKind::Polyline : ShapeKind::Polygon;
            shape.arc = false;
        }
        for (auto &point : shape.points)
        {
            point.x *= mmPerUnit;
            point.y *= mmPerUnit;
        }
        for (auto &contour : shape.paths)
            for (auto &point : contour)
            {
                point.x *= mmPerUnit;
                point.y *= mmPerUnit;
            }
        shape.radius *= mmPerUnit;
        shape.minorRadius *= mmPerUnit;
        if (shape.style.strokeWidth)
            *shape.style.strokeWidth *= mmPerUnit;
    }
    return renderDxf(drawing, fusion || drawing.fusion);
}
} // namespace zfj
