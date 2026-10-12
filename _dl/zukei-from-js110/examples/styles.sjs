drawing.setUnit("mm");
drawing.line(0, 0, 100, 0, { stroke: "#0066cc", strokeWidth: 0.5 });
drawing.rect(5, 10, 40, 25, { stroke: "#cc0000", strokeWidth: 1, fill: "#ffcc00" });
drawing.circle(70, 25, 15, { stroke: null, fill: "#00aa66" });
drawing.polygon([[10, 50], [45, 45], [30, 75]], {
    stroke: "#663399", strokeWidth: 0.7, fill: "#ccccff"
});
