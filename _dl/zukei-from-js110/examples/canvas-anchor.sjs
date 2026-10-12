// SVGキャンバスは左上(0,0)、右下(200,100)。中心は(100,50)。
drawing.setUnit("mm");
drawing.setCanvas(200, 100);

var a = path.circle(30, 25, 10);
drawing.draw(a, { stroke: "#777777", fill: "#eeeeee", strokeWidth: 0.5 });

// キャンバス中心を基準に180度回転すると中心は(170,75)になる。
var b = path.transformed(a, { rot: 180, anchor: "canvasCenter" });
drawing.draw(b, { stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5 });

// キャンバス中心の目印。
drawing.line(95, 50, 105, 50, { stroke: "#cc0000", strokeWidth: 0.5 });
drawing.line(100, 45, 100, 55, { stroke: "#cc0000", strokeWidth: 0.5 });
