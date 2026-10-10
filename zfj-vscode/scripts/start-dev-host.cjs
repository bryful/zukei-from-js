// VS CodeのextensionHost launch経路を避け、通常起動後にデバッガーを接続します。
const { spawn } = require('node:child_process');
const net = require('node:net');
const http = require('node:http');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

const port = 9333;
const [executable, folder] = process.argv.slice(2);

function checkPort() {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', () => reject(new Error(
      'ポート9333が使用中です。前の開発用ウィンドウを閉じてからF5を押してください。'
    )));
    server.listen(port, '127.0.0.1', () => server.close(resolve));
  });
}

function inspectorReady() {
  return new Promise(resolve => {
    const request = http.get(`http://127.0.0.1:${port}/json/list`, response => {
      let body = '';
      response.on('data', chunk => { body += chunk; });
      response.on('end', () => {
        try {
          resolve(JSON.parse(body).some(target => target.webSocketDebuggerUrl));
        } catch { resolve(false); }
      });
      response.on('error', () => resolve(false));
    });
    request.setTimeout(500, () => request.destroy());
    request.on('error', () => resolve(false));
  });
}

async function main() {
  if (!executable || !folder) { throw new Error('VS Codeのパスと拡張機能フォルダが必要です。'); }
  await checkPort();
  const env = { ...process.env };
  // ターミナルの自動アタッチ設定を開発用ホストへ引き継がないようにします。
  delete env.NODE_OPTIONS;
  delete env.VSCODE_INSPECTOR_OPTIONS;
  delete env.ELECTRON_RUN_AS_NODE;
  const child = spawn(executable, [
    '--new-window', '--disable-extensions',
    '--user-data-dir', path.join(folder, '.dev-profile'),
    `--extensionDevelopmentPath=${folder}`, `--inspect-extensions=${port}`,
    '--folder-uri', pathToFileURL(folder).href
  ], { detached: true, stdio: 'ignore', env });
  await new Promise((resolve, reject) => {
    child.once('spawn', resolve);
    child.once('error', reject);
  });
  child.unref();
  console.log('ローカルの開発用ウィンドウを起動しています…');
  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) {
    if (await inspectorReady()) {
      console.log('接続準備ができました。デバッガーを接続します。');
      return;
    }
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  throw new Error('開発用ホストの接続準備が30秒以内に完了しませんでした。VS Codeのログを確認してください。');
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
