import ShogiEngineClient from "./shogiEngineClient";
import fs from 'node:fs';
import path from 'node:path';
import {
  InitialPositionSFEN,
  Position,
  Record,
} from "../tsshogi/src";

import { exportKIF } from "../tsshogi/src/kakinoki9";

const autoPlay = async (limitStep: number): Promise<Record> => {
  const client = new ShogiEngineClient("../source/minishogi-by-gcc");

  const initialPosition = new Position();
  initialPosition.resetBySFEN(InitialPositionSFEN.STANDARD);
  
  const record = new Record(initialPosition);
  
  await client.usi();
  await client.isReady();
  await client.usinewgame();
  
  for(let i = 0 ; i < limitStep ; i++) {
    // 現在の局面をエンジンに送信
    const movesUSI = record.getUSI();
    // position <pos|sfen> ~~ コマンドで、はじめのposition部分を消して送信
    const positionCommand = movesUSI.substring("position ".length);
    await client.position(positionCommand);

    // 最善手を取得
    const goResponses = await client.go("");

    // `bestmove <move> ponder <ponder>`の形式で返される
    const bestMoveUSI = goResponses.split(' ')[1];
    const move = record.position.createMoveByUSI(bestMoveUSI);
    if (!move) throw new Error(`Invalid usi: ${bestMoveUSI}`);

    // 棋譜に追加
    const isVaildAppend = record.append(move);
    if(!isVaildAppend) {
      throw new Error(`Failed to append move: ${move.toString()}`);
    }
  }

  await client.quit();

  return record;
};

// debug
// autoPlay(100).then(record => {
//   const kif = exportKIF(record);
//   const dir = './data/record/';

//   fs.mkdirSync(dir, { recursive: true });
//   const filePath = path.join(dir, `record-${Date.now()}.kif`);
//   fs.writeFileSync(filePath, kif);
//   console.log(`Record saved to ${filePath}`);
// })
