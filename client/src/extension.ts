import * as path from 'path';
import { ExtensionContext, window, workspace, commands } from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: ExtensionContext) {
    const serverPath = context.asAbsolutePath(
        path.join('server', process.platform === 'win32' ? 'cco-lsp.exe' : 'cco-lsp')
    );

    const serverOptions: ServerOptions = {
        run: {
            command: serverPath,
            transport: TransportKind.stdio
        },
        debug: {
            command: serverPath,
            args: ['--debug'],
            transport: TransportKind.stdio
        }
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'cco' }],
        synchronize: {
            fileEvents: workspace.createFileSystemWatcher('**/*.cco')
        }
    };

    client = new LanguageClient(
        'cco-language-server',
        'CCO Language Server',
        serverOptions,
        clientOptions
    );

    client.start();

    context.subscriptions.push(
        commands.registerCommand('cco.showSymbols', async () => {
            if (!client) return;
            const uri = window.activeTextEditor?.document.uri.toString();
            if (!uri) return;
            const symbols = await client.sendRequest('textDocument/documentSymbol', {
                textDocument: { uri }
            });
            window.showInformationMessage(`Found symbols: ${JSON.stringify(symbols)}`);
        })
    );
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) return undefined;
    return client.stop();
}
