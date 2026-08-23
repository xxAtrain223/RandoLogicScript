export const nativeTargets = {
  'darwin-arm64': { architecture: 'arm64', cpu: 0x0100000c, executable: 'rls_language_server', format: 'macho' },
  'darwin-x64': { architecture: 'x64', cpu: 0x01000007, executable: 'rls_language_server', format: 'macho' },
  'linux-arm64': { architecture: 'arm64', cpu: 183, executable: 'rls_language_server', format: 'elf' },
  'linux-x64': { architecture: 'x64', cpu: 62, executable: 'rls_language_server', format: 'elf' },
  'win32-arm64': { architecture: 'arm64', cpu: 0xaa64, executable: 'rls_language_server.exe', format: 'pe' },
  'win32-x64': { architecture: 'x64', cpu: 0x8664, executable: 'rls_language_server.exe', format: 'pe' },
};

function assertReadable(image, offset, length, description) {
  if (!Buffer.isBuffer(image) || offset < 0 || image.length < offset + length) {
    throw new Error(`Invalid ${description}: image is too small.`);
  }
}

function validatePe(image, expected, target) {
  assertReadable(image, 0, 0x40, 'PE header');
  if (image.toString('ascii', 0, 2) !== 'MZ') {
    throw new Error('Invalid PE image: missing MZ signature.');
  }

  const peOffset = image.readUInt32LE(0x3c);
  assertReadable(image, peOffset, 6, 'PE signature');
  if (image.toString('ascii', peOffset, peOffset + 4) !== 'PE\0\0') {
    throw new Error('Invalid PE image: missing PE signature.');
  }

  const machine = image.readUInt16LE(peOffset + 4);
  if (machine !== expected.cpu) {
    throw new Error(`PE machine 0x${machine.toString(16)} does not match ${target}.`);
  }

  const dynamicRuntime = image.toString('latin1').match(
    /(?:MSVCP\d+|VCRUNTIME\d+(?:_\d+)?|ucrtbase)d?\.dll/i,
  );
  if (dynamicRuntime) {
    throw new Error(`Server imports dynamic runtime ${dynamicRuntime[0]}.`);
  }
}

function validateElf(image, expected, target) {
  assertReadable(image, 0, 20, 'ELF header');
  if (!image.subarray(0, 4).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46]))) {
    throw new Error('Invalid ELF image: missing ELF signature.');
  }

  const machine = image.readUInt16LE(18);
  if (machine !== expected.cpu) {
    throw new Error(`ELF machine ${machine} does not match ${target}.`);
  }
}

function validateMachO(image, expected, target) {
  assertReadable(image, 0, 8, 'Mach-O header');
  const magic = image.readUInt32LE(0);
  if (magic !== 0xfeedfacf) {
    throw new Error(`Invalid Mach-O image: unexpected magic 0x${magic.toString(16)}.`);
  }

  const cpuType = image.readUInt32LE(4);
  if (cpuType !== expected.cpu) {
    throw new Error(`Mach-O CPU 0x${cpuType.toString(16)} does not match ${target}.`);
  }
}

export function validateNativeImage(image, target) {
  const expected = nativeTargets[target];
  if (!expected) {
    throw new Error(`Unsupported native target ${target}.`);
  }

  if (expected.format === 'pe') {
    validatePe(image, expected, target);
  } else if (expected.format === 'elf') {
    validateElf(image, expected, target);
  } else {
    validateMachO(image, expected, target);
  }
}