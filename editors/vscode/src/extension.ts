import * as fs from 'node:fs';
import * as path from 'node:path';

import * as vscode from 'vscode';
import {
  Executable,
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;
let watcher: vscode.FileSystemWatcher | undefined;

type SectionSnippetIndentation = 'client' | 'server';

function executableName(): string {
  return process.platform === 'win32' ? 'rls_language_server.exe' : 'rls_language_server';
}

function existingExecutable(candidates: string[]): string | undefined {
  return candidates.find((candidate) => {
    try {
      return fs.statSync(candidate).isFile();
    } catch {
      return false;
    }
  });
}

function configuredServerPath(): string | undefined {
  const configured = vscode.workspace
    .getConfiguration('randoLogicScript')
    .get<string>('server.path', '')
    .trim();
  if (!configured) {
    return undefined;
  }
  if (path.isAbsolute(configured)) {
    return configured;
  }
  const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
  return workspaceRoot ? path.resolve(workspaceRoot, configured) : path.resolve(configured);
}

function configuredSectionSnippetIndentation(): SectionSnippetIndentation {
  return vscode.workspace
    .getConfiguration('randoLogicScript')
    .get<SectionSnippetIndentation>('completion.sectionSnippetIndentation', 'client');
}

export function resolveServerExecutable(context: vscode.ExtensionContext): string | undefined {
  const executable = executableName();
  const configured = configuredServerPath();
  if (configured) {
    return existingExecutable([configured]);
  }

  const environmentPath = process.env.RLS_LANGUAGE_SERVER_PATH;
  const workspaceRoots = vscode.workspace.workspaceFolders?.map((folder) => folder.uri.fsPath) ?? [];
  const candidates = [
    environmentPath,
    context.asAbsolutePath(path.join('server', `${process.platform}-${process.arch}`, executable)),
    ...workspaceRoots.flatMap((root) => [
      path.join(root, 'build', 'lsp', executable),
      path.join(root, 'build', 'lsp', 'Debug', executable),
      path.join(root, 'build', 'lsp', 'Release', executable),
      path.join(root, 'build-vs', 'lsp', 'Debug', executable),
      path.join(root, 'build-vs', 'lsp', 'Release', executable),
    ]),
  ].filter((candidate): candidate is string => Boolean(candidate));
  return existingExecutable(candidates);
}

async function startClient(context: vscode.ExtensionContext): Promise<void> {
  const command = resolveServerExecutable(context);
  if (!command) {
    const selection = await vscode.window.showErrorMessage(
      'RLS language server executable was not found. Build it or configure randoLogicScript.server.path.',
      'Open Settings',
    );
    if (selection === 'Open Settings') {
      await vscode.commands.executeCommand(
        'workbench.action.openSettings',
        'randoLogicScript.server.path',
      );
    }
    return;
  }

  const argumentsValue = vscode.workspace
    .getConfiguration('randoLogicScript')
    .get<string[]>('server.arguments', []);
  const executable: Executable = {
    command,
    args: argumentsValue,
    transport: TransportKind.stdio,
    options: {
      cwd: vscode.workspace.workspaceFolders?.[0]?.uri.fsPath,
    },
  };
  const serverOptions: ServerOptions = executable;
  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'rls' },
      { scheme: 'untitled', language: 'rls' },
    ],
    initializationOptions: {
      completion: {
        sectionSnippetIndentation: configuredSectionSnippetIndentation(),
      },
    },
    synchronize: {
      fileEvents: watcher,
    },
  };

  client = new LanguageClient(
    'randoLogicScript',
    'Rando Logic Script Language Server',
    serverOptions,
    clientOptions,
  );
  await client.start();
}

async function stopClient(): Promise<void> {
  const activeClient = client;
  client = undefined;
  await activeClient?.dispose();
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  watcher = vscode.workspace.createFileSystemWatcher('**/{*.rls,rls.json}');
  context.subscriptions.push(watcher);
  context.subscriptions.push(
    vscode.commands.registerCommand('randoLogicScript.restartServer', async () => {
      await stopClient();
      await startClient(context);
    }),
  );
  await startClient(context);
}

export async function deactivate(): Promise<void> {
  await stopClient();
  watcher = undefined;
}