import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

import { runTests } from '@vscode/test-electron';

function executableName(): string {
  return process.platform === 'win32' ? 'rls_language_server.exe' : 'rls_language_server';
}

function discoverServer(repositoryRoot: string): string {
  const executable = executableName();
  const candidates = [
    process.env.RLS_LANGUAGE_SERVER_PATH,
    path.join(repositoryRoot, 'build', 'lsp', executable),
    path.join(repositoryRoot, 'build', 'lsp', 'Debug', executable),
    path.join(repositoryRoot, 'build', 'lsp', 'Release', executable),
    path.join(repositoryRoot, 'build-vs', 'lsp', 'Debug', executable),
    path.join(repositoryRoot, 'build-vs', 'lsp', 'Release', executable),
  ].filter((candidate): candidate is string => Boolean(candidate));
  const server = candidates.find((candidate) => fs.existsSync(candidate));
  if (!server) {
    throw new Error('Build rls_language_server or set RLS_LANGUAGE_SERVER_PATH before testing.');
  }
  return server;
}

async function main(): Promise<void> {
  const extensionDevelopmentPath = path.resolve(__dirname, '..', '..');
  const repositoryRoot = path.resolve(extensionDevelopmentPath, '..', '..');
  const extensionTestsPath = path.resolve(__dirname, 'suite', 'index');
  const fixturePath = path.join(extensionDevelopmentPath, 'test-fixture');
  const userDataDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'rls-vscode-'));
  process.env.RLS_LANGUAGE_SERVER_PATH = discoverServer(repositoryRoot);

  try {
    await runTests({
      extensionDevelopmentPath,
      extensionTestsPath,
      launchArgs: [
        fixturePath,
        '--disable-extensions',
        `--user-data-dir=${userDataDirectory}`,
      ],
      extensionTestsEnv: {
        RLS_LANGUAGE_SERVER_PATH: process.env.RLS_LANGUAGE_SERVER_PATH,
      },
    });
  } finally {
    fs.rmSync(userDataDirectory, { recursive: true, force: true });
  }
}

main().catch((error: unknown) => {
  console.error(error);
  process.exit(1);
});