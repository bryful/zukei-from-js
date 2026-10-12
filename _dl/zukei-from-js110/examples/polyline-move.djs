drawing.setUnit("mm");
drawing.setCanvas(180, 110);

// 開いた折れ線。最後から最初へは接続しない。
drawing.polyline([[10, 10], [45, 10], [45, 40]], {
    stroke: "#003366", strokeWidth: 1
});

var p = path.polyline([[10, 60], [45, 60], [45, 90]], {
    stroke: "#006633", strokeWidth: 1
});
drawing.draw(p);

// 元パスはそのままに、移動したコピーを作る。
var q = p.moved(55, 0);
drawing.draw(q);

// 元パスを変更。draw済みの図形は変更しない。
p.move(110, 0);
drawing.draw(p);

// 共通APIも使用可能。
var star = path.star({ center: [85, 25], outerRadius: 15, innerRadius: 7 });
drawing.draw(path.moved(star, 50, 0), { stroke: "#663300", fill: "#ffcc66" });
