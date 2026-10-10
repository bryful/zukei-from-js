import * as vscode from 'vscode';
import * as path from 'node:path';
import { execFile } from 'node:child_process';

export function registerFileAssociationCommands(
  context: vscode.ExtensionContext,
  output: vscode.OutputChannel,
  chooseExecutable: () => Promise<string | undefined>
): void {
  let running = false;
  async function configure(install: boolean): Promise<void> {
    if (running) {
      void vscode.window.showInformationMessage('拡張子の登録・削除を実行中です。');
      return;
    }
    running = true;
    const action = install ? 'Register File Associations' : 'Unregister File Associations';
    try {
      if (process.platform !== 'win32') {
        throw new Error('拡張子の登録・削除はWindowsで利用できます。');
      }
      if (!vscode.workspace.isTrusted) {
        throw new Error('実行するには、ワークスペースを信頼してください。');
      }
      const executable = await chooseExecutable();
      if (!executable) { return; }
      const argument = install ? '--install' : '--uninstall';
      output.appendLine(`\n${action}: ${executable} ${argument}`);
      await vscode.window.withProgress({
        location: vscode.ProgressLocation.Notification,
        title: `ZFJ: ${action}…`
      }, () => new Promise<void>((resolve, reject) => {
        // 入力ファイルを渡さず、現在のユーザー用の登録処理だけを呼び出します。
        execFile(executable, [argument], {
          cwd: path.dirname(executable), windowsHide: true, timeout: 30000,
          encoding: 'utf8', maxBuffer: 4 * 1024 * 1024
        }, (error, stdout, stderr) => {
          if (stdout) { output.appendLine(stdout.trimEnd()); }
          if (stderr) { output.appendLine(stderr.trimEnd()); }
          if (error) { reject(new Error(stderr.trim() || error.message)); }
          else { resolve(); }
        });
      }));
      const message = install
        ? '.sjs・.djsの拡張子とアイコンを現在のユーザーに登録しました。'
        : '現在のユーザーのZFJ拡張子登録を削除しました。';
      output.appendLine(message);
      void vscode.window.showInformationMessage(`ZFJ: ${message}`);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      output.appendLine(`${action}エラー: ${message}`);
      output.show(true);
      void vscode.window.showErrorMessage(`ZFJ: ${message}`);
    } finally {
      running = false;
    }
  }
  context.subscriptions.push(
    vscode.commands.registerCommand('zfj.registerFileAssociations', () => configure(true)),
    vscode.commands.registerCommand('zfj.unregisterFileAssociations', () => configure(false))
  );
}
