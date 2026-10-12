// 円・楕円・開いた円弧。角度は0が右、90が画面上の下。既定は時計回り。
drawing.setUnit("mm");
drawing.setCanvas(180, 120);
drawing.circle(35, 30, 20, { fill: "#66aaff" });
drawing.circle(115, 30, { radiusX: 40, radiusY: 20 }, { fill: "#ffcc66" });
drawing.circle(35, 85, { radius: 20, start: 0, end: 270 }, {
    stroke: "#0066cc", strokeWidth: 1
});
var arc = path.circle(115, 85, {
    radiusX: 40, radiusY: 20, start: 0, end: 270, clockwise: false
});
drawing.draw(arc, { stroke: "#cc6600", strokeWidth: 1 });
