const fs = require('node:fs');

const cmake = fs.readFileSync('CMakeLists.txt', 'utf8');
const manifest = fs.readFileSync('editors/visualstudio/src/source.extension.vsixmanifest', 'utf8');
const vscodeVersion = require('../editors/vscode/package.json').version;
const cmakeVersion = cmake.match(/project\s*\(\s*RandoLogicScript\s+VERSION\s+([^\s)]+)/i)?.[1];
const visualStudioVersion = manifest.match(/<Identity\b[^>]*\bVersion="([^"]+)"/i)?.[1];

if (!cmakeVersion || !visualStudioVersion) {
  throw new Error('Could not read every release version.');
}

if (cmakeVersion !== vscodeVersion || visualStudioVersion !== vscodeVersion) {
  throw new Error(
    `Release versions differ: CMake=${cmakeVersion}, VS Code=${vscodeVersion}, Visual Studio=${visualStudioVersion}.`,
  );
}

const tag = process.argv[2];
if (tag && tag !== `v${vscodeVersion}`) {
  throw new Error(`Tag ${tag} does not match release version ${vscodeVersion}.`);
}