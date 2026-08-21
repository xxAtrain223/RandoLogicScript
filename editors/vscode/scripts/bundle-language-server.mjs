import { copyFileSync, existsSync, mkdirSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join, resolve } from 'node:path';
import { execFileSync } from 'node:child_process';

const extensionDirectory = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repositoryDirectory = resolve(extensionDirectory, '..', '..');
const executableName = process.platform === 'win32'
  ? 'rls_language_server.exe'
  : 'rls_language_server';
const expectedArchitectures = {
  arm64: { elf: 183, macho: 0x0100000c, pe: 0xaa64 },
  x64: { elf: 62, macho: 0x01000007, pe: 0x8664 },
};
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

const expectedArchitecture = expectedArchitectures[process.arch];
if (!expectedArchitecture) {
  throw new Error(`Unsupported release architecture ${process.arch}.`);
}

const image = readFileSync(executable);
if (process.platform === 'win32') {
  const peOffset = image.readUInt32LE(0x3c);
  const machine = image.readUInt16LE(peOffset + 4);
  if (machine !== expectedArchitecture.pe) {
    throw new Error(`Refusing to label PE machine 0x${machine.toString(16)} as ${process.arch}.`);
  }
  const dynamicRuntime = image.toString('latin1').match(
    /(?:MSVCP\d+|VCRUNTIME\d+(?:_\d+)?|ucrtbase)d?\.dll/i,
  );
  if (dynamicRuntime) {
    throw new Error(`Refusing to package a server linked to dynamic runtime ${dynamicRuntime[0]}.`);
  }
} else if (process.platform === 'linux') {
  const machine = image.readUInt16LE(18);
  if (machine !== expectedArchitecture.elf) {
    throw new Error(`Refusing to label ELF machine ${machine} as ${process.arch}.`);
  }
} else if (process.platform === 'darwin') {
  const cpuType = image.readUInt32LE(4);
  if (cpuType !== expectedArchitecture.macho) {
    throw new Error(`Refusing to label Mach-O CPU 0x${cpuType.toString(16)} as ${process.arch}.`);
  }
}

const destinationDirectory = join(
  extensionDirectory,
  'server',
  `${process.platform}-${process.arch}`,
);
mkdirSync(destinationDirectory, { recursive: true });
copyFileSync(executable, join(destinationDirectory, executableName));