/*
 * Pergyra semantic-squiggle VS Code client (docs/140 slice 4b).
 *
 * Thin client: it runs the pgy-lsp server (the C analyzer is the source of
 * truth) and, for each published diagnostic, reads `data.squiggleClass` and
 * draws a coloured wavy underline. This is what gives the four distinct colours
 * — including violet — that LSP severity alone cannot express (LSP severity has
 * only error/warning/information/hint; the colour is otherwise editor-chosen).
 *
 * Red is left to VS Code's native error squiggle (blocking diagnostics);
 * amber/violet/blue are advisory and decorated here.
 */

import {
  workspace,
  window,
  ExtensionContext,
  TextEditorDecorationType,
  Range,
  Diagnostic,
} from 'vscode';
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

type SquiggleClass = 'amber' | 'violet' | 'blue';

const SQUIGGLE_COLORS: Record<SquiggleClass, string> = {
  amber: '#d7a000',
  violet: '#9b59b6',
  blue: '#3498db',
};

/* Latest diagnostics per document, captured from the LSP publish stream. */
const diagnosticsByUri = new Map<string, Diagnostic[]>();

export function activate(context: ExtensionContext): void {
  const decorations = {} as Record<SquiggleClass, TextEditorDecorationType>;
  (Object.keys(SQUIGGLE_COLORS) as SquiggleClass[]).forEach((cls) => {
    decorations[cls] = window.createTextEditorDecorationType({
      textDecoration: `underline wavy ${SQUIGGLE_COLORS[cls]}`,
    });
    context.subscriptions.push(decorations[cls]);
  });

  const serverPath = workspace
    .getConfiguration('pergyra')
    .get<string>('serverPath', 'pgy-lsp');

  const serverOptions: ServerOptions = {
    run: { command: serverPath, transport: TransportKind.stdio },
    debug: { command: serverPath, transport: TransportKind.stdio },
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'pergyra' }],
    middleware: {
      handleDiagnostics: (uri, diagnostics, next) => {
        diagnosticsByUri.set(uri.toString(), diagnostics);
        next(uri, diagnostics);
        applyDecorations(decorations);
      },
    },
  };

  client = new LanguageClient(
    'pergyra',
    'Pergyra Language Server',
    serverOptions,
    clientOptions
  );

  context.subscriptions.push(
    window.onDidChangeActiveTextEditor(() => applyDecorations(decorations))
  );

  client.start();
}

function applyDecorations(
  decorations: Record<SquiggleClass, TextEditorDecorationType>
): void {
  const editor = window.activeTextEditor;
  if (editor === undefined) {
    return;
  }

  const diagnostics =
    diagnosticsByUri.get(editor.document.uri.toString()) ?? [];
  const buckets: Record<SquiggleClass, Range[]> = {
    amber: [],
    violet: [],
    blue: [],
  };

  for (const diagnostic of diagnostics) {
    /* The LSP `data` object rides along on the converted diagnostic; it is not
     * part of the public vscode.Diagnostic type, so read it defensively. */
    const data = (diagnostic as unknown as { data?: { squiggleClass?: string } })
      .data;
    const cls = data?.squiggleClass;
    if (cls === 'amber' || cls === 'violet' || cls === 'blue') {
      buckets[cls].push(diagnostic.range);
    }
  }

  (Object.keys(buckets) as SquiggleClass[]).forEach((cls) => {
    editor.setDecorations(decorations[cls], buckets[cls]);
  });
}

export function deactivate(): Thenable<void> | undefined {
  return client !== undefined ? client.stop() : undefined;
}
