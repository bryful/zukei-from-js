#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace zfj
{

/// 図面の入力単位で表した二次元座標。
struct Point
{
    double x;
    double y;
};

/// JSの描画関数と対応する図形の種類。
enum class ShapeKind
{
    Line,
    Rectangle,
    Circle,
    Polygon,
    BooleanGroup,
    Path,
    SvgComposite, ///< SVGのマスクで表示時に合成する。輪郭の分割・計算はしない。
    Ellipse,      ///< 円を縦横別倍率で拡縮した楕円。円弧として保持する。
    Polyline      ///< 2点以上の開いた折れ線。塗りと領域演算は行わない。
};

/// 図形の線と塗り。色は24bit RGB、nulloptは描画なしを表す。
/// 線幅の未指定時は実寸0.25mm、指定時は図面の入力単位で解釈する。
struct Style
{
    std::optional<uint32_t> stroke = 0x000000;
    std::optional<uint32_t> fill;
    std::optional<double> strokeWidth;
};

/// 1つの図形。円は中心点と半径、その他は端点または輪郭の頂点を保持する。
struct Shape
{
    ShapeKind kind;
    std::vector<Point> points;
    double radius = 0;
    Style style;
    std::string operation;                 ///< BooleanGroup / SvgCompositeのand/or/xor。
    std::vector<Shape> children;           ///< DXFでは合成せず、この元図形を展開する。
    std::vector<std::vector<Point>> paths; ///< SVG合成結果の複数輪郭。穴も別輪郭で保持する。
    double minorRadius = 0;                ///< Ellipseの短半径。radiusは長半径。
    double angle = 0;                      ///< Ellipseの長軸方向。ラジアン。
    bool simpleContour = false;            ///< 正多角形・星型・長方形変換由来の単純輪郭。
};

/// 線幅を含めない図形の外接矩形。空の図形ではvalid=false。
struct Bounds
{
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool valid = false;
    void include(Point point); ///< 点を外接矩形へ追加する。
};

/// 図形や合成の元図形全体の外接矩形を返す。
Bounds pathBounds(const Shape &shape);

/// アンカーを基準にスケール、度数回転、相対移動を行う。線幅は変えない。
void transformPath(Shape &shape, Point pos, double degrees, Point scale, Point anchor);

/// 幾何データの種類と線幅を変えず、全頂点・円中心を相対移動する。
void movePath(Shape &shape, Point offset);

/// 正多角形を作る。edgeRadius=trueならradiusは中心から辺の中心までの距離。
Shape makeRegularPolygon(Point center, double radius, unsigned sides, bool edgeRadius, double rot);

/// 外半径と内半径を交互に持つ星型を作る。pointsは外側の頂点数、rotは度数。
Shape makeStar(Point center, double outerRadius, double innerRadius, unsigned points, double rot);

/// 円・楕円上の点。パラメータは長軸からのラジアン。
Point curvePoint(const Shape &shape, double parameter);

/// Clipper2とFusion用に円・楕円を最大偏差0.01mmで近似する。
std::vector<Point> sampleCurve(const Shape &shape, double mmPerUnit);

/// SVGの楕円要素を出力する。スタイルは呼び出し側が指定する。
void writeSvgEllipse(std::ostream &output, const Shape &shape);

/// 円・楕円の元輪郭をSVGの2本の円弧として出力する。
void writeSvgCurveContour(std::ostream &output, const Shape &shape);

/// SVGの固定キャンバス。幅・高さは図面の入力単位、原点は(0,0)。
struct Canvas
{
    double width;
    double height;
};

/// 図形一覧と図面全体の単位・DPI。実行ごとに生成し、グローバル状態は持たない。
struct Drawing
{
    std::vector<Shape> shapes;
    std::string unit = "mm";
    double dpi = 72;
    std::optional<Canvas> canvas;
    bool fusion = false;       ///< DXFをFusion向けの輪郭だけで出力する。
    unsigned combineDepth = 0; ///< JSコールバックの過剰なネストを防ぐ実行中カウンター。
    uint32_t pathClassId = 0;  ///< この実行のQuickJSランタイムで発行したパス値のクラスID。
};

/// 図面をSVGまたはAutoCAD 2004 ASCII DXFへ変換する。unitはCLI優先順位を解決済みの単位。
/// SVGは入力座標を保持し、DXFはmmへ換算する。入力の図面は変更しない。
std::string render(Drawing drawing, bool svg, const std::string &unit, bool fusion = false);

/// mmへ換算済みの図面をDXFへ変換する。円・閉じた輪郭の塗りはHATCHを使用する。
std::string renderDxf(const Drawing &drawing, bool fusion = false);

/// SVG用にBooleanGroupをClipper2で解決する。入力の単位からmmで演算し、元の単位へ戻す。
Drawing resolveSvgPaths(Drawing drawing, double mmPerUnit);

/// DXF用にBooleanGroupを元の図形へ展開する。論理演算とグループのスタイルは適用しない。
Drawing expandOriginalShapes(Drawing drawing);

/// 元図形を保持し、SVGのマスクで合成する。範囲はSVGのviewBox、IDは図面内で一意にする。
void writeSvgComposite(std::ostream &output, const Shape &shape, double mmPerUnit, double x,
                       double y, double width, double height, unsigned &nextId);

} // namespace zfj
