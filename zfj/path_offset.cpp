#include "clipper2/clipper.h"
#include "drawing.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace zfj
{
namespace
{
using namespace Clipper2Lib;
constexpr double precisionScale = 10000; ///< 0.0001mm格子へ換算する倍率。
constexpr size_t vertexLimit = 100000;   ///< 入出力全体の頂点数制限。
constexpr size_t workLimit = 2000000;    ///< 交差・包含検証の候補数制限。

/// 線分の端点と輪郭内の番号。X方向に並べて交差候補を絞る。
struct Segment
{
    Point64 a, b;
    size_t contour, index, count;
    int64_t minX, maxX, minY, maxY;
};

/// 点が端点を含む線分上にあるかを整数演算で判定する。
bool onSegment(const Point64 &p, const Point64 &a, const Point64 &b)
{
    return CrossProductSign(a, b, p) == 0 && p.x >= std::min(a.x, b.x) &&
           p.x <= std::max(a.x, b.x) && p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}

/// 接触・重複を含む交差。隣接辺の正常な共有端点だけは呼出元で許可する。
bool intersects(const Segment &a, const Segment &b)
{
    const int ab1 = CrossProductSign(a.a, a.b, b.a), ab2 = CrossProductSign(a.a, a.b, b.b);
    const int ba1 = CrossProductSign(b.a, b.b, a.a), ba2 = CrossProductSign(b.a, b.b, a.b);
    return (ab1 * ab2 < 0 && ba1 * ba2 < 0) || onSegment(a.a, b.a, b.b) ||
           onSegment(a.b, b.a, b.b) || onSegment(b.a, a.a, a.b) || onSegment(b.b, a.a, a.b);
}

/// 重複点を整理し、退化・自己交差・輪郭同士の接触を検出する。
void validate(Paths64 &paths, bool closed)
{
    if (paths.empty())
        throw std::runtime_error("offset: empty path");
    size_t total = 0;
    std::vector<Segment> edges;
    for (size_t k = 0; k < paths.size(); ++k)
    {
        auto &p = paths[k];
        StripDuplicates(p, closed);
        total += p.size();
        if (total > vertexLimit)
            throw std::runtime_error("offset: too many vertices");
        if (p.size() < (closed ? 3u : 2u) || (closed && Area(p) == 0))
            throw std::runtime_error("offset: degenerate contour");
        const size_t count = closed ? p.size() : p.size() - 1;
        for (size_t i = 0; i < count; ++i)
        {
            const auto a = p[i], b = p[(i + 1) % p.size()];
            edges.push_back({a, b, k, i, count, std::min(a.x, b.x), std::max(a.x, b.x),
                             std::min(a.y, b.y), std::max(a.y, b.y)});
        }
    }
    std::sort(edges.begin(), edges.end(),
              [](const auto &a, const auto &b) { return a.minX < b.minX; });
    size_t checks = 0;
    for (size_t i = 0; i < edges.size(); ++i)
        for (size_t j = i + 1; j < edges.size() && edges[j].minX <= edges[i].maxX; ++j)
        {
            if (++checks > workLimit)
                throw std::runtime_error("offset: intersection validation limit exceeded");
            const auto &a = edges[i], &b = edges[j];
            if (a.maxY < b.minY || b.maxY < a.minY)
                continue;
            const size_t difference = a.index > b.index ? a.index - b.index : b.index - a.index;
            const bool adjacent = a.contour == b.contour &&
                                  (difference == 1 || (closed && difference + 1 == a.count));
            if (adjacent)
            {
                const auto shared = a.a == b.a || a.a == b.b ? a.a : a.b;
                const auto otherA = a.a == shared ? a.b : a.a;
                const auto otherB = b.a == shared ? b.b : b.a;
                if (onSegment(otherA, shared, otherB) || onSegment(otherB, shared, otherA))
                    throw std::runtime_error("offset: overlapping adjacent segments");
            }
            else if (intersects(a, b))
                throw std::runtime_error("offset: self-intersection or touching contours");
        }
}

/// 包含深さを求め、外周を正、穴を負に正規化する。鏡面反転した入力にも対応する。
std::vector<size_t> depths(Paths64 &paths, bool normalize)
{
    std::vector<size_t> result(paths.size());
    size_t checks = 0;
    for (size_t i = 0; i < paths.size(); ++i)
        for (size_t j = 0; j < paths.size(); ++j)
        {
            if (i == j)
                continue;
            checks += paths[j].size();
            if (checks > workLimit)
                throw std::runtime_error("offset: containment validation limit exceeded");
            const auto location = PointInPolygon(paths[i][0], paths[j]);
            if (location == PointInPolygonResult::IsOn)
                throw std::runtime_error("offset: touching contours");
            if (location == PointInPolygonResult::IsInside)
                ++result[i];
        }
    if (normalize)
        for (size_t i = 0; i < paths.size(); ++i)
            if ((Area(paths[i]) > 0) != (result[i] % 2 == 0))
                std::reverse(paths[i].begin(), paths[i].end());
    return result;
}
} // namespace

Shape offsetPath(const Shape &source, double distance, const std::string &cap,
                 const std::string &join, double mmPerUnit)
{
    using namespace Clipper2Lib;
    if (!std::isfinite(distance) || std::abs(distance) > 1e9)
        throw std::runtime_error("offset: distance must be finite and within +/-1e9");
    const EndType end = cap == "butt"     ? EndType::Butt
                        : cap == "square" ? EndType::Square
                                          : EndType::Round;
    if (cap != "butt" && cap != "square" && cap != "round")
        throw std::runtime_error("offset: cap must be butt, square or round");
    const JoinType joint = join == "miter"   ? JoinType::Miter
                           : join == "round" ? JoinType::Round
                                             : JoinType::Bevel;
    if (join != "miter" && join != "round" && join != "bevel")
        throw std::runtime_error("offset: join must be miter, round or bevel");
    if (source.kind == ShapeKind::SvgComposite)
        throw std::runtime_error(
            "offset: SVG display composites are not geometry; use path.combine");
    const bool open =
        source.arc || source.kind == ShapeKind::Line || source.kind == ShapeKind::Polyline;
    if (open && distance <= 0)
        throw std::runtime_error("offset: open paths require a positive distance");
    // 円は解析的に処理し、編集可能な円を保持する。
    if (source.kind == ShapeKind::Circle && !source.arc)
    {
        Shape result = source;
        result.radius += distance;
        if (result.radius <= 0 || result.radius * mmPerUnit < 1 / precisionScale)
            throw std::runtime_error("offset: circle collapsed");
        movePath(result, {0, 0}); // 出力座標の上限も検証する。
        return result;
    }
    Shape geometry = source;
    if (source.kind == ShapeKind::BooleanGroup)
    {
        Drawing temporary;
        temporary.shapes.push_back(source);
        geometry = resolveSvgPaths(std::move(temporary), mmPerUnit).shapes.front();
    }
    PathsD input;
    if (geometry.kind == ShapeKind::Path)
    {
        for (const auto &contour : geometry.paths)
        {
            PathD p;
            for (const auto v : contour)
                p.push_back({v.x * mmPerUnit, v.y * mmPerUnit});
            input.push_back(std::move(p));
        }
    }
    else
    {
        const auto points =
            geometry.kind == ShapeKind::Circle || geometry.kind == ShapeKind::Ellipse
                ? sampleCurve(geometry, mmPerUnit)
                : geometry.points;
        PathD p;
        for (const auto v : points)
            p.push_back({v.x * mmPerUnit, v.y * mmPerUnit});
        input.push_back(std::move(p));
    }
    int error = 0;
    auto paths = ScalePaths<int64_t, double>(input, precisionScale, error);
    if (error)
        throw std::runtime_error("offset: coordinate conversion failed");
    validate(paths, !open);
    auto before = open ? std::vector<size_t>{0} : depths(paths, true);
    Paths64 output;
    const double delta = distance * mmPerUnit * precisionScale;
    if (distance != 0 && std::abs(delta) < 1)
        throw std::runtime_error("offset: distance is below 0.0001mm precision");
    if (distance == 0)
        output = paths;
    else
    {
        // 丸端・丸角で過大な数の頂点を確保する前に上限を調べる。
        if (joint == JoinType::Round || (open && end == EndType::Round))
        {
            const double angle =
                std::acos(std::clamp(1 - 0.01 / std::abs(distance * mmPerUnit), -1.0, 1.0));
            const double steps = angle > 0 ? std::ceil(std::acos(-1.0) / angle) : workLimit + 1;
            size_t vertices = 0;
            for (const auto &p : paths)
                vertices += p.size();
            if (steps * (joint == JoinType::Round ? vertices : 2) > workLimit)
                throw std::runtime_error("offset: rounded geometry exceeds work limit");
        }
        ClipperOffset engine(2.0, 0.01 * precisionScale);
        engine.AddPaths(paths, joint, open ? end : EndType::Polygon);
        engine.Execute(delta, output);
        if (engine.ErrorCode())
            throw std::runtime_error("offset: Clipper2 offset failed");
    }
    if (output.empty())
        throw std::runtime_error("offset: path collapsed");
    validate(output, true);
    const auto after = depths(output, true);
    auto beforeSignature = before, afterSignature = after;
    std::sort(beforeSignature.begin(), beforeSignature.end());
    std::sort(afterSignature.begin(), afterSignature.end());
    if (afterSignature != beforeSignature)
        throw std::runtime_error("offset: contour topology changed (collapse, split or merge)");
    if (!open && distance != 0)
    {
        // 消滅と分裂が同時に起きて輪郭数が偶然一致しても、元輪郭との一対一対応を要求する。
        std::vector<bool> matched(output.size());
        size_t checks = 0;
        for (size_t i = 0; i < paths.size(); ++i)
        {
            size_t match = output.size();
            for (size_t j = 0; j < output.size(); ++j)
            {
                if (before[i] != after[j])
                    continue;
                const bool resultContainsInput = (distance > 0) == (before[i] % 2 == 0);
                const auto &container = resultContainsInput ? output[j] : paths[i];
                const auto point = resultContainsInput ? paths[i][0] : output[j][0];
                checks += container.size();
                if (checks > workLimit)
                    throw std::runtime_error("offset: correspondence validation limit exceeded");
                if (PointInPolygon(point, container) != PointInPolygonResult::IsOutside)
                {
                    if (match != output.size() || matched[j])
                        throw std::runtime_error("offset: contour split or merge");
                    match = j;
                }
            }
            if (match == output.size())
                throw std::runtime_error("offset: contour collapsed or changed containment");
            matched[match] = true;
        }
    }
    Shape result{ShapeKind::Path, {}};
    result.style = source.style;
    for (const auto &p : output)
    {
        std::vector<Point> contour;
        for (const auto v : p)
            contour.push_back({v.x / precisionScale / mmPerUnit, v.y / precisionScale / mmPerUnit});
        result.paths.push_back(std::move(contour));
    }
    movePath(result, {0, 0});
    return result;
}
} // namespace zfj
