import * as vscode from 'vscode';

interface ApiMethod {
  name: string;
  signature: string;
  snippet: string;
  description: string;
}

// zfj/drawing_api.cppに登録されている関数と対応させます。
export const drawingMethods: readonly ApiMethod[] = [
  { name: 'line', signature: 'line(x1, y1, x2, y2, style?)', snippet: 'line(${1:0}, ${2:0}, ${3:100}, ${4:0})', description: '2点を結ぶ線を描きます。styleのfillに色は指定できません。' },
  { name: 'rect', signature: 'rect(x, y, width, height, style?)', snippet: 'rect(${1:0}, ${2:0}, ${3:100}, ${4:60})', description: '四角形を描きます。幅と高さは正数です。' },
  { name: 'circle', signature: 'circle(cx, cy, radius, style?)', snippet: 'circle(${1:50}, ${2:30}, ${3:15})', description: '円を描きます。半径は正数です。' },
  { name: 'polygon', signature: 'polygon(points, style?)', snippet: 'polygon([[${1:0}, ${2:0}], [${3:100}, ${4:0}], [${5:50}, ${6:60}]])', description: '[[x, y], ...]で3点以上を指定します。輪郭は自動的に閉じます。' },
  { name: 'polyline', signature: 'polyline(points, style?)', snippet: 'polyline([[${1:0}, ${2:0}], [${3:100}, ${4:0}], [${5:100}, ${6:60}]])', description: '2～100000頂点の開いた折れ線を描きます。終点と始点は接続しません。fillの色指定は不可（null・"none"は可）。SVGはpolyline、DXFは閉じないLWPOLYLINE。Fusionでも開いたままです。合成の入力には使えません。' },
  { name: 'setUnit', signature: 'setUnit(unit)', snippet: 'setUnit("${1|mm,pt,px|}")', description: '図面全体の単位をmm・pt・pxから指定します。既定はmm。CLIの--unitが優先します。' },
  { name: 'setDpi', signature: 'setDpi(dpi)', snippet: 'setDpi(${1:96})', description: 'px単位のDPIを指定します。既定72、1～1000000の有限数。mm・ptには影響しません。' },
  { name: 'setCanvas', signature: 'setCanvas(width, height)', snippet: 'setCanvas(${1:1920}, ${2:1080})', description: 'SVGの表示範囲を固定します。幅と高さは入力単位の正数。DXFには影響しません。' },
  { name: 'setDxfMode', signature: 'setDxfMode(mode)', snippet: 'setDxfMode("${1|standard,fusion|}")', description: 'DXFの出力モードを指定します。fusionでは色・線幅・塗りを除いた輪郭を出力します。' },
  { name: 'draw', signature: 'draw(path, style?)', snippet: 'draw(${1:shape})', description: 'path APIで作ったパスをコピーして図面へ追加します。styleは元パスを変更しません。合成結果のstyle省略時は黒線・実寸0.25mm・塗りなし。DXFでは合成前の元図形と元スタイルを出力します。描画後に元パスを変換しても描画済み図形は変わりません。' },
  { name: 'combine', signature: 'combine(operation, callback, style?)', snippet: 'combine("${1|or,and,xor|}", () => {\n\tdrawing.rect(${2:0}, ${3:0}, ${4:100}, ${5:60});\n\tdrawing.circle(${6:50}, ${7:30}, ${8:25});\n})', description: '互換用の合成APIです。callback内で追加した2つ以上の閉じた図形をSVG出力時にClipper2で合成します。新規コードにはpath.combineとdrawing.drawを推奨します。DXFでは元図形を出力します。' }
];

const transformDescription = 'pos: {x,y}または[x,y]で相対移動、rot: 度数、scale: {x,y}または[x,y]で倍率（100が等倍、0不可、負数で反転）。anchorは名前または絶対座標。既定はcenter。アンカー中心の拡縮→回転→移動の順。SVGの正回転は時計回り、DXFでは反時計回り。線幅は変わりません。canvasCenterには先にdrawing.setCanvasが必要です。';
const anchors = 'topLeft,top,topRight,centerLeft,center,centerRight,bottomLeft,bottomCenter,bottomRight,canvasCenter';

export const pathMethods: readonly ApiMethod[] = [
  ...drawingMethods.slice(0, 5).map(method => ({
    ...method,
    description: method.description.replace('描きます。', 'パスを作成します。')
      + ' この時点では描画せずパスを返します。drawing.drawで図面へ追加します。'
  })),
  { name: 'move', signature: 'move(path, x, y)', snippet: 'move(${1:shape}, ${2:10}, ${3:20})', description: '入力単位で相対移動し、元パスを変更して同じパスを返します。x/y両方が必須。負数・0も指定可能。線幅・スタイルは変えません。draw済みの図形は変更しません。範囲超過時は元パスを変更しません。' },
  { name: 'moved', signature: 'moved(path, x, y)', snippet: 'moved(${1:shape}, ${2:30}, ${3:0})', description: '入力単位で相対移動した独立した新しいパスを返します。元パスは変更しません。x/y両方が必須。負数・0も指定可能。線幅・スタイルは変えません。' },
  { name: 'combine', signature: 'combine(operation, a, b, ...)', snippet: 'combine("${1|or,and,xor|}", ${2:a}, ${3:b})', description: '2つ以上の閉じたパスを合成します。orは和集合、andは共通部分、xorは排他的論理和。SVG出力時にClipper2で実際の輪郭を計算し、穴を複合パスで保持します。円は最大偏差0.01mmで近似します。line・polylineとsvgcombineの結果は入力不可。drawing.drawで描画します。DXFでは元図形と元スタイルを出力します。' },
  { name: 'svgcombine', signature: 'svgcombine(operation, a, b, ...)', snippet: 'svgcombine("${1|or,and,xor|}", ${2:a}, ${3:b})', description: '元図形を保持してSVGの表示時に合成します。or・and・xor、2つ以上の閉じたパスを指定します。XORだけの長方形・円・楕円・正多角形・星型は偶奇規則の複合パス、それ以外はマスク等を使用します。line・polylineは入力不可。combineの結果も入力可能ですが、逆にcombineへ渡すことはできません。drawing.drawで描画します。DXFでは元図形を出力します。' },
  { name: 'transform', signature: 'transform(path, parameters?)', snippet: 'transform(${1:shape}, { pos: [${2:0}, ${3:0}], rot: ${4:0}, scale: [${5:100}, ${6:100}], anchor: "${7|' + anchors + '|}" })', description: '元パスを変更し、同じパスを返します。' + transformDescription },
  { name: 'transformed', signature: 'transformed(path, parameters?)', snippet: 'transformed(${1:shape}, { pos: [${2:60}, ${3:0}], rot: ${4:0}, scale: [${5:100}, ${6:100}], anchor: "${7|' + anchors + '|}" })', description: '元パスを変更せず、独立した新しいパスを返します。' + transformDescription },
  { name: 'regularPolygon', signature: 'regularPolygon(options?, style?)', snippet: 'regularPolygon({ center: [${1:0}, ${2:0}], radius: ${3:30}, radiusMode: "${4|vertex,edge|}", sides: ${5:6}, rot: ${6:0} })', description: '正多角形のパスを返します。既定: center=[0,0]、radius=30（入力単位）、radiusMode="vertex"、sides=3、rot=0。radiusは正数、sidesは3～100000の整数。vertexは頂点まで、edgeは辺中心までの半径。回転0では頂点または辺中心がY負方向です。centerは{x,y}も指定可能。drawing.drawで描画します。' },
  { name: 'star', signature: 'star(options?, style?)', snippet: 'star({ center: [${1:0}, ${2:0}], outerRadius: ${3:30}, innerRadius: ${4:15}, points: ${5:5}, rot: ${6:0} })', description: '星型のパスを返します。既定: center=[0,0]、outerRadius=30、innerRadius=15（入力単位）、points=5、rot=0。0 < innerRadius < outerRadius、pointsは3～50000の整数。輪郭の頂点数はpointsの2倍。回転0では最初の頂点がY負方向です。centerは{x,y}も指定可能。drawing.drawで描画します。' }
];

// 同じ引数を受け取る直接描画API。パス用の説明をそのまま使わないようにします。
const directRadialMethods = pathMethods.filter(method => ['regularPolygon', 'star'].includes(method.name)).map(method => ({
  ...method,
  description: method.description.replace(/(正多角形|星型)のパスを返します。/, '$1を直接描画します。戻り値はundefinedです。')
    .replace('drawing.drawで描画します。', '')
}));
export const allDrawingMethods: readonly ApiMethod[] = [...drawingMethods, ...directRadialMethods];
export const pathValueMethods: readonly ApiMethod[] = pathMethods.filter(method => ['move', 'moved'].includes(method.name)).map(method => ({
  ...method,
  signature: `${method.name}(x, y)`,
  snippet: method.name + '(${1:10}, ${2:20})'
}));

const apiMethods = { drawing: allDrawingMethods, path: pathMethods, value: pathValueMethods };
type Namespace = keyof typeof apiMethods;

// 簡易な構文照合。pathで作成した変数と、そのmove/movedによるコピーを追跡します。
function receiverNamespace(prefix: string, source: string): Namespace | undefined {
  const api = /(?:^|[^\w$.])(drawing|path)\s*\.\s*\w*$/.exec(prefix);
  if (api) { return api[1] as Namespace; }
  const names = pathMethods.map(method => method.name).join('|');
  if (new RegExp(`(?:^|[^\\w$.])path\\.(?:${names})\\([^;\\n]*\\)\\s*\\.\\s*\\w*$`).test(prefix)) {
    return 'value';
  }
  const receiver = /(?:^|[^\w$.])([A-Za-z_$][\w$]*)\s*\.\s*\w*$/.exec(prefix)?.[1];
  if (!receiver) { return undefined; }
  const variables = new Set<string>();
  const assignments = /\b(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:path\.(\w+)\s*\(|([A-Za-z_$][\w$]*)\s*\.(?:move|moved)\s*\()/g;
  for (const match of source.matchAll(assignments)) {
    if ((match[2] && pathMethods.some(method => method.name === match[2])) || variables.has(match[3])) {
      variables.add(match[1]);
    }
  }
  return variables.has(receiver) ? 'value' : undefined;
}

function documentation(namespace: Namespace, method: ApiMethod): vscode.MarkdownString {
  const text = new vscode.MarkdownString();
  text.appendCodeblock(`${namespace === 'value' ? 'パス値' : namespace}.${method.signature}`, 'javascript');
  text.appendText(method.description);
  if (method.signature.includes('style?')) {
    text.appendText('\n\nstyleは省略可能です。stroke: "#RRGGBB" | null | "none"、strokeWidth: 正数、fill: "#RRGGBB" | null | "none"。既定は黒線・実寸0.25mm・塗りなしです。');
  }
  return text;
}

export function registerLanguageSupport(context: vscode.ExtensionContext): void {
  // JavaScriptのファイル関連付けを既に設定している場合にもAPI補完を提供します。
  const selector: vscode.DocumentSelector = [
    { language: 'zfj-js' },
    { language: 'javascript', pattern: '**/*.sjs' },
    { language: 'javascript', pattern: '**/*.djs' }
  ];
  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(selector, {
      provideCompletionItems(document, position) {
        const prefix = document.lineAt(position.line).text.slice(0, position.character);
        const namespace = receiverNamespace(prefix, document.getText());
        if (!namespace) {
          return undefined;
        }
        const word = document.getWordRangeAtPosition(position);
        const suffix = document.lineAt(position.line).text.slice(word?.end.character ?? position.character);
        return apiMethods[namespace].map(method => {
          const item = new vscode.CompletionItem(method.name, vscode.CompletionItemKind.Method);
          item.detail = `${namespace === 'value' ? 'パス値' : namespace}.${method.signature}`;
          item.documentation = documentation(namespace, method);
          item.insertText = /^\s*\(/.test(suffix) ? method.name : new vscode.SnippetString(method.snippet);
          item.range = word ?? new vscode.Range(position, position);
          return item;
        });
      }
    }, '.'),
    vscode.languages.registerHoverProvider(selector, {
      provideHover(document, position) {
        const range = document.getWordRangeAtPosition(position);
        if (!range) { return undefined; }
        const prefix = document.lineAt(position.line).text.slice(0, range.start.character);
        const namespace = receiverNamespace(prefix, document.getText());
        if (!namespace) { return undefined; }
        const method = apiMethods[namespace].find(entry => entry.name === document.getText(range));
        return method ? new vscode.Hover(documentation(namespace, method), range) : undefined;
      }
    })
  );
}
