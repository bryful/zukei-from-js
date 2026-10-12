drawing.setUnit("mm");
drawing.setDxfMode("fusion");
drawing.rect(0, 0, 100, 60);
// 塗りだけの指定も、Fusion向けモードでは円の輪郭として出力されます。
drawing.circle(50, 30, 15, { stroke: null, fill: "#ff0000" });
