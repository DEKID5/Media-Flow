// Generate a proper 256x256 PNG icon for electron-builder
// The current build/icon.png is actually a JPEG (FF D8 FF E0 header)
// electron-builder requires a real PNG file (at least 256x256)

import fs from 'fs';
import zlib from 'zlib';

const width = 256;
const height = 256;

function crc32(buf) {
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i];
    for (let j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >>> 1) ^ 0xEDB88320;
      } else {
        crc = crc >>> 1;
      }
    }
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function createChunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const typeBuffer = Buffer.from(type, 'ascii');
  const crcData = Buffer.concat([typeBuffer, data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(crcData), 0);
  return Buffer.concat([length, typeBuffer, data, crc]);
}

// PNG signature
const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

// IHDR chunk - RGBA (color type 6)
const ihdrData = Buffer.alloc(13);
ihdrData.writeUInt32BE(width, 0);
ihdrData.writeUInt32BE(height, 4);
ihdrData.writeUInt8(8, 8);    // bit depth
ihdrData.writeUInt8(6, 9);    // color type 6 = RGBA
ihdrData.writeUInt8(0, 10);   // compression
ihdrData.writeUInt8(0, 11);   // filter
ihdrData.writeUInt8(0, 12);   // interlace
const ihdr = createChunk('IHDR', ihdrData);

// Create pixel data - a professional-looking gradient icon
const rawData = Buffer.alloc(height * (1 + width * 4)); // filter byte + RGBA
const cx = width / 2, cy = height / 2, maxR = width / 2;

for (let y = 0; y < height; y++) {
  const rowOffset = y * (1 + width * 4);
  rawData[rowOffset] = 0; // No filter
  for (let x = 0; x < width; x++) {
    const pixelOffset = rowOffset + 1 + x * 4;
    const dx = x - cx, dy = y - cy;
    const dist = Math.sqrt(dx * dx + dy * dy);
    
    if (dist < maxR - 2) {
      // Inside circle - gradient from deep purple to teal
      const t = dist / maxR;
      const angle = Math.atan2(dy, dx) / Math.PI;
      rawData[pixelOffset]     = Math.floor(40 + 60 * t + 30 * angle);   // R
      rawData[pixelOffset + 1] = Math.floor(60 + 120 * (1 - t));         // G
      rawData[pixelOffset + 2] = Math.floor(180 + 50 * Math.abs(angle)); // B
      rawData[pixelOffset + 3] = 255;                                     // A
    } else if (dist < maxR) {
      // Anti-aliased edge
      const alpha = Math.max(0, Math.min(255, Math.floor((maxR - dist) * 127)));
      rawData[pixelOffset]     = 60;
      rawData[pixelOffset + 1] = 120;
      rawData[pixelOffset + 2] = 200;
      rawData[pixelOffset + 3] = alpha;
    } else {
      // Outside - transparent
      rawData[pixelOffset]     = 0;
      rawData[pixelOffset + 1] = 0;
      rawData[pixelOffset + 2] = 0;
      rawData[pixelOffset + 3] = 0;
    }
  }
}

const compressed = zlib.deflateSync(rawData);
const idat = createChunk('IDAT', compressed);
const iend = createChunk('IEND', Buffer.alloc(0));
const png = Buffer.concat([signature, ihdr, idat, iend]);

fs.writeFileSync('build/icon.png', png);

// Verify
const header = [...png.slice(0, 8)].map(b => b.toString(16).padStart(2, '0')).join(' ');
const w = png.readUInt32BE(16);
const h = png.readUInt32BE(20);
console.log(`Created ${w}x${h} PNG icon (${png.length} bytes)`);
console.log(`Header: ${header}`);
console.log(`File type: ${header.startsWith('89 50 4e 47') ? 'Valid PNG' : 'NOT PNG!'}`);
