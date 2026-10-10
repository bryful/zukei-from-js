#include "drawing.h"
#include <cmath>
#include <stdexcept>

namespace zfj
{
namespace
{
/// 頂点を追加する前に、既存の座標制限と同じ範囲を検証する。
void addVertex(Shape &shape, Point center, double radius, double angle)
{
    const Point p{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 1e9 || std::abs(p.y) > 1e9)
        throw std::runtime_error("Generated coordinates exceed +/-1e9");
    shape.points.push_back(p);
}
} // namespace

/// 頂点半径または辺中心半径から、上向きの正多角形を作る。rotは度数。
Shape makeRegularPolygon(Point center, double radius, unsigned sides, bool edgeRadius, double rot)
{
    if (sides < 3 || sides > 100000)
        throw std::runtime_error("sides must be an integer from 3 to 100000");
    if (!std::isfinite(radius) || radius <= 0)
        throw std::runtime_error("radius must be positive");
    const double pi = std::acos(-1.0);
    const double vertexRadius = edgeRadius ? radius / std::cos(pi / sides) : radius;
    const double start =
        -pi / 2 + std::remainder(rot, 360.0) * pi / 180 - (edgeRadius ? pi / sides : 0);
    Shape shape{ShapeKind::Polygon, {}};
    shape.simpleContour = true;
    shape.points.reserve(sides);
    for (unsigned i = 0; i < sides; ++i)
        addVertex(shape, center, vertexRadius, start + 2 * pi * static_cast<double>(i) / sides);
    return shape;
}

/// 外半径・内半径を交互に配置した単純な星型を作る。pointsは外側の頂点数。
Shape makeStar(Point center, double outerRadius, double innerRadius, unsigned points, double rot)
{
    if (points < 3 || points > 50000)
        throw std::runtime_error("points must be an integer from 3 to 50000");
    if (!std::isfinite(outerRadius) || !std::isfinite(innerRadius) || innerRadius <= 0 ||
        outerRadius <= innerRadius)
        throw std::runtime_error("Star radii must satisfy 0 < innerRadius < outerRadius");
    const double pi = std::acos(-1.0);
    const double start = -pi / 2 + std::remainder(rot, 360.0) * pi / 180;
    Shape shape{ShapeKind::Polygon, {}};
    shape.simpleContour = true;
    shape.points.reserve(points * 2);
    for (unsigned i = 0; i < points * 2; ++i)
        addVertex(shape, center, i % 2 == 0 ? outerRadius : innerRadius,
                  start + pi * static_cast<double>(i) / points);
    return shape;
}
} // namespace zfj
