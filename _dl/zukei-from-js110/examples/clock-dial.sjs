// Clock dial, 200 x 200 mm. SVG coordinates: down is positive Y.
// ZFJ accepts .sjs: use the identical clock-dial.sjs to export.
// 60 seconds per revolution, with five subdivisions per second (0.2 s).
drawing.setUnit("mm");
drawing.setCanvas(200, 200);

var cx = 100;
var cy = 100;
var ink = "#202C3A";
var subdivisions = 5; // Change to 10 for 0.1-second divisions.

function polar(radius, angle) {
    var radians = (angle - 90) * Math.PI / 180;
    return [cx + radius * Math.cos(radians), cy + radius * Math.sin(radians)];
}

function tick(angle, inner, outer, width, color) {
    var a = polar(inner, angle);
    var b = polar(outer, angle);
    drawing.line(a[0], a[1], b[0], b[1], {
        stroke: color, strokeWidth: width
    });
}

drawing.circle(cx, cy, 97, { stroke: ink, strokeWidth: 0.7, fill: "#FFFFFF" });
drawing.circle(cx, cy, 94, { stroke: ink, strokeWidth: 0.25 });

// Major marks every 5 seconds, medium marks every second, fine marks every 0.2 s.
for (var i = 0; i < 60 * subdivisions; i++) {
    var major = i % (5 * subdivisions) === 0;
    var second = i % subdivisions === 0;
    tick(i * 360 / (60 * subdivisions), major ? 82 : second ? 86 : 89,
        92, major ? 1.0 : second ? 0.5 : 0.2, major || second ? ink : "#64748B");
}

// Vector numerals: no external font or text API is required.
var segments = [
    [[0, 0], [4, 0]], [[4, 0], [4, 5]], [[4, 5], [4, 10]],
    [[0, 10], [4, 10]], [[0, 5], [0, 10]], [[0, 0], [0, 5]],
    [[0, 5], [4, 5]]
];
var digits = ["012345", "12", "01643", "01236", "5612",
              "05623", "056432", "012", "0123456", "012356"];

function number(value, x, y, scale, color, width) {
    var text = String(value);
    var totalWidth = (text.length * 6 - 2) * scale;
    for (var d = 0; d < text.length; d++) {
        var active = digits[Number(text[d])];
        for (var s = 0; s < active.length; s++) {
            var ends = segments[Number(active[s])];
            var left = x - totalWidth / 2 + d * 6 * scale;
            var top = y - 5 * scale;
            drawing.line(left + ends[0][0] * scale, top + ends[0][1] * scale,
                         left + ends[1][0] * scale, top + ends[1][1] * scale,
                         { stroke: color, strokeWidth: width });
        }
    }
}

for (var hour = 1; hour <= 12; hour++) {
    var position = polar(72, hour * 30);
    number(hour, position[0], position[1], 1.05, ink, 0.7);
}

// Inner seconds labels: 00 at the top, then 05 ... 55 clockwise.
for (var seconds = 0; seconds < 60; seconds += 5) {
    var labelPosition = polar(56, seconds * 6);
    var label = seconds < 10 ? "0" + seconds : String(seconds);
    number(label, labelPosition[0], labelPosition[1], 0.5, "#64748B", 0.3);
}

// Center registration mark; no hands so the dial can be reused as a template.
drawing.circle(cx, cy, 1.5, { stroke: ink, strokeWidth: 0.3 });
drawing.line(cx - 3, cy, cx + 3, cy, { stroke: ink, strokeWidth: 0.2 });
drawing.line(cx, cy - 3, cx, cy + 3, { stroke: ink, strokeWidth: 0.2 });
