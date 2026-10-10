#include "drawing.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace zfj
{
namespace
{
/// AutoCADの規定線幅（1/100mm）から最も近いものを選ぶ。同距離なら細い方を選ぶ。
int nearestLineweight(double millimetres)
{
    const int weights[] = {0,  5,  9,  13, 15, 18,  20,  25,  30,  35,  40,  50,
                           53, 60, 70, 80, 90, 100, 106, 120, 140, 158, 200, 211};
    const double requested = millimetres * 100;
    return *std::min_element(std::begin(weights), std::end(weights), [requested](int a, int b)
                             { return std::abs(a - requested) < std::abs(b - requested); });
}

/// DXFのタグ列とハンドルを管理する。状態は1回の出力内だけで保持する。
class DxfWriter
{
  public:
    /// ロケールに依存しない小数点表記と座標精度を設定する。
    explicit DxfWriter(bool fusion) : fusion_(fusion)
    {
        output_.imbue(std::locale::classic());
        output_ << std::setprecision(15);
    }

    /// グループコードと値の組を1つ書き出す。
    template <class T> void tag(int code, const T &value)
    {
        output_ << code << '\n' << value << '\n';
    }

    /// 文書全体を構築する。図形は入力順に塗り→輪郭の順で追加する。
    std::string write(const Drawing &drawing)
    {
        tag(0, "SECTION");
        tag(2, "HEADER");
        tag(9, "$ACADVER");
        tag(1, "AC1018");
        tag(9, "$INSUNITS");
        tag(70, 4); // DXF座標はmm。
        tag(9, "$MEASUREMENT");
        tag(70, 1);
        tag(9, "$LWDISPLAY");
        tag(290, 1);
        tag(0, "ENDSEC");
        writeTables();
        writeBlocks();
        tag(0, "SECTION");
        tag(2, "ENTITIES");
        for (const auto &shape : drawing.shapes)
        {
            if (!fusion_ && shape.style.fill)
                writeHatch(shape);
            // Fusionでは塗りだけの図形も輪郭化し、完全に非表示の図形は省略する。
            if (shape.style.stroke || (fusion_ && shape.style.fill))
                writeOutline(shape);
        }
        tag(0, "ENDSEC");
        tag(0, "EOF");
        return output_.str();
    }

  private:
    std::ostringstream output_;   ///< 文書のタグ列。
    unsigned nextHandle_ = 0x100; ///< テーブルの固定ハンドルと重ならないエンティティ番号。
    bool fusion_;                 ///< 色・線幅・HATCHを除去してスケッチの輪郭だけを残す。

    /// 新しいエンティティ用ハンドルを16進文字列で発行する。
    std::string handle()
    {
        std::ostringstream value;
        value << std::hex << std::uppercase << nextHandle_++;
        return value.str();
    }

    /// 最小の線種・レイヤー・モデル空間レコードを定義する。
    void writeTables()
    {
        tag(0, "SECTION");
        tag(2, "TABLES");
        tag(0, "TABLE");
        tag(2, "LTYPE");
        tag(5, "1");
        tag(100, "AcDbSymbolTable");
        tag(70, 1);
        tag(0, "LTYPE");
        tag(5, "2");
        tag(330, "1");
        tag(100, "AcDbSymbolTableRecord");
        tag(100, "AcDbLinetypeTableRecord");
        tag(2, "CONTINUOUS");
        tag(70, 0);
        tag(3, "Solid line");
        tag(72, 65);
        tag(73, 0);
        tag(40, 0);
        tag(0, "ENDTAB");
        tag(0, "TABLE");
        tag(2, "LAYER");
        tag(5, "3");
        tag(100, "AcDbSymbolTable");
        tag(70, 1);
        tag(0, "LAYER");
        tag(5, "4");
        tag(330, "3");
        tag(100, "AcDbSymbolTableRecord");
        tag(100, "AcDbLayerTableRecord");
        tag(2, "0");
        tag(70, 0);
        tag(62, 7);
        tag(6, "CONTINUOUS");
        tag(0, "ENDTAB");
        tag(0, "TABLE");
        tag(2, "BLOCK_RECORD");
        tag(5, "5");
        tag(100, "AcDbSymbolTable");
        tag(70, 2);
        for (auto item : {std::pair{"6", "*Model_Space"}, std::pair{"7", "*Paper_Space"}})
        {
            tag(0, "BLOCK_RECORD");
            tag(5, item.first);
            tag(330, "5");
            tag(100, "AcDbSymbolTableRecord");
            tag(100, "AcDbBlockTableRecord");
            tag(2, item.second);
            tag(70, 0);
            tag(280, 1);
            tag(281, 0);
        }
        tag(0, "ENDTAB");
        tag(0, "ENDSEC");
    }

    /// モデル空間・ペーパー空間の空ブロックを出力する。
    void writeBlocks()
    {
        tag(0, "SECTION");
        tag(2, "BLOCKS");
        for (auto item : {std::pair{"6", "*Model_Space"}, std::pair{"7", "*Paper_Space"}})
        {
            tag(0, "BLOCK");
            tag(5, handle());
            tag(330, item.first);
            tag(100, "AcDbEntity");
            tag(8, "0");
            tag(100, "AcDbBlockBegin");
            tag(2, item.second);
            tag(70, 0);
            tag(10, 0);
            tag(20, 0);
            tag(30, 0);
            tag(3, item.second);
            tag(1, "");
            tag(0, "ENDBLK");
            tag(5, handle());
            tag(330, item.first);
            tag(100, "AcDbEntity");
            tag(8, "0");
            tag(100, "AcDbBlockEnd");
        }
        tag(0, "ENDSEC");
    }

    /// レイヤー・RGB色・線幅の共通属性を出力する。420のTrueColorを優先させる。
    void beginEntity(const char *type, uint32_t color, int weight)
    {
        tag(0, type);
        tag(5, handle());
        tag(330, "6");
        tag(100, "AcDbEntity");
        tag(8, "0");
        tag(62, fusion_ ? 256 : 7);
        if (!fusion_)
        {
            tag(420, color);
            tag(370, weight);
        }
    }

    /// 輪郭をLINE、CIRCLE、閉じたLWPOLYLINEのいずれかとして出力する。
    void writeOutline(const Shape &shape)
    {
        if (shape.kind == ShapeKind::Path)
        {
            for (const auto &contour : shape.paths)
            {
                Shape polygon{ShapeKind::Polygon, contour};
                polygon.style = shape.style;
                writeOutline(polygon);
            }
            return;
        }
        const auto point = shape.points.front();
        const int weight = nearestLineweight(shape.style.strokeWidth.value_or(0.25));
        if (shape.kind == ShapeKind::Line)
        {
            beginEntity("LINE", shape.style.stroke.value_or(0), weight);
            tag(100, "AcDbLine");
            tag(10, point.x);
            tag(20, point.y);
            tag(30, 0);
            tag(11, shape.points[1].x);
            tag(21, shape.points[1].y);
            tag(31, 0);
        }
        else if (shape.kind == ShapeKind::Circle)
        {
            beginEntity(shape.arc ? "ARC" : "CIRCLE", shape.style.stroke.value_or(0), weight);
            tag(100, "AcDbCircle");
            tag(10, point.x);
            tag(20, point.y);
            tag(30, 0);
            tag(40, shape.radius);
            if (shape.arc)
            {
                // DXFのARCは常に反時計回り。負方向は端点を交換する。
                const double start =
                    shape.curveStart + (shape.curveSweep < 0 ? shape.curveSweep : 0);
                const double end = shape.curveStart + (shape.curveSweep > 0 ? shape.curveSweep : 0);
                auto degrees = [](double value)
                {
                    double result = std::fmod(value * 180 / std::acos(-1.0), 360.0);
                    return result < 0 ? result + 360 : result;
                };
                tag(100, "AcDbArc");
                tag(50, degrees(start));
                tag(51, degrees(end));
            }
        }
        else if (shape.kind == ShapeKind::Ellipse)
        {
            beginEntity("ELLIPSE", shape.style.stroke.value_or(0), weight);
            tag(100, "AcDbEllipse");
            tag(10, point.x);
            tag(20, point.y);
            tag(30, 0);
            tag(11, shape.radius * std::cos(shape.angle));
            tag(21, shape.radius * std::sin(shape.angle));
            tag(31, 0);
            tag(40, shape.minorRadius / shape.radius);
            const double start =
                shape.arc ? shape.curveStart + (shape.curveSweep < 0 ? shape.curveSweep : 0) : 0;
            const double end =
                shape.arc ? shape.curveStart + (shape.curveSweep > 0 ? shape.curveSweep : 0)
                          : 2 * std::acos(-1.0);
            auto normalized = [](double t)
            {
                double result = std::fmod(t, 2 * std::acos(-1.0));
                return result < 0 ? result + 2 * std::acos(-1.0) : result;
            };
            tag(41, shape.arc ? normalized(start) : start);
            tag(42, shape.arc ? normalized(end) : end);
        }
        else
        {
            beginEntity("LWPOLYLINE", shape.style.stroke.value_or(0), weight);
            tag(100, "AcDbPolyline");
            tag(90, shape.points.size());
            tag(70, shape.kind == ShapeKind::Polyline ? 0 : 1);
            for (auto vertex : shape.points)
            {
                tag(10, vertex.x);
                tag(20, vertex.y);
            }
        }
    }

    /// 塗りを非関連のソリッドHATCHで出力する。円境界は2本の半円弧で正確に表す。
    void writeHatch(const Shape &shape)
    {
        beginEntity("HATCH", *shape.style.fill, 0);
        tag(100, "AcDbHatch");
        tag(10, 0);
        tag(20, 0);
        tag(30, 0);
        tag(210, 0);
        tag(220, 0);
        tag(230, 1);
        tag(2, "SOLID");
        tag(70, 1);
        tag(71, 0);
        tag(91, shape.kind == ShapeKind::Path ? shape.paths.size() : 1);
        if (shape.kind == ShapeKind::Path)
        {
            // 最大の輪郭は外周。全体を鏡面反転しても外周・穴の区別を維持する。
            auto areaOf = [](const std::vector<Point> &contour)
            {
                double area = 0;
                for (size_t i = 0; i < contour.size(); ++i)
                {
                    const auto a = contour[i], b = contour[(i + 1) % contour.size()];
                    area += (a.x - contour[0].x) * (b.y - contour[0].y) -
                            (b.x - contour[0].x) * (a.y - contour[0].y);
                }
                return area;
            };
            double outerArea = 0;
            for (const auto &contour : shape.paths)
                if (std::abs(areaOf(contour)) > std::abs(outerArea))
                    outerArea = areaOf(contour);
            for (const auto &contour : shape.paths)
            {
                tag(92, (areaOf(contour) > 0) == (outerArea > 0) ? 3 : 2);
                tag(72, 0);
                tag(73, 1);
                tag(93, contour.size());
                for (const auto p : contour)
                {
                    tag(10, p.x);
                    tag(20, p.y);
                }
                tag(97, 0);
            }
            tag(75, 0);
            tag(76, 1);
            tag(98, 0);
            return;
        }
        if (shape.kind == ShapeKind::Circle || shape.kind == ShapeKind::Ellipse)
        {
            tag(92, 1);
            tag(93, 2);
            for (int start : {0, 180})
            {
                tag(72, shape.kind == ShapeKind::Circle ? 2 : 3);
                tag(10, shape.points[0].x);
                tag(20, shape.points[0].y);
                if (shape.kind == ShapeKind::Circle)
                    tag(40, shape.radius);
                else
                {
                    tag(11, shape.radius * std::cos(shape.angle));
                    tag(21, shape.radius * std::sin(shape.angle));
                    tag(40, shape.minorRadius / shape.radius);
                }
                tag(50, start);
                tag(51, start + 180);
                tag(73, 1);
            }
        }
        else
        {
            tag(92, 3);
            tag(72, 0);
            tag(73, 1);
            tag(93, shape.points.size());
            for (auto point : shape.points)
            {
                tag(10, point.x);
                tag(20, point.y);
            }
        }
        tag(97, 0);
        tag(75, 0);
        tag(76, 1);
        tag(98, 0);
    }
};
} // namespace

/// mmへ換算済みの図面をAutoCAD 2004 ASCII DXFとして保存可能な文字列へ変換する。
std::string renderDxf(const Drawing &drawing, bool fusion)
{
    return DxfWriter(fusion).write(drawing);
}
} // namespace zfj
