// 元の円・長方形を複合パスにし、偶奇塗りと共通の線幅で描画する。
drawing.setUnit("mm");
drawing.setCanvas(120, 100);
var a = path.rect(10, 10, 70, 70);
var b = path.circle(75, 55, 30);
var v = path.svgcombine("xor", a, b);
drawing.draw(v, { stroke: "#003366", strokeWidth: 0.5, fill: "#66aaff" });
