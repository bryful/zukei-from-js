// AND：元の長方形を描画し、円のマスクで共通部分を表示する。
drawing.setUnit("mm");
drawing.setCanvas(120, 100);
var a = path.rect(10, 10, 70, 70);
var b = path.circle(75, 55, 30);
var v = path.svgcombine("and", a, b);
drawing.draw(v, { stroke: "#006633", strokeWidth: 0.5, fill: "#66cc99" });
