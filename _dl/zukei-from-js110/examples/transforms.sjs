// path.transformは元パスを変更し、path.transformedは独立した新しいパスを作る。
drawing.setUnit("mm");
drawing.setCanvas(180, 105);

var a = path.rect(10, 10, 30, 20);
drawing.draw(a, { stroke: "#777777", fill: "#eeeeee", strokeWidth: 0.5 });

// コピーを移動・回転・拡縮。aは変更しない。
var b = path.transformed(a, {
    pos: { x: 65, y: 5 },
    rot: 25,
    scale: { x: 150, y: 100 },
    anchor: "center"
});
drawing.draw(b, { stroke: "#003366", fill: "#66aaff", strokeWidth: 0.5 });

// 元パスを変更。先にdrawした図形は変更されない。
path.transform(a, { pos: [120, 5] });
drawing.draw(a, { stroke: "#006633", fill: "#66cc99", strokeWidth: 0.5 });

// 円は等倍率なら円、縦横別倍率なら楕円として保持する。
var circle = path.circle(25, 70, 12);
drawing.draw(circle, { stroke: "#777777", fill: "#eeeeee", strokeWidth: 0.5 });
var ellipse = path.transformed(circle, {
    pos: [60, 0], scale: [180, 70], rot: -20, anchor: [25, 70]
});
drawing.draw(ellipse, { stroke: "#663300", fill: "#ffcc66", strokeWidth: 0.5 });

// 合成パス全体も共通アンカーで変換できる。
var r = path.rect(0, 0, 25, 25);
var c = path.circle(25, 15, 12);
var v = path.svgcombine("xor", r, c);
path.transform(v, { pos: [130, 58], rot: 15, anchor: "topLeft" });
drawing.draw(v, { stroke: "#663366", fill: "#cc99cc", strokeWidth: 0.5 });
