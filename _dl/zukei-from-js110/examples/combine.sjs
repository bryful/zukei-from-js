drawing.setUnit("mm");
drawing.setCanvas(160, 55);

var a = path.rect(5, 5, 30, 30);
var b = path.rect(20, 15, 30, 30);
drawing.draw(path.svgcombine("or", a, b), {
    stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5
});

var c = path.rect(60, 5, 30, 30);
var d = path.rect(75, 15, 30, 30);
drawing.draw(path.svgcombine("and", c, d), {
    stroke: "#006633", fill: "#66cc99", strokeWidth: 0.5
});

var e = path.rect(110, 5, 30, 30);
var f = path.rect(125, 15, 30, 30);
drawing.draw(path.svgcombine("xor", e, f), {
    stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5
});
