// OSの登録は変更せず、コマンドからexeへの引数・成功・失敗・キャンセルを検証します。
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const commands = new Map();
const calls = [];
const errors = [];
const notifications = [];
const logs = [];
let executable = 'C:\\日本語 空白\\zfj.exe';
let failure;
let selected = 0;
let pending;
let delay = false;
const processMock = { platform: 'win32' };
const vscode = {
  workspace: { isTrusted: true },
  commands: { registerCommand: (name, callback) => { commands.set(name, callback); return {}; } },
  window: {
    showInformationMessage: async message => { notifications.push(message); },
    showErrorMessage: async message => { errors.push(message); },
    withProgress: async (_, callback) => callback()
  },
  ProgressLocation: { Notification: 15 }
};
const mockExecFile = (file, args, options, callback) => {
  calls.push({ file, args: Array.from(args), options });
  if (delay) { pending = callback; return; }
  callback(failure, failure ? '' : 'Done', failure ? 'Unknown option' : '');
};
const sandbox = { exports: {}, process: processMock, require: name => {
  if (name === 'vscode') { return vscode; }
  if (name === 'node:child_process') { return { execFile: mockExecFile }; }
  return require(name);
} };
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../out/file-associations.js'), 'utf8'), sandbox);
sandbox.exports.registerFileAssociationCommands({ subscriptions: [] }, {
  appendLine: message => logs.push(message), show() {}
}, async () => { selected++; return executable; });
const install = () => commands.get('zfj.registerFileAssociations')();
const uninstall = () => commands.get('zfj.unregisterFileAssociations')();
(async () => {
  await install(); await uninstall();
  assert.equal(calls.length, 2);
  assert.deepEqual(calls.map(call => call.args), [['--install'], ['--uninstall']]);
  for (const call of calls) {
    assert.equal(call.file, executable);
    assert.equal(call.options.windowsHide, true);
    assert.equal(call.options.timeout, 30000);
    assert.equal(call.options.shell, undefined);
  }
  assert.equal(errors.length, 0);
  assert.equal(notifications.length, 2);
  executable = undefined;
  await install(); assert.equal(calls.length, 2);
  executable = 'C:\\Tools\\zfj.exe';
  failure = new Error('failed');
  await install();
  assert.ok(errors.at(-1).includes('Unknown option'));
  failure = undefined;
  await uninstall(); assert.equal(calls.length, 4); // 失敗後も再実行できる
  vscode.workspace.isTrusted = false;
  const beforeSelected = selected;
  await install(); assert.equal(selected, beforeSelected); assert.equal(calls.length, 4);
  vscode.workspace.isTrusted = true;
  processMock.platform = 'linux';
  await install(); assert.equal(calls.length, 4);
  processMock.platform = 'win32';
  delay = true;
  const first = install();
  await Promise.resolve();
  await uninstall(); assert.equal(calls.length, 5);
  pending(null, 'Done', ''); await first;
  assert.ok(logs.some(line => line.includes('--install')));
  console.log('PASS: association commands, exact standalone arguments, cancellation, errors/retry, trust/platform checks, concurrent-operation guard (registry unchanged)');
})().catch(error => { console.error(error); process.exitCode = 1; });
