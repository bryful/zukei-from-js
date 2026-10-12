var a = path.rect(0, 0, 100, 100);
var b = path.rect(25, 25, 75, 75);
var v = path.svgcombine("xor", a, b);
drawing.draw(v, { stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5 });
