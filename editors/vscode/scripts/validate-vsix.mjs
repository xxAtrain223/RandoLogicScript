import { readFile } from 'node:fs/promises';
import { basename } from 'node:path';

import JSZip from 'jszip';

import {
  nativeTargets,
  validateNativeImage,
} from '../../shared/native-binary-validation.mjs';

const [, , vsixPath, target] = process.argv;
if (!vsixPath || !target || !nativeTargets[target]) {
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
const expectedServer = `extension/server/${target}/${nativeTargets[target].executable}`;
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
validateNativeImage(image, target);

console.log(`Validated ${basename(vsixPath)} for ${target}.`);