import * as vscode from 'vscode';
import * as path from 'node:path';
import * as os from 'node:os';
import { execFile } from 'node:child_process';
import { mkdtemp, readFile, unlink, rmdir } from 'node:fs/promises';

interface Preview {
  panel: vscode.WebviewPanel;
  document: vscode.TextDocument;
  executable: string;
  queue: Promise<void>;
  disposed: boolean;
  image?: string;
}

function escapeHtml(text: string): string {
  return text.replace(/[&<>"']/g, character => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  }[character]!));
}

// SVGはimgのデータURIとして表示し、スクリプトや外部リソースの読み込みを許可しません。
export function previewHtml(image: string | undefined, message: string): string {
  return `<!DOCTYPE html>
<html lang="ja"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data:; style-src 'unsafe-inline';">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ZFJ SVG Preview</title>
<style>
body { margin: 0; padding: 16px; color: var(--vscode-foreground); background: var(--vscode-editor-background); font-family: var(--vscode-font-family); }
p { white-space: pre-wrap; overflow-wrap: anywhere; }
.canvas { background: white; padding: 16px; overflow: auto; }
img { display: block; max-width: 100%; height: auto; margin: auto; }
</style></head><body><p role="status">${escapeHtml(message)}</p>
${image ? `<div class="canvas"><img src="data:image/svg+xml;base64,${image}" alt="生成した図形"></div>` : ''}
</body></html>`;
}

async function generate(preview: Preview, output: vscode.OutputChannel): Promise<void> {
  if (preview.disposed) { return; }
  let directory: string | undefined;
  let destination: string | undefined;
  try {
    // 保存時にも信頼状態を再確認します。
    if (!vscode.workspace.isTrusted) { throw new Error('プレビューにはワークスペースの信頼が必要です。'); }
    preview.panel.webview.html = previewHtml(preview.image, 'プレビューを更新しています…');
    directory = await mkdtemp(path.join(os.tmpdir(), 'zfj-preview-'));
    destination = path.join(directory, 'preview.svg');
    const input = preview.document.uri.fsPath;
    await new Promise<void>((resolve, reject) => {
      execFile(preview.executable, [input, '-o', destination!], {
        cwd: path.dirname(input), windowsHide: true, timeout: 30000,
        encoding: 'utf8', maxBuffer: 4 * 1024 * 1024
      }, (error, stdout, stderr) => {
        if (stdout) { output.appendLine(stdout.trimEnd()); }
        if (stderr) { output.appendLine(stderr.trimEnd()); }
        if (error) { reject(new Error(stderr.trim() || error.message)); }
        else { resolve(); }
      });
    });
    const svg = await readFile(destination);
    if (!preview.disposed) {
      preview.image = svg.toString('base64');
      preview.panel.webview.html = previewHtml(preview.image,
        `${path.basename(input)} — 保存すると更新します。`);
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    output.appendLine(`プレビューエラー: ${message}`);
    if (!preview.disposed) {
      preview.panel.webview.html = previewHtml(preview.image,
        `更新に失敗しました。${preview.image ? '前回成功した画像を表示しています。\n' : '\n'}${message}`);
    }
  } finally {
    if (destination) {
      try { await unlink(destination); }
      catch (error) {
        if ((error as NodeJS.ErrnoException).code !== 'ENOENT') {
          output.appendLine(`一時ファイルの削除に失敗: ${String(error)}`);
        }
      }
    }
    if (directory) {
      try { await rmdir(directory); }
      catch (error) { output.appendLine(`一時フォルダの削除に失敗: ${String(error)}`); }
    }
  }
}

export function registerPreview(
  context: vscode.ExtensionContext,
  output: vscode.OutputChannel,
  chooseExecutable: (document: vscode.TextDocument) => Promise<string | undefined>
): void {
  const previews = new Map<string, Preview>();
  // 同じプレビューの生成を直列化し、古い実行結果による上書きを避けます。
  function refresh(preview: Preview): Promise<void> {
    preview.queue = preview.queue.then(() => generate(preview, output));
    return preview.queue;
  }
  context.subscriptions.push(
    vscode.commands.registerCommand('zfj.preview', async () => {
      try {
        if (!vscode.workspace.isTrusted) { throw new Error('プレビューにはワークスペースの信頼が必要です。'); }
        const activePreview = [...previews.values()].find(entry => entry.panel.active);
        const document = vscode.window.activeTextEditor?.document ?? activePreview?.document;
        if (!document || document.uri.scheme !== 'file' || path.extname(document.uri.fsPath) !== '.sjs') {
          throw new Error('ローカルの.sjsファイルを開いてください。DXFのプレビューには未対応です。');
        }
        if (document.isDirty && !(await document.save())) { return; }
        const executable = await chooseExecutable(document);
        if (!executable) { return; }
        const key = document.uri.toString();
        let preview = previews.get(key);
        if (preview) {
          preview.document = document;
          preview.executable = executable;
          preview.panel.reveal(vscode.ViewColumn.Beside, true);
        } else {
          const panel = vscode.window.createWebviewPanel('zfj.preview',
            `ZFJ: ${path.basename(document.uri.fsPath)}`, { viewColumn: vscode.ViewColumn.Beside, preserveFocus: true },
            { enableScripts: false, localResourceRoots: [] });
          preview = { panel, document, executable, disposed: false, queue: Promise.resolve() };
          previews.set(key, preview);
          const state = preview;
          const subscription = panel.onDidDispose(() => {
            state.disposed = true;
            previews.delete(key);
            subscription.dispose();
          });
        }
        await refresh(preview);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        output.appendLine(`プレビューエラー: ${message}`);
        void vscode.window.showErrorMessage(`ZFJ: ${message}`);
      }
    }),
    vscode.workspace.onDidSaveTextDocument(document => {
      const preview = previews.get(document.uri.toString());
      if (preview) {
        preview.document = document;
        return refresh(preview);
      }
    }),
    { dispose() { for (const preview of [...previews.values()]) { preview.panel.dispose(); } } }
  );
}
