import * as vscode from 'vscode';
import * as path from 'node:path';
import { execFile } from 'node:child_process';
import { access, stat } from 'node:fs/promises';
import { constants } from 'node:fs';
import { registerLanguageSupport } from './language';
import { registerPreview } from './preview';
import { registerFileAssociationCommands } from './file-associations';

let exporting = false;

async function chooseExecutable(document?: vscode.TextDocument): Promise<string | undefined> {
  const config = vscode.workspace.getConfiguration('zfj', document?.uri);
  const configured = config.get<string>('executablePath', '').trim();
  if (configured) {
    if (!path.isAbsolute(configured)) {
      throw new Error('設定「ZFJ: Executable Path」には絶対パスを指定してください。');
    }
    if (!(await stat(configured)).isFile()) {
      throw new Error('設定したzfj.exeがファイルではありません。');
    }
    return configured;
  }

  const selected = await vscode.window.showOpenDialog({
    title: 'ZFJ: Select Executable',
    canSelectMany: false,
    canSelectFiles: true,
    canSelectFolders: false,
    filters: { 'Windows実行ファイル': ['exe'] }
  });
  if (!selected?.length) {
    return undefined;
  }
  const executable = selected[0].fsPath;
  await config.update('executablePath', executable, vscode.ConfigurationTarget.Global);
  return executable;
}

async function exportDrawing(output: vscode.OutputChannel): Promise<void> {
  if (exporting) {
    void vscode.window.showInformationMessage('ZFJの書き出しを実行中です。');
    return;
  }
  exporting = true;
  try {
    if (!vscode.workspace.isTrusted) {
      throw new Error('書き出しを実行するには、ワークスペースを信頼してください。');
    }
    const document = vscode.window.activeTextEditor?.document;
    if (!document || document.uri.scheme !== 'file') {
      throw new Error('ローカルに保存した.sjsまたは.djsファイルを開いてください。');
    }
    const input = document.uri.fsPath;
    const extension = path.extname(input);
    if (extension !== '.sjs' && extension !== '.djs') {
      throw new Error('対象は小文字の.sjsまたは.djsファイルです。');
    }
    if (document.isDirty && !(await document.save())) {
      return;
    }
    const executable = await chooseExecutable(document);
    if (!executable) {
      return;
    }
    const format = extension === '.sjs' ? 'svg' : 'dxf';
    const destination = await vscode.window.showSaveDialog({
      title: 'ZFJ: Export',
      defaultUri: vscode.Uri.file(input.slice(0, -extension.length) + '.' + format),
      filters: extension === '.sjs' ? { SVG: ['svg'], PDF: ['pdf'] } : { DXF: ['dxf'] }
    });
    if (!destination) {
      return;
    }
    const allowed = extension === '.sjs' ? ['.svg', '.pdf'] : ['.dxf'];
    if (destination.scheme !== 'file' || !allowed.includes(path.extname(destination.fsPath))) {
      throw new Error(`出力先はローカルの${allowed.join(' / ')}ファイルを指定してください。`);
    }
    const args = [input, '-o', destination.fsPath];
    try {
      await access(destination.fsPath, constants.F_OK);
      const answer = await vscode.window.showWarningMessage(
        `${path.basename(destination.fsPath)}を上書きしますか？`,
        { modal: true },
        '上書きする'
      );
      if (answer !== '上書きする') {
        return;
      }
      args.push('--force');
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'ENOENT') {
        throw error;
      }
    }

    output.show(true);
    output.appendLine(`\n書き出し: ${input}`);
    output.appendLine(`実行ファイル: ${executable}`);
    output.appendLine(`出力先: ${destination.fsPath}`);
    await vscode.window.withProgress({
      location: vscode.ProgressLocation.Notification,
      title: 'ZFJ: Exporting…'
    }, () => new Promise<void>((resolve, reject) => {
      execFile(executable, args, {
        cwd: path.dirname(input),
        windowsHide: true,
        timeout: 30000,
        encoding: 'utf8',
        maxBuffer: 4 * 1024 * 1024
      }, (error, stdout, stderr) => {
        if (stdout) { output.appendLine(stdout.trimEnd()); }
        if (stderr) { output.appendLine(stderr.trimEnd()); }
        if (error) {
          reject(new Error(stderr.trim() || error.message));
        } else {
          resolve();
        }
      });
    }));
    output.appendLine('書き出し完了');
    void vscode.window.showInformationMessage(`書き出しました: ${destination.fsPath}`);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    output.appendLine(`エラー: ${message}`);
    output.show(true);
    void vscode.window.showErrorMessage(`ZFJ: ${message}`);
  } finally {
    exporting = false;
  }
}

// 拡張機能のコマンドが初めて実行される際に呼び出されます。
export function activate(context: vscode.ExtensionContext): void {
  registerLanguageSupport(context);
  const output = vscode.window.createOutputChannel('ZFJ');
  registerPreview(context, output, chooseExecutable);
  registerFileAssociationCommands(context, output, () => chooseExecutable());
  const command = vscode.commands.registerCommand('zfj.helloWorld', () => {
    void vscode.window.showInformationMessage('Hello World! ZFJ拡張機能が動作しています。');
  });

  // 拡張機能の終了時に、コマンドの登録を解除します。
  const exportCommand = vscode.commands.registerCommand('zfj.export', () => exportDrawing(output));
  context.subscriptions.push(command, exportCommand, output);
}
