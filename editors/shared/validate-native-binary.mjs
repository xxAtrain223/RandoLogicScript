import { readFileSync } from 'node:fs';
import { basename } from 'node:path';

import { nativeTargets, validateNativeImage } from './native-binary-validation.mjs';

const [, , imagePath, target] = process.argv;
if (!imagePath || !target || !nativeTargets[target]) {
  throw new Error('Usage: node validate-native-binary.mjs <image-path> <target>');
}

validateNativeImage(readFileSync(imagePath), target);
console.log(`Validated ${basename(imagePath)} for ${target}.`);