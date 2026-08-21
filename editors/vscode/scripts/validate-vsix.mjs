import { readFile } from 'node:fs/promises';
import { basename } from 'node:path';

import JSZip from 'jszip';

const [, , vsixPath, target] = process.argv;
const targets = {
  'darwin-arm64': { cpu: 0x0100000c, executable: 'rls_language_server', format: 'macho' },
  'darwin-x64': { cpu: 0x01000007, executable: 'rls_language_server', format: 'macho' },
  'linux-arm64': { cpu: 183, executable: 'rls_language_server', format: 'elf' },
  'linux-x64': { cpu: 62, executable: 'rls_language_server', format: 'elf' },
  'win32-arm64': { cpu: 0xaa64, executable: 'rls_language_server.exe', format: 'pe' },
  'win32-x64': { cpu: 0x8664, executable: 'rls_language_server.exe', format: 'pe' },
};

if (!vsixPath || !target || !targets[target]) {
  throw new Error('Usage: node scripts/validate-vsix.mjs <vsix-path> <target>');
}

const archive = await JSZip.loadAsync(await readFile(vsixPath), { checkCRC32: true });
const manifest = await archive.file('extension.vsixmanifest')?.async('string');
if (!manifest?.includes(`TargetPlatform="${target}"`)) {
  throw new Error(`VSIX manifest does not target ${target}.`);
}

const packageJson = JSON.parse(
  await archive.file('extension/package.json')?.async('string') ?? 'null',
);
if (!packageJson || packageJson.engines?.vscode !== '^1.82.0') {
  throw new Error('VSIX package manifest has an unexpected VS Code engine requirement.');
}

const serverFiles = Object.values(archive.files)
  .filter((entry) => !entry.dir && entry.name.startsWith('extension/server/'))
  .map((entry) => entry.name);
const expectedServer = `extension/server/${target}/${targets[target].executable}`;
if (serverFiles.length !== 1 || serverFiles[0] !== expectedServer) {
  throw new Error(`Expected only ${expectedServer}; found ${serverFiles.join(', ') || 'none'}.`);
}

const serverEntry = archive.file(expectedServer);
if (!serverEntry) {
  throw new Error(`VSIX is missing ${expectedServer}.`);
}
if (!target.startsWith('win32-')) {
  const permissions = serverEntry.unixPermissions;
  if (typeof permissions !== 'number' || (permissions & 0o111) === 0) {
    throw new Error(`${expectedServer} is not marked executable.`);
  }
}

const image = await serverEntry.async('nodebuffer');
const expected = targets[target];
if (expected.format === 'pe') {
  const peOffset = image.readUInt32LE(0x3c);
  const machine = image.readUInt16LE(peOffset + 4);
  if (machine !== expected.cpu) {
    throw new Error(`PE machine 0x${machine.toString(16)} does not match ${target}.`);
  }
  const dynamicRuntime = image.toString('latin1').match(
    /(?:MSVCP\d+|VCRUNTIME\d+(?:_\d+)?|ucrtbase)d?\.dll/i,
  );
  if (dynamicRuntime) {
    throw new Error(`Packaged server imports dynamic runtime ${dynamicRuntime[0]}.`);
  }
} else if (expected.format === 'elf') {
  const machine = image.readUInt16LE(18);
  if (machine !== expected.cpu) {
    throw new Error(`ELF machine ${machine} does not match ${target}.`);
  }
} else {
  const cpuType = image.readUInt32LE(4);
  if (cpuType !== expected.cpu) {
    throw new Error(`Mach-O CPU 0x${cpuType.toString(16)} does not match ${target}.`);
  }
}

console.log(`Validated ${basename(vsixPath)} for ${target}.`);