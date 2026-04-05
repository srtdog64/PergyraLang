const vscode = require('vscode');
const path = require('path');
const fs = require('fs');

/**
 * Find pgy compiler path.
 * Priority: settings > workspace bin/pgy > PATH
 */
function findCompiler() {
    const config = vscode.workspace.getConfiguration('pergyra');
    const configPath = config.get('compilerPath');
    if (configPath && fs.existsSync(configPath)) {
        return configPath;
    }

    // Try workspace root bin/pgy.exe or bin/pgy
    const folders = vscode.workspace.workspaceFolders;
    if (folders) {
        for (const folder of folders) {
            const candidates = [
                path.join(folder.uri.fsPath, 'bin', 'pgy.exe'),
                path.join(folder.uri.fsPath, 'bin', 'pgy'),
            ];
            for (const c of candidates) {
                if (fs.existsSync(c)) {
                    return c;
                }
            }
        }
    }

    // Fallback to PATH
    return 'pgy';
}

let outputChannel;

function getOutputChannel() {
    if (!outputChannel) {
        outputChannel = vscode.window.createOutputChannel('Pergyra');
    }
    return outputChannel;
}

function runFile(filePath, flags) {
    const compiler = findCompiler();
    const channel = getOutputChannel();
    channel.clear();
    channel.show(true);

    const cwd = path.dirname(filePath);
    const fileName = path.basename(filePath);

    channel.appendLine(`[Pergyra] ${compiler} ${fileName} ${flags}`);
    channel.appendLine('---');

    const cp = require('child_process');
    const proc = cp.spawn(compiler, [filePath, ...flags.split(/\s+/).filter(Boolean)], {
        cwd: cwd,
        shell: true,
    });

    proc.stdout.on('data', (data) => {
        channel.append(data.toString());
    });

    proc.stderr.on('data', (data) => {
        channel.append(data.toString());
    });

    proc.on('close', (code) => {
        channel.appendLine('---');
        channel.appendLine(`[Pergyra] exit code: ${code}`);
    });
}

function activate(context) {
    // Run command (play button)
    const runCmd = vscode.commands.registerCommand('pergyra.run', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'pergyra') {
            vscode.window.showWarningMessage('Open a .pgy file first.');
            return;
        }

        // Save before run
        editor.document.save().then(() => {
            const config = vscode.workspace.getConfiguration('pergyra');
            const flags = config.get('runFlags') || '--run';
            runFile(editor.document.fileName, flags);
        });
    });

    // Build command
    const buildCmd = vscode.commands.registerCommand('pergyra.build', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'pergyra') {
            vscode.window.showWarningMessage('Open a .pgy file first.');
            return;
        }

        editor.document.save().then(() => {
            runFile(editor.document.fileName, '--compile');
        });
    });

    context.subscriptions.push(runCmd, buildCmd);
}

function deactivate() {
    if (outputChannel) {
        outputChannel.dispose();
    }
}

module.exports = { activate, deactivate };
