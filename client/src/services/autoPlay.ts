import ShogiEngineClient from "./shogiEngineClient";
import fs from 'node:fs';
import path from 'node:path';
import {
  InitialPositionSFEN,
  Position,
  Record,
  RecordMetadataKey,
  SpecialMoveType,
} from "../tsshogi/src";

import { exportKIF } from "../tsshogi/src/kakinoki9";

const initGame = async (client: ShogiEngineClient) => {
  await client.usi();
  await client.isReady();
  await client.usinewgame();
}

interface AutoPlayConfig {
  limitStep: number; // 最大手数
  blackEngineName: string; // 先手エンジンのパス
  whiteEngineName: string; // 後手エンジンのパス
}

/**
 * 自動対局を実行
 * @param AutoPlayConfig config 自動対局の設定
 * @returns {Promise<Record>} 棋譜オブジェクト
 */
const autoPlay = async (config: AutoPlayConfig): Promise<Record> => {
  // 棋譜の初期化
  const initialPosition = new Position();
  initialPosition.resetBySFEN(InitialPositionSFEN.STANDARD);
  const record = new Record(initialPosition);

  // 棋譜にプレイヤー情報を設定
  record.metadata.setStandardMetadata(RecordMetadataKey.BLACK_NAME, config.blackEngineName);
  record.metadata.setStandardMetadata(RecordMetadataKey.WHITE_NAME, config.whiteEngineName);
  
  // エンジンの初期化
  const blackClient = new ShogiEngineClient(`../engines/${config.blackEngineName}`);
  const whiteClient = new ShogiEngineClient(`../engines/${config.whiteEngineName}`);
  await Promise.all([
    initGame(blackClient),
    initGame(whiteClient)
  ]);
  console.log("Engines initialized");
  
  // 交互に指す
  const clients = [blackClient, whiteClient];
  for(let i = 0 ; i < config.limitStep ; i++) {
    const client = clients[i % 2];

    // 現在の局面をエンジンに送信
    const movesUSI = record.getUSI();
    // position <pos|sfen> ~~ コマンドで、はじめのposition部分を消して送信
    const positionCommand = movesUSI.substring("position ".length);
    await client.position(positionCommand);

    // 最善手を取得
    const startTime = performance.now();
    const goResponses = await client.go("");
    const thinkTime = performance.now() - startTime;

    // `bestmove <move> ponder <ponder>`の形式で返される
    const bestMoveUSI = goResponses.split(' ')[1];
    if(bestMoveUSI.includes("resign")) {
      record.append(SpecialMoveType.RESIGN);
      break;
    }
    const move = record.position.createMoveByUSI(bestMoveUSI);
    if (!move) throw new Error(`Invalid usi: ${bestMoveUSI}`);

    // 棋譜に追加
    const isVaildAppend = record.append(move);
    if(!isVaildAppend) {
      throw new Error(`Failed to append move: ${move.toString()}`);
    }
    record.current.setElapsedMs(thinkTime);
  }

  await Promise.all([
    blackClient.quit(),
    whiteClient.quit()
  ]);

  return record;
};

// debug
console.time('autoPlay');
autoPlay({
  limitStep: 100,
  blackEngineName: "random-player-aarch64",
  whiteEngineName: "random-player-aarch64",
  // whiteEngineName: "alphabeta-aarch64",
}).then(record => {
  console.timeEnd('autoPlay');
  const kif = exportKIF(record);
  const dir = './data/record/';

  fs.mkdirSync(dir, { recursive: true });
  const filePath = path.join(dir, `record-${Date.now()}.kif`);
  fs.writeFileSync(filePath, kif);
  console.log(`Record saved to ${filePath}`);
})
