#include "drawing.h"
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace zfj
{
namespace
{
/// SVGの元図形と表示合成をdefsへ保存する。Clipper2は呼び出さない。
class CompositeWriter
{
  public:
    CompositeWriter(std::ostream &out, double x, double y, double width, double height,
                    unsigned &nextId)
        : out_(out), x_(x), y_(y), width_(width), height_(height), nextId_(nextId)
    {
    }

    /// 図面内で一意の参照IDを発行する。
    std::string id()
    {
        return "zfj" + std::to_string(nextId_++);
    }

    /// フィルターやマスクの範囲を入力座標で明示する。
    void bounds()
    {
        out_ << " x=\"" << x_ << "\" y=\"" << y_ << "\" width=\"" << width_ << "\" height=\""
             << height_ << "\"";
    }

    /// 元図形を参照する。色は継承し、マスク内では白・黒を直接指定する。
    void use(const std::string &reference, const char *fill = nullptr)
    {
        out_ << "<use xlink:href=\"#" << reference << "\"";
        if (fill)
            out_ << " fill=\"" << fill << "\"";
        out_ << "/>";
    }

    /// 指定領域を通すマスク、またはその補集合のマスクを定義する。
    std::string mask(const std::string &reference, bool inverse = false)
    {
        const auto name = id();
        out_ << "<mask id=\"" << name
             << "\" maskUnits=\"userSpaceOnUse\" maskContentUnits=\"userSpaceOnUse\""
                " mask-type=\"luminance\" style=\"mask-type:luminance\"";
        bounds();
        out_ << ">";
        if (inverse)
        {
            out_ << "<rect";
            bounds();
            out_ << " fill=\"white\" stroke=\"none\"/>";
        }
        use(reference, inverse ? "black" : "white");
        out_ << "</mask>";
        return name;
    }

    /// 二つの領域をSVGのマスクで合成する。元図形は参照として残す。
    std::string pair(const std::string &a, const std::string &b, const std::string &operation)
    {
        const auto name = id();
        Region node{nullptr, a, b, operation};
        if (operation == "or")
        {
            out_ << "<g id=\"" << name << "\">";
            use(a);
            use(b);
        }
        else if (operation == "and")
        {
            const auto clip = mask(b);
            node.maskB = clip;
            out_ << "<g id=\"" << name << "\" mask=\"url(#" << clip << ")\">";
            use(a);
        }
        else
        {
            const auto notA = mask(a, true), notB = mask(b, true);
            node.maskA = notA;
            node.maskB = notB;
            out_ << "<g id=\"" << name << "\"><g mask=\"url(#" << notB << ")\">";
            use(a);
            out_ << "</g><g mask=\"url(#" << notA << ")\">";
            use(b);
            out_ << "</g>";
        }
        out_ << "</g>";
        regions_.emplace(name, std::move(node));
        return name;
    }

    /// 円はcircle、長方形はrectとして保持し、合成を再帰的な参照へ変換する。
    std::string region(const Shape &shape)
    {
        if (shape.kind == ShapeKind::SvgComposite)
        {
            auto result = region(shape.children.front());
            for (size_t i = 1; i < shape.children.size(); ++i)
                result = pair(result, region(shape.children[i]), shape.operation);
            return result;
        }
        const auto name = id();
        // fillを固定すると黒いuseから継承できず、補集合マスクが白くなるため指定しない。
        out_ << "<g id=\"" << name << "\" stroke=\"none\">";
        primitive(shape);
        out_ << "</g>";
        regions_.emplace(name, Region{&shape});
        return name;
    }

    /// 元図形を通常の描画オブジェクトとして出力する。塗り用の全面rectへ置換しない。
    void paint(const std::string &reference)
    {
        const auto &node = regions_.at(reference);
        if (node.shape)
        {
            out_ << "<g data-zfj-source=\"original\">";
            primitive(*node.shape);
            out_ << "</g>";
            return;
        }
        if (node.operation == "or")
        {
            paint(node.a);
            paint(node.b);
            return;
        }
        out_ << "<g mask=\"url(#" << node.maskB << ")\">";
        paint(node.a);
        out_ << "</g>";
        if (node.operation == "xor")
        {
            out_ << "<g mask=\"url(#" << node.maskA << ")\">";
            paint(node.b);
            out_ << "</g>";
        }
    }

    /// 円・長方形・多角形の元の幾何データを保持して書き出す。
    void primitive(const Shape &shape)
    {
        if (shape.kind == ShapeKind::Circle)
            out_ << "<circle cx=\"" << shape.points[0].x << "\" cy=\"" << shape.points[0].y
                 << "\" r=\"" << shape.radius << "\"/>";
        else if (shape.kind == ShapeKind::Ellipse)
        {
            writeSvgEllipse(out_, shape);
            out_ << "/>";
        }
        else if (shape.kind == ShapeKind::Rectangle)
            out_ << "<rect x=\"" << shape.points[0].x << "\" y=\"" << shape.points[0].y
                 << "\" width=\"" << shape.points[1].x - shape.points[0].x << "\" height=\""
                 << shape.points[2].y - shape.points[0].y << "\"/>";
        else if (shape.kind == ShapeKind::Path)
        {
            out_ << "<path fill-rule=\"nonzero\" d=\"";
            for (const auto &contour : shape.paths)
            {
                if (contour.empty())
                    continue;
                out_ << 'M' << contour[0].x << ' ' << contour[0].y;
                for (size_t i = 1; i < contour.size(); ++i)
                    out_ << 'L' << contour[i].x << ' ' << contour[i].y;
                out_ << 'Z';
            }
            out_ << "\"/>";
        }
        else
        {
            out_ << "<polygon points=\"";
            for (auto point : shape.points)
                out_ << point.x << ',' << point.y << ' ';
            out_ << "\"/>";
        }
    }

  private:
    /// 表示ツリーの参照と、その枝に適用するマスク。shapeはrender中だけ参照する。
    struct Region
    {
        const Shape *shape = nullptr;
        std::string a, b, operation, maskA, maskB;
    };
    std::ostream &out_;                               ///< SVGの出力先。
    double x_, y_, width_, height_;                   ///< 表示領域とマスクの処理範囲。
    unsigned &nextId_;                                ///< 図面全体で共有するIDカウンター。
    std::unordered_map<std::string, Region> regions_; ///< 描画用の元図形と合成構造。
};

/// RGB色をSVGの16進数表記へ変換する。
std::string rgb(uint32_t color)
{
    std::ostringstream text;
    text << '#' << std::hex << std::setfill('0') << std::setw(6) << color;
    return text.str();
}

/// 円・楕円・既知の単純輪郭のXORは、偶奇規則の複合パスで表せる。
bool supportsCompoundXor(const Shape &shape)
{
    if (shape.kind == ShapeKind::Rectangle || shape.kind == ShapeKind::Circle ||
        shape.kind == ShapeKind::Ellipse ||
        (shape.kind == ShapeKind::Polygon && shape.simpleContour))
        return true;
    if (shape.kind != ShapeKind::SvgComposite || shape.operation != "xor")
        return false;
    for (const auto &child : shape.children)
        if (!supportsCompoundXor(child))
            return false;
    return true;
}

/// 元の輪郭を閉じたサブパスとして保存する。円は折れ線にせず円弧を使用する。
void writeCompoundContours(std::ostream &out, const Shape &shape)
{
    if (shape.kind == ShapeKind::SvgComposite)
    {
        for (const auto &child : shape.children)
            writeCompoundContours(out, child);
        return;
    }
    const auto p = shape.points.front();
    if (shape.kind == ShapeKind::Circle || shape.kind == ShapeKind::Ellipse)
    {
        writeSvgCurveContour(out, shape);
        return;
    }
    out << 'M' << p.x << ' ' << p.y;
    for (size_t i = 1; i < shape.points.size(); ++i)
        out << 'L' << shape.points[i].x << ' ' << shape.points[i].y;
    out << 'Z';
}
} // namespace

/// 表示合成を元図形・マスク・フィルターとして書き出し、計算済み輪郭とは区別する。
void writeSvgComposite(std::ostream &out, const Shape &shape, double mmPerUnit, double x, double y,
                       double width, double height, unsigned &nextId)
{
    if (supportsCompoundXor(shape))
    {
        out << "<path data-zfj-combine=\"svg\" data-zfj-artwork=\"compound\""
               " fill-rule=\"evenodd\" fill=\""
            << (shape.style.fill ? rgb(*shape.style.fill) : "none") << "\" stroke=\""
            << (shape.style.stroke ? rgb(*shape.style.stroke) : "none") << "\" stroke-width=\""
            << shape.style.strokeWidth.value_or(0.25 / mmPerUnit) << "\" d=\"";
        writeCompoundContours(out, shape);
        out << "\"/>\n";
        return;
    }
    CompositeWriter writer(out, x, y, width, height, nextId);
    out << "<g data-zfj-combine=\"svg\" stroke=\"none\"><defs>";
    const auto region = writer.region(shape);
    std::string strokeFilter;
    if (shape.style.stroke)
    {
        strokeFilter = writer.id();
        const double radius = shape.style.strokeWidth.value_or(0.25 / mmPerUnit) / 2;
        out << "<filter id=\"" << strokeFilter << "\" filterUnits=\"userSpaceOnUse\"";
        writer.bounds();
        out << "><feMorphology in=\"SourceAlpha\" operator=\"dilate\" radius=\"" << radius
            << "\" result=\"outer\"/><feMorphology in=\"SourceAlpha\" operator=\"erode\" radius=\""
            << radius
            << "\" result=\"inner\"/><feComposite in=\"outer\" in2=\"inner\""
               " operator=\"out\" result=\"edge\"/><feFlood flood-color=\""
            << rgb(*shape.style.stroke)
            << "\"/><feComposite in2=\"edge\" operator=\"in\"/></filter>";
    }
    out << "</defs>";
    if (shape.style.fill)
    {
        out << "<g data-zfj-artwork=\"originals\" fill=\"" << rgb(*shape.style.fill) << "\">";
        writer.paint(region);
        out << "</g>";
    }
    if (shape.style.stroke)
    {
        out << "<g data-zfj-artwork=\"outline\" fill=\"white\" filter=\"url(#" << strokeFilter
            << ")\">";
        writer.paint(region);
        out << "</g>";
    }
    out << "</g>\n";
}
} // namespace zfj
