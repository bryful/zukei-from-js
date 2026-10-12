// Clipper2の輪郭計算。同じ図形を分割した計算済みpathとして保存する。
drawing.setUnit("mm");
drawing.setCanvas(120, 100);
var a = path.rect(10, 10, 70, 70);
var b = path.circle(75, 55, 30);
var v = path.combine("xor", a, b);
drawing.draw(v, { stroke:"#000000", fill: "#66aaff" });
