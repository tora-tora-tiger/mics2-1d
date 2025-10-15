import { InitialPositionSFEN, Position, Record } from "../tsshogi/src";
import { exportKIF } from "../tsshogi/src/kakinoki9";
import fs from 'node:fs';
import path from 'node:path';

const sfenToKIF = (sfen: string): string | Error => {
  const record = Record.newByUSI(sfen);
  if (record instanceof Error) {
    throw record;
  }
  return exportKIF(record);
}

// debug
const kif = sfenToKIF("position startpos moves 3e2d 3a2b 2d3e 2b1c 1e1c 1b1c S*4b 4a5b 4b5a+ 5b2e+ 3e4d 2e1d 4d3c 1d2c 3c4d B*1b R*2e R*3a 2e2c 1b2c 5a5b R*1e B*3e 1e1d+ 5d5c 2c1b 4d4c 2a2b 4c5d 1b4e+ 5e4e 3a3e+ 4e3e G*3d");
if (kif instanceof Error) {
  throw kif;
}
const dir = './data/record/';

  fs.mkdirSync(dir, { recursive: true });
  const filePath = path.join(dir, `record-${Date.now()}.kif`);
  fs.writeFileSync(filePath, kif);
  console.log(`Record saved to ${filePath}`);