#include "drawing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace zfj
{
void Bounds::include(Point point)
{
    if (!valid)
    {
        minX = maxX = point.x;
        minY = maxY = point.y;
        valid = true;
    }
    minX = std::min(minX, point.x);
    minY = std::min(minY, point.y);
    maxX = std::max(maxX, point.x);
    maxY = std::max(maxY, point.y);
}

/// 円・楕円の正確な外接矩形と、合成の元図形を再帰的に集計する。
Bounds pathBounds(const Shape &shape)
{
    Bounds bounds;
    if (shape.kind == ShapeKind::Circle || shape.kind == ShapeKind::Ellipse)
    {
        const double a = shape.radius;
        const double b = shape.kind == ShapeKind::Circle ? a : shape.minorRadius;
        const double angle = shape.kind == ShapeKind::Circle ? 0 : shape.angle;
        const double x = std::hypot(a * std::cos(angle), b * std::sin(angle));
        const double y = std::hypot(a * std::sin(angle), b * std::cos(angle));
        bounds.include({shape.points[0].x - x, shape.points[0].y - y});
        bounds.include({shape.points[0].x + x, shape.points[0].y + y});
    }
    else
        for (const auto point : shape.points)
            bounds.include(point);
    for (const auto &contour : shape.paths)
        for (const auto point : contour)
            bounds.include(point);
    for (const auto &child : shape.children)
    {
        const auto b = pathBounds(child);
        if (b.valid)
        {
            bounds.include({b.minX, b.minY});
            bounds.include({b.maxX, b.maxY});
        }
    }
    return bounds;
}

/// 元の幾何データへ変換を適用する。円・楕円は行列の特異値から正確な軸を復元する。
void transformPath(Shape &shape, Point pos, double degrees, Point scale, Point anchor)
{
    const double pi = std::acos(-1.0);
    const double theta = std::remainder(degrees, 360.0) * pi / 180.0;
    const double c = std::cos(theta), s = std::sin(theta);
    auto vector = [&](Point v) -> Point
    { return {c * scale.x * v.x - s * scale.y * v.y, s * scale.x * v.x + c * scale.y * v.y}; };
    auto point = [&](Point p) -> Point
    {
        const auto v = vector({p.x - anchor.x, p.y - anchor.y});
        return {v.x + anchor.x + pos.x, v.y + anchor.y + pos.y};
    };
    if (shape.kind == ShapeKind::Circle || shape.kind == ShapeKind::Ellipse)
    {
        const double angle = shape.kind == ShapeKind::Circle ? 0 : shape.angle;
        const double a = shape.radius;
        const double b = shape.kind == ShapeKind::Circle ? a : shape.minorRadius;
        const Point u = vector({a * std::cos(angle), a * std::sin(angle)});
        const Point v = vector({-b * std::sin(angle), b * std::cos(angle)});
        const double xx = u.x * u.x + v.x * v.x;
        const double yy = u.y * u.y + v.y * v.y;
        const double xy = u.x * u.y + v.x * v.y;
        const double major = std::sqrt((xx + yy + std::hypot(xx - yy, 2 * xy)) / 2);
        const double minor = std::abs(u.x * v.y - u.y * v.x) / major;
        if (!std::isfinite(major) || !std::isfinite(minor) || major <= 0 || minor <= 0)
            throw std::runtime_error("Transform produces a degenerate circle or ellipse");
        shape.radius = major;
        shape.minorRadius = minor;
        shape.angle = std::atan2(2 * xy, xx - yy) / 2;
        shape.kind =
            std::abs(major - minor) <= major * 1e-12 ? ShapeKind::Circle : ShapeKind::Ellipse;
    }
    for (auto &p : shape.points)
        p = point(p);
    for (auto &contour : shape.paths)
        for (auto &p : contour)
            p = point(p);
    if (shape.kind == ShapeKind::Rectangle)
    {
        const auto b = pathBounds(shape);
        // 軸平行の長方形はrectのまま残し、反転時も正の幅・高さに正規化する。
        const double tolerance = 1e-12 * std::max({1.0, b.maxX - b.minX, b.maxY - b.minY});
        bool aligned = true;
        for (size_t i = 0; i < shape.points.size(); ++i)
        {
            const auto p = shape.points[i], q = shape.points[(i + 1) % shape.points.size()];
            if (std::abs(p.x - q.x) > tolerance && std::abs(p.y - q.y) > tolerance)
                aligned = false;
        }
        if (aligned)
            shape.points = {{b.minX, b.minY}, {b.maxX, b.minY}, {b.maxX, b.maxY}, {b.minX, b.maxY}};
        else
        {
            shape.kind = ShapeKind::Polygon;
            shape.simpleContour = true;
        }
    }
    for (auto &child : shape.children)
        transformPath(child, pos, degrees, scale, anchor);
    const auto bounds = pathBounds(shape);
    if (bounds.valid && (!std::isfinite(bounds.minX) || !std::isfinite(bounds.minY) ||
                         !std::isfinite(bounds.maxX) || !std::isfinite(bounds.maxY) ||
                         std::max({std::abs(bounds.minX), std::abs(bounds.minY),
                                   std::abs(bounds.maxX), std::abs(bounds.maxY)}) > 1e9))
        throw std::runtime_error("Transformed coordinates exceed +/-1e9");
}

/// 移動専用処理。円・楕円の軸や長方形の種類を再計算せずに保持する。
void movePath(Shape &shape, Point offset)
{
    for (auto &point : shape.points)
    {
        point.x += offset.x;
        point.y += offset.y;
    }
    for (auto &contour : shape.paths)
        for (auto &point : contour)
        {
            point.x += offset.x;
            point.y += offset.y;
        }
    for (auto &child : shape.children)
        movePath(child, offset);
    const auto bounds = pathBounds(shape);
    if (bounds.valid && (!std::isfinite(bounds.minX) || !std::isfinite(bounds.minY) ||
                         !std::isfinite(bounds.maxX) || !std::isfinite(bounds.maxY) ||
                         std::max({std::abs(bounds.minX), std::abs(bounds.minY),
                                   std::abs(bounds.maxX), std::abs(bounds.maxY)}) > 1e9))
        throw std::runtime_error("Moved coordinates exceed +/-1e9");
}

Point curvePoint(const Shape &shape, double parameter)
{
    const double angle = shape.kind == ShapeKind::Circle ? 0 : shape.angle;
    const double b = shape.kind == ShapeKind::Circle ? shape.radius : shape.minorRadius;
    const double x = shape.radius * std::cos(parameter), y = b * std::sin(parameter);
    return {shape.points[0].x + x * std::cos(angle) - y * std::sin(angle),
            shape.points[0].y + x * std::sin(angle) + y * std::cos(angle)};
}

std::vector<Point> sampleCurve(const Shape &shape, double mmPerUnit)
{
    const double pi = std::acos(-1.0);
    const double halfAngle =
        std::acos(std::clamp(1.0 - 0.01 / (shape.radius * mmPerUnit), -1.0, 1.0));
    const double requested = halfAngle > 0 ? std::ceil(pi / halfAngle) : 100001;
    if (requested > 100000)
        throw std::runtime_error("Curve needs too many vertices for 0.01mm path tolerance");
    const size_t count = static_cast<size_t>(std::max(12.0, requested));
    std::vector<Point> points;
    points.reserve(count);
    for (size_t i = 0; i < count; ++i)
        points.push_back(curvePoint(shape, 2 * pi * static_cast<double>(i) / count));
    return points;
}

void writeSvgEllipse(std::ostream &out, const Shape &shape)
{
    const auto p = shape.points[0];
    out << "<ellipse cx=\"" << p.x << "\" cy=\"" << p.y << "\" rx=\"" << shape.radius << "\" ry=\""
        << shape.minorRadius << "\" transform=\"rotate(" << shape.angle * 180 / std::acos(-1.0)
        << ' ' << p.x << ' ' << p.y << ")\"";
}

void writeSvgCurveContour(std::ostream &out, const Shape &shape)
{
    const double b = shape.kind == ShapeKind::Circle ? shape.radius : shape.minorRadius;
    const double angle = shape.kind == ShapeKind::Circle ? 0 : shape.angle * 180 / std::acos(-1.0);
    const auto start = curvePoint(shape, 0), middle = curvePoint(shape, std::acos(-1.0));
    out << 'M' << start.x << ' ' << start.y << 'A' << shape.radius << ' ' << b << ' ' << angle
        << " 0 1 " << middle.x << ' ' << middle.y << 'A' << shape.radius << ' ' << b << ' ' << angle
        << " 0 1 " << start.x << ' ' << start.y << 'Z';
}
} // namespace zfj
