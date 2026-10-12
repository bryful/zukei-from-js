// OR：元の長方形と円を編集可能な描画オブジェクトとして保持する。
drawing.setUnit("mm");
drawing.setCanvas(120, 100);
var a = path.rect(10, 10, 70, 70);
var b = path.circle(75, 55, 30);
var v = path.svgcombine("or", a, b);
drawing.draw(v, { stroke: "#003366", strokeWidth: 0.5, fill: "#66aaff" });
