import { copyFileSync, existsSync, mkdirSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join, resolve } from 'node:path';
import { execFileSync } from 'node:child_process';

import {
  nativeTargets,
  validateNativeImage,
} from '../../shared/native-binary-validation.mjs';

const extensionDirectory = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repositoryDirectory = resolve(extensionDirectory, '..', '..');
const executableName = process.platform === 'win32'
  ? 'rls_language_server.exe'
  : 'rls_language_server';
const configuredBuildDirectory = process.env.RLS_VSCODE_RELEASE_BUILD_DIRECTORY;
const buildDirectory = configuredBuildDirectory
  ? resolve(repositoryDirectory, configuredBuildDirectory)
  : join(repositoryDirectory, 'build-vscode-release');

execFileSync(
  'cmake',
  [
    '-S', repositoryDirectory,
    '-B', buildDirectory,
    '-DCMAKE_BUILD_TYPE=Release',
    '-DBUILD_TESTING=OFF',
    '-DRLS_STATIC_MSVC_RUNTIME=ON',
  ],
  { stdio: 'inherit' },
);
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

const target = `${process.platform}-${process.arch}`;
if (!nativeTargets[target]) {
  throw new Error(`Unsupported release target ${target}.`);
}
validateNativeImage(readFileSync(executable), target);

const destinationDirectory = join(
  extensionDirectory,
  'server',
  `${process.platform}-${process.arch}`,
);
mkdirSync(destinationDirectory, { recursive: true });
copyFileSync(executable, join(destinationDirectory, executableName));