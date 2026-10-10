// 誤って保存された「local」リモート参照だけを修復します。
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { execFileSync } = require('node:child_process');
const { DatabaseSync } = require('node:sqlite');

const running = execFileSync('tasklist.exe', ['/FI', 'IMAGENAME eq Code.exe', '/FO', 'CSV', '/NH'], { encoding: 'utf8' });
if (/"Code\.exe"/i.test(running)) {
  console.error('すべてのVS Codeウィンドウを閉じてから実行してください。変更は行っていません。');
  process.exit(1);
}

const profile = process.env.USERPROFILE;
const storage = path.join(profile, 'AppData/Roaming/Code/User/globalStorage/storage.json');
const shared = path.join(profile, '.vscode-shared/sharedStorage/state.vscdb');
const badPrefix = 'vscode-remote://local/';
const target = path.resolve(__dirname, '..');
let changes = 0;

function repair(value) {
  if (typeof value === 'string' && value.startsWith(badPrefix)) {
    const decoded = decodeURIComponent(value.slice(badPrefix.length)).replace(/\//g, '\\');
    // 今回のプロジェクトの誤参照だけを対象にします。
    if (path.resolve(decoded).toLowerCase() !== target.toLowerCase()) { return value; }
    changes++;
    return pathToFileURL(target).href;
  }
  if (!value || typeof value !== 'object') { return value; }
  const result = Array.isArray(value) ? [] : {};
  const hasBadFolder = Object.values(value).some(entry => typeof entry === 'string'
    && entry.startsWith(badPrefix)
    && path.resolve(decodeURIComponent(entry.slice(badPrefix.length))).toLowerCase() === target.toLowerCase());
  for (const [key, entry] of Object.entries(value)) {
    if (hasBadFolder && key === 'remoteAuthority' && entry === 'local') { changes++; continue; }
    const repairedKey = key.startsWith(badPrefix) ? repair(key) : key;
    result[repairedKey] = repair(entry);
  }
  return result;
}

const backup = path.join(target, 'restore-backups', new Date().toISOString().replace(/[:.]/g, '-'));
fs.mkdirSync(backup, { recursive: true });
const original = fs.readFileSync(storage, 'utf8');
const updated = repair(JSON.parse(original));
fs.writeFileSync(path.join(backup, 'storage.json'), original);
if (fs.existsSync(shared)) {
  // VS Code終了後なので、共有DBと補助ファイルをそのまま退避できます。
  for (const suffix of ['', '-wal', '-shm']) {
    if (fs.existsSync(shared + suffix)) {
      fs.copyFileSync(shared + suffix, path.join(backup, 'shared-state.vscdb' + suffix));
    }
  }
  const db = new DatabaseSync(shared);
  try {
    const key = 'history.recentlyOpenedPathsList';
    const row = db.prepare('SELECT value FROM ItemTable WHERE key = ?').get(key);
    if (row) {
      const history = repair(JSON.parse(String(row.value)));
      db.prepare('UPDATE ItemTable SET value = ? WHERE key = ?').run(JSON.stringify(history), key);
    }
  } finally { db.close(); }
}
fs.writeFileSync(storage, JSON.stringify(updated, null, 4));
console.log(`修復した参照・接続指定: ${changes}`);
console.log(`バックアップ: ${backup}`);
