#!/usr/bin/env node
// Cut the erase-state padding off the merged image.
//
//   node tools/trim-merged.mjs build/<name>.ino.merged.bin build/<name>.full.bin
//
// arduino-cli's merged image is padded with 0xFF out to the declared flash size,
// so an 8MB Cardputer build ships 8MB for 1.2MB of content. Flash is erased to
// 0xFF anyway and esptool only writes the bytes present, so truncating at the last
// real byte flashes identically -- it just stops making everyone download 7MB of
// nothing. The assert is the guarantee: if anything but padding were being cut,
// this fails instead of shipping a broken image.

import { readFileSync, writeFileSync } from 'node:fs';

const [src, dst] = process.argv.slice(2);
if (!src || !dst) {
  console.error('usage: node tools/trim-merged.mjs <merged.bin> <out.bin>');
  process.exit(1);
}

const data = readFileSync(src);
let end = data.length;
while (end > 0 && data[end - 1] === 0xff) end--;
for (let i = end; i < data.length; i++) {
  if (data[i] !== 0xff) throw new Error(`byte ${i} past the content is not padding`);
}

const aligned = (end + 0xfff) & ~0xfff; // keep 4K flash alignment
writeFileSync(dst, data.subarray(0, aligned));
console.log(
  `${src}: ${data.length} bytes -> ${dst}: ${aligned} bytes ` +
    `(dropped ${data.length - aligned} bytes of 0xFF padding)`,
);
