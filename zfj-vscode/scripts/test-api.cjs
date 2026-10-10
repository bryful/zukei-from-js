// VS Code APIを模擬し、補完と実際のzfj.exeによる書き出し・プレビューを検証します。
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const { execFileSync } = require('node:child_process');

const root = path.resolve(__dirname, '..');
const repository = path.dirname(root);
const executable = path.resolve(process.argv[2] || path.join(repository, 'x64/Release/zfj.exe'));
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'zfj-extension-test-'));
const commands = new Map();
const panels = [];
const errors = [];
let activeDocument;
let destination;
let saveHandler;
let completionProvider;
let hoverProvider;
let saves = 0;
const uri = file => ({ scheme: 'file', fsPath: file, toString: () => file });
const documentFor = file => ({ uri: uri(file), isDirty: false, save: async () => { saves++; return true; } });

class MarkdownString {
  constructor() { this.value = ''; }
  appendCodeblock(text) { this.value += text; return this; }
  appendText(text) { this.value += text; return this; }
}
const vscode = {
  workspace: {
    isTrusted: true,
    getConfiguration: () => ({ get: () => executable }),
    onDidSaveTextDocument: handler => { saveHandler = handler; return {}; }
  },
  languages: {
    registerCompletionItemProvider: (_, provider) => { completionProvider = provider; return {}; },
    registerHoverProvider: (_, provider) => { hoverProvider = provider; return {}; }
  },
  window: {
    get activeTextEditor() { return { document: activeDocument }; },
    createOutputChannel: () => ({ appendLine() {}, show() {} }),
    showSaveDialog: async () => uri(destination),
    showWarningMessage: async () => '上書きする',
    showErrorMessage: async message => { errors.push(message); },
    showInformationMessage: async () => {},
    withProgress: async (_, callback) => callback(),
    createWebviewPanel: (_, title, __, options) => {
      assert.equal(options.enableScripts, false);
      let onDispose;
      const panel = {
        title, active: false, webview: { html: '' }, reveal() {},
        onDidDispose(callback) { onDispose = callback; return { dispose() {} }; },
        dispose() { onDispose(); }
      };
      panels.push(panel);
      return panel;
    }
  },
  commands: { registerCommand: (name, callback) => { commands.set(name, callback); return {}; } },
  Uri: { file: uri }, ViewColumn: { Beside: 2 }, ProgressLocation: { Notification: 15 },
  CompletionItemKind: { Method: 1 },
  CompletionItem: class { constructor(label) { this.label = label; } },
  SnippetString: class { constructor(value) { this.value = value; } },
  MarkdownString,
  Range: class { constructor(start, end) { this.start = start; this.end = end; } },
  Hover: class { constructor(contents) { this.contents = contents; } }
};
const modules = new Map();
function loadModule(file) {
  if (modules.has(file)) { return modules.get(file); }
  const sandbox = { exports: {}, require: name => {
    if (name === 'vscode') { return vscode; }
    if (name.startsWith('.')) { return loadModule(path.resolve(path.dirname(file), name + '.js')); }
    return require(name);
  } };
  vm.runInNewContext(fs.readFileSync(file, 'utf8'), sandbox, { filename: file });
  modules.set(file, sandbox.exports);
  return sandbox.exports;
}
function expandSnippet(snippet) {
  const defaults = new Map();
  return snippet
    .replace(/\$\{(\d+)\|([^|]+)\|\}/g, (_, index, choices) => {
      const value = choices.split(',')[0]; defaults.set(index, value); return value;
    })
    .replace(/\$\{(\d+):([^}]+)\}/g, (_, index, value) => { defaults.set(index, value); return value; })
    .replace(/\$\{(\d+)\}/g, (_, index) => defaults.get(index) || '')
    .replace(/\$\d+/g, '');
}

async function main() {
  const language = loadModule(path.join(root, 'out/language.js'));
  loadModule(path.join(root, 'out/extension.js')).activate({ subscriptions: [] });
  const cpp = fs.readFileSync(path.join(repository, 'zfj/drawing_api.cpp'), 'utf8');
  const registered = { drawing: ['line', 'rect', 'circle', 'polygon'], path: ['line', 'rect', 'circle', 'polygon'] };
  for (const match of cpp.matchAll(/JS_SetPropertyStr\(\s*context,\s*(api|pathApi),\s*"(\w+)"/g)) {
    registered[match[1] === 'api' ? 'drawing' : 'path'].push(match[2]);
  }
  const setup = 'drawing.setCanvas(300,300); var a=path.rect(0,0,100,100); var b=path.circle(70,50,40); var shape=path.rect(10,20,40,30);\n';
  let checks = 0;
  function checkScript(code) {
    const input = path.join(temp, 'check.sjs');
    fs.writeFileSync(input, setup + code, 'utf8');
    execFileSync(executable, [input, '--check'], { timeout: 30000, stdio: 'pipe' });
    checks++;
  }
  for (const namespace of ['drawing', 'path']) {
    const metadata = language[namespace === 'drawing' ? 'allDrawingMethods' : 'pathMethods'];
    assert.deepEqual(Array.from(metadata, item => item.name).sort(), registered[namespace].sort());
    const text = namespace + '.';
    const document = { lineAt: () => ({ text }), getWordRangeAtPosition: () => undefined, getText: () => text };
    const items = completionProvider.provideCompletionItems(document, { line: 0, character: text.length });
    assert.equal(items.length, metadata.length);
    for (const item of items) {
      assert.ok(item.detail.startsWith(namespace + '.'));
      checkScript(namespace + '.' + expandSnippet(item.insertText.value) + ';');
      const range = { start: { character: text.length }, end: { character: text.length + item.label.length } };
      const existing = {
        lineAt: () => ({ text: text + item.label + '()' }),
        getWordRangeAtPosition: () => range,
        getText: () => item.label
      };
      const hover = hoverProvider.provideHover(existing, { line: 0, character: range.end.character });
      assert.ok(hover.contents.value.startsWith(namespace + '.'));
      const replacement = completionProvider.provideCompletionItems(existing, { line: 0, character: range.end.character });
      assert.equal(replacement.find(entry => entry.label === item.label).insertText, item.label);
    }
  }
  assert.equal(completionProvider.provideCompletionItems({
    lineAt: () => ({ text: 'other.path.' }), getWordRangeAtPosition: () => undefined, getText: () => 'other.path.'
  }, { line: 0, character: 11 }), undefined);

  for (const [source, receiver] of [
    ['var p = path.offset(path.rect(0,0,20,20),2);\np.', 'p'],
    ['var p = path.polyline([[0,0],[10,10]]);\np.', 'p'],
    ['var p = path.star(); var q = p.moved(30,0);\nq.', 'q'],
    ['path.circle(0,0,10).', 'path.circle(0,0,10)'],
    ['path.circle(0,0,10).moved(30,40).', 'path.circle(0,0,10).moved(30,40)']
  ]) {
    const text = source.split('\n').at(-1);
    const valueDocument = { lineAt: () => ({ text }), getWordRangeAtPosition: () => undefined, getText: () => source };
    const items = completionProvider.provideCompletionItems(valueDocument, { line: 0, character: text.length });
    assert.deepEqual(Array.from(items, item => item.label), ['move', 'moved']);
    for (const item of items) { checkScript('shape.' + expandSnippet(item.insertText.value) + ';'); }
    const range = { start: { character: text.length }, end: { character: text.length + 4 } };
    const hover = hoverProvider.provideHover({
      lineAt: () => ({ text: text + 'move(1,2)' }), getWordRangeAtPosition: () => range,
      getText: argument => argument ? 'move' : source
    }, { line: 0, character: text.length + 2 });
    assert.ok(hover.contents.value.includes('move(x, y)'), receiver);
  }
  assert.equal(completionProvider.provideCompletionItems({
    lineAt: () => ({ text: 'object.' }), getWordRangeAtPosition: () => undefined,
    getText: () => 'var object = {};\nobject.'
  }, { line: 0, character: 7 }), undefined);

  const snippets = JSON.parse(fs.readFileSync(path.join(root, 'snippets/zfj.json'), 'utf8'));
  for (const snippet of Object.values(snippets)) {
    let code = expandSnippet(Array.isArray(snippet.body) ? snippet.body.join('\n') : snippet.body);
    if (snippet.prefix === 'zstyle') { code = `drawing.rect(0,0,10,10,${code});`; }
    checkScript(code);
  }
  // 全演算と全アンカーを、実装が受け付ける値であることまで確認します。
  for (const operation of ['or', 'and', 'xor']) {
    checkScript(`drawing.draw(path.combine("${operation}",a,b)); drawing.draw(path.svgcombine("${operation}",a,b));`);
  }
  for (const anchor of ['topLeft', 'top', 'topRight', 'centerLeft', 'center', 'centerRight', 'bottomLeft', 'bottomCenter', 'bottomRight', 'canvasCenter']) {
    checkScript(`drawing.draw(path.transformed(shape,{anchor:"${anchor}",rot:30,scale:[150,80]}));`);
  }

  for (const clockwise of [true, false]) {
    for (const namespace of ['drawing', 'path']) {
      const call = namespace + '.circle(50,50,{radiusX:30,radiusY:15,start:0,end:270,clockwise:' + clockwise + '})';
      checkScript(namespace === 'path' ? 'drawing.draw(' + call + ');' : call + ';');
    }
  }
  for (const cap of ['butt', 'square', 'round']) {
    for (const join of ['miter', 'round', 'bevel']) {
      checkScript('drawing.draw(path.offset(path.polyline([[0,0],[50,0],[50,50]]),5,{cap:"' + cap + '",join:"' + join + '"}));');
    }
  }
  for (const distance of [-5,0,5]) { checkScript('drawing.draw(path.offset(shape,' + distance + '));'); }
  let exports = 0;
  for (const name of ['basic', 'combine', 'path-values', 'transforms', 'radial-shapes', 'polyline-move', 'svg-render-combine', 'path-combine', 'circle-arcs', 'offsets']) {
    for (const extension of ['sjs', 'djs']) {
      const input = path.join(repository, 'examples', `${name}.${extension}`);
      if (!fs.existsSync(input)) { continue; }
      activeDocument = documentFor(input);
      activeDocument.isDirty = true;
      destination = path.join(temp, `日本語 空白-${name}.${extension === 'sjs' ? 'svg' : 'dxf'}`);
      await commands.get('zfj.export')();
      assert.deepEqual(errors, []);
      const content = fs.readFileSync(destination, 'utf8');
      assert.ok(extension === 'sjs' ? content.includes('<svg') : content.includes('ENTITIES'));
      if (name === 'circle-arcs') {
        assert.ok(extension === 'sjs' ? /<path[^>]*d="[^"]*A/.test(content) : /\bARC\b/.test(content));
        assert.ok(extension === 'sjs' ? content.includes('<ellipse') : /\bELLIPSE\b/.test(content));
      }
      if (name === 'offsets') { assert.ok(content.includes(extension === 'sjs' ? '<path' : 'LWPOLYLINE')); }
      exports++;
    }
  }
  assert.equal(saves, exports);

  const previewInput = path.join(temp, 'プレビュー 空白.sjs');
  activeDocument = documentFor(previewInput);
  fs.copyFileSync(path.join(repository, 'examples/radial-shapes.sjs'), previewInput);
  await commands.get('zfj.preview')();
  assert.deepEqual(errors, []);
  const panel = panels.at(-1);
  const image = () => Buffer.from(panel.webview.html.match(/base64,([^"]+)/)[1], 'base64').toString('utf8');
  assert.ok(image().includes('<polygon'));
  for (const [name, marker] of [['transforms', '<ellipse'], ['polyline-move', '<polyline'], ['svg-render-combine', 'evenodd'], ['path-combine', '<path'], ['circle-arcs', '<ellipse'], ['offsets', '<path']]) {
    fs.copyFileSync(path.join(repository, 'examples', name + '.sjs'), previewInput);
    await saveHandler(activeDocument);
    assert.ok(image().includes(marker), name);
  }
  fs.writeFileSync(previewInput, setup + 'drawing.draw(path.svgcombine("and",a,b),{stroke:null,fill:"#66aaff"});');
  await saveHandler(activeDocument);
  assert.ok(image().includes('<mask'));
  const beforeError = image();
  fs.writeFileSync(previewInput, 'path.star(');
  await saveHandler(activeDocument);
  assert.equal(image(), beforeError);
  assert.ok(panel.webview.html.includes('更新に失敗'));
  panel.dispose();
  assert.equal(saveHandler(activeDocument), undefined);
  console.log(`PASS: 27 API completions/hover, path-value methods/chains, ${checks} executable checks, ${exports} SVG/DXF exports, preview refresh (polygon/polyline/ellipse/path/mask) and error retention`);
}

main().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
  // このテストが作った一時フォルダの直下だけを片付けます。
  for (const entry of fs.readdirSync(temp)) { fs.unlinkSync(path.join(temp, entry)); }
  fs.rmdirSync(temp);
});
