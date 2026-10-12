drawing.setUnit("mm");
drawing.line(0, 0, 100, 20);
drawing.rect(10, 30, 40, 25);
drawing.circle(75, 45, 15);
drawing.polygon([[10, 80], [40, 65], [55, 90], [20, 100]]);
// JavaScript expressions, functions and loops are available.
for (let i = 0; i < 3; ++i) {
    drawing.circle(70 + i * 12, 85, 4);
}

