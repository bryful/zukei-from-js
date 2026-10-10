// DXF：移動・回転・拡縮。等倍率の円はCIRCLE、縦横別倍率はELLIPSE。
drawing.setUnit("mm");
var a = path.rect(10, 10, 30, 20);
drawing.draw(a);
var b = path.transformed(a, { pos: [65, 5], rot: 25, scale: [150, 100] });
drawing.draw(b);
path.transform(a, { pos: [120, 5] });
drawing.draw(a);

var c = path.circle(25, 70, 12);
drawing.draw(c);
drawing.draw(path.transformed(c, { pos: [60, 0], scale: [180, 70], rot: -20 }));

// 合成はDXFでは無効。元図形の両方に変換が適用される。
var v = path.svgcombine("xor", path.rect(0, 0, 25, 25), path.circle(25, 15, 12));
path.transform(v, { pos: [130, 58], rot: 15, anchor: "topLeft" });
drawing.draw(v);
