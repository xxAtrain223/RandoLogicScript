import { copyFileSync, existsSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join, resolve } from 'node:path';
import { execFileSync } from 'node:child_process';

const extensionDirectory = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repositoryDirectory = resolve(extensionDirectory, '..', '..');
const executableName = process.platform === 'win32'
  ? 'rls_language_server.exe'
  : 'rls_language_server';
const buildDirectories = ['build', 'build-vs']
  .map((directory) => join(repositoryDirectory, directory))
  .filter(existsSync);

if (buildDirectories.length === 0) {
  throw new Error('No CMake build directory was found. Configure the project before bundling the language server.');
}

const buildDirectory = buildDirectories[0];
execFileSync(
  'cmake',
  ['--build', buildDirectory, '--config', 'Release', '--target', 'rls_language_server'],
  { stdio: 'inherit' },
);

const executableCandidates = [
  join(buildDirectory, 'lsp', 'Release', executableName),
  join(buildDirectory, 'lsp', executableName),
  join(buildDirectory, 'lsp', 'Debug', executableName),
];
const executable = executableCandidates.find(existsSync);

if (!executable) {
  throw new Error(`The CMake build completed but did not produce ${executableName}.`);
}

const destinationDirectory = join(
  extensionDirectory,
  'server',
  `${process.platform}-${process.arch}`,
);
mkdirSync(destinationDirectory, { recursive: true });
copyFileSync(executable, join(destinationDirectory, executableName));