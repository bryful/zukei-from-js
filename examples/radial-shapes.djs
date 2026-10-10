// 正多角形・星型。半径は現在の入力単位。mmの場合の既定値は30/15mm。
drawing.setUnit("mm");
drawing.setCanvas(260, 160);

// 頂点までの半径30、3辺、回転0。
drawing.draw(path.regularPolygon({ center: [40, 45] }), {
    stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5
});

// 辺中心までの半径25、6辺。上辺が水平になる。
drawing.draw(path.regularPolygon({ center: [125, 45], radius: 25, radiusMode: "edge", sides: 6 }), {
    stroke: "#006633", fill: "#66cc99", strokeWidth: 0.5
});

// 頂点までの半径30、8辺、22.5度回転。
drawing.draw(path.regularPolygon({ center: [215, 45], sides: 8, rot: 22.5 }), {
    stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5
});

// 外半径30、内半径15、外側5頂点。
drawing.draw(path.star({ center: [40, 115] }), {
    stroke: "#663366", fill: "#cc99cc", strokeWidth: 0.5
});

// 内半径を小さくして尖った7頂点の星型にする。
drawing.draw(path.star({ center: [125, 115], innerRadius: 10, points: 7, rot: 15 }), {
    stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5
});

// 生成したパスもXORや移動・回転に利用できる。
var a = path.regularPolygon({ center: [205, 115], radius: 25, sides: 6 });
var b = path.star({ center: [220, 115], outerRadius: 25, innerRadius: 12 });
drawing.draw(path.svgcombine("xor", a, b), {
    stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5
});
