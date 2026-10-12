// offsetは元パスを変えず、新しい閉じたパスを返す。
drawing.setUnit("mm");
drawing.setCanvas(190, 150);
["butt", "square", "round"].forEach(function(cap, index) {
    var y = 20 + index * 25;
    var line = path.line(20, y, 75, y);
    drawing.draw(path.offset(line, 6, { cap: cap }), {
        stroke: "#003366", fill: "#66aaff", strokeWidth: 0.4
    });
    drawing.draw(line, { stroke: "#cc3300", strokeWidth: 0.5 });
});
var box = path.rect(115, 25, 45, 40);
drawing.draw(path.offset(box, 7), { fill: "#cceeff" });
drawing.draw(box, { stroke: "#cc3300" });
drawing.draw(path.offset(box, -7), { fill: "#66aaff" });
var ring = path.combine("xor", path.rect(20, 105, 55, 30), path.rect(35, 113, 25, 14));
drawing.draw(path.offset(ring, 3), { fill: "#ffcc66" });
var arc = path.circle(135, 110, { radiusX: 25, radiusY: 15, start: 0, end: 180 });
drawing.draw(path.offset(arc, 4, { cap: "round" }), { fill: "#ffcc66" });
