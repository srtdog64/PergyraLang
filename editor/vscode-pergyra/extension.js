const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const cp = require('child_process');

const isWindows = process.platform === 'win32';

function unquotePath(value) {
    if (!value) {
        return value;
    }
    const text = String(value).trim();
    if ((text.startsWith('"') && text.endsWith('"'))
        || (text.startsWith("'") && text.endsWith("'"))) {
        return text.slice(1, -1);
    }
    return text;
}

function shouldUseShellForCompiler(candidate) {
    return isWindows && (isScriptLikeCompiler(candidate) || candidate.includes(' '));
}

function isScriptLikeCompiler(candidate) {
    const ext = path.extname(candidate || '').toLowerCase();
    return isWindows && (ext === '.cmd' || ext === '.bat');
}

function resolveExisting(candidate) {
    if (!candidate) {
        return null;
    }
    try {
        return fs.existsSync(candidate) && fs.statSync(candidate).isFile() ? candidate : null;
    } catch {
        return null;
    }
}

function resolveWorkspaceConfigPath(rawPath) {
    if (!rawPath) {
        return null;
    }
    const normalizedPath = unquotePath(rawPath);

    const candidates = [];
    if (path.isAbsolute(normalizedPath)) {
        candidates.push(normalizedPath);
    } else {
        const folders = vscode.workspace.workspaceFolders || [];
        for (const folder of folders) {
            candidates.push(path.join(folder.uri.fsPath, normalizedPath));
        }
    }

    const extensions = isWindows ? ['', '.exe', '.bat', '.cmd', '.com'] : [''];
    for (const candidate of candidates) {
        for (const ext of extensions) {
            const resolved = resolveExisting(candidate + ext);
            if (resolved) {
                return resolved;
            }
        }
    }
    return null;
}

function findInPath(binaryName) {
    const pathEnv = process.env.PATH || '';
    const dirs = pathEnv.split(path.delimiter).filter(Boolean);
    const extCandidates = isWindows
        ? ['', '.exe', '.bat', '.cmd', '.com']
        : [''];

    const candidates = extCandidates.map(ext => `${binaryName}${ext}`);

    for (const dir of dirs) {
        for (const name of candidates) {
            const candidate = path.join(dir, name);
            const resolved = resolveExisting(candidate);
            if (resolved) {
                return resolved;
            }
        }
    }

    return null;
}

/**
 * Find pgy compiler path.
 * Priority: settings > workspace bin/pgy(.exe) > PATH
 */
function findCompiler() {
    const config = vscode.workspace.getConfiguration('pergyra');
    const configPath = config.get('compilerPath');
    const fromConfig = resolveWorkspaceConfigPath(configPath);
    if (fromConfig) {
        return fromConfig;
    }

    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        const exts = isWindows ? ['', '.exe', '.bat', '.cmd', '.com'] : [''];
        const localCandidates = [];
        const basePath = path.join(folder.uri.fsPath, 'bin', 'pgy');
        for (const ext of exts) {
            localCandidates.push(`${basePath}${ext}`);
        }
        for (const candidate of localCandidates) {
            const resolved = resolveExisting(candidate);
            if (resolved) {
                return resolved;
            }
        }
    }

    const fromPath = findInPath('pgy');
    if (fromPath) {
        return fromPath;
    }

    return isWindows ? 'pgy.exe' : 'pgy';
}

function splitFlags(rawFlags) {
    if (!rawFlags) {
        return [];
    }
    const tokens = rawFlags.match(/\"(?:[^\"\\\\]|\\\\.)*\"|'(?:[^'\\\\]|\\\\.)*'|\\S+/g);
    if (!tokens) {
        return [];
    }
    return tokens
        .map((value) => value.trim())
        .filter(Boolean)
        .map((value) => unquotePath(value));
}

let outputChannel;

function getOutputChannel() {
    if (!outputChannel) {
        outputChannel = vscode.window.createOutputChannel('Pergyra');
    }
    return outputChannel;
}

function bindProcessHandlers(proc, channel) {
    const onOutput = (data, target) => {
        target.append(data.toString());
    };

    proc.stdout.on('data', (data) => {
        onOutput(data, channel);
    });

    proc.stderr.on('data', (data) => {
        onOutput(data, channel);
    });

    proc.on('close', (code) => {
        channel.appendLine('---');
        channel.appendLine(`[Pergyra] exit code: ${code}`);
    });
}

function runFile(filePath, flags) {
    const compiler = findCompiler();
    const channel = getOutputChannel();
    channel.clear();
    channel.show(true);
    const compilerLabel = path.basename(compiler);
    const spawnShell = isWindows || shouldUseShellForCompiler(compiler);

    const folders = vscode.workspace.workspaceFolders;
    const cwd = (folders && folders.length > 0) ? folders[0].uri.fsPath : path.dirname(filePath);
    const fileName = path.basename(filePath);
    const args = [filePath, ...splitFlags(flags)];

    channel.appendLine(`[Pergyra] ${compilerLabel} ${fileName} ${args.slice(1).join(' ')}`);
    channel.appendLine('---');

    let attemptedFallback = false;
    let proc = cp.spawn(compiler, args, {
        cwd,
        shell: spawnShell,
        windowsHide: true,
    });
    bindProcessHandlers(proc, channel);

    const handleError = (error) => {
        if (!attemptedFallback
            && isWindows
            && (error.code === 'ENOENT')
            && !compiler.toLowerCase().endsWith('.exe')) {
            const exeCandidate = `${compiler}.exe`;
            if (resolveExisting(exeCandidate)) {
                attemptedFallback = true;
                proc = cp.spawn(exeCandidate, args, {
                    cwd,
                    shell: true,
                    windowsHide: true,
                });
                bindProcessHandlers(proc, channel);
                proc.on('error', handleError);
                return;
            }
        }
        channel.appendLine('---');
        channel.appendLine(`[Pergyra] failed to run compiler '${compiler}': ${error.message}`);
        vscode.window.showErrorMessage(`Pergyra run failed: ${error.message}`);
    };

    proc.on('error', handleError);
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
