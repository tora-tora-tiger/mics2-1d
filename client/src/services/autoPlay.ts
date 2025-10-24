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

interface AutoPlayColorConfig {
  engineName: string; // エンジンのパス
  displayName?: string; // 棋譜に表示する名前
}

interface AutoPlayConfig {
  limitStep: number; // 最大手数
  black: AutoPlayColorConfig;
  white: AutoPlayColorConfig;
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
  record.metadata.setStandardMetadata(RecordMetadataKey.BLACK_NAME, config.black.displayName ?? config.black.engineName);
  record.metadata.setStandardMetadata(RecordMetadataKey.WHITE_NAME, config.white.displayName ?? config.white.engineName);
  
  // エンジンの初期化
  const blackClientPath = config.black.engineName === "latest"
    ? `../source/minishogi-by-gcc`
    : `../engines/${config.black.engineName}`;
  const whiteClientPath = config.white.engineName === "latest"
    ? `../source/minishogi-by-gcc`
    : `../engines/${config.white.engineName}`;
  const blackClient = new ShogiEngineClient(blackClientPath);
  const whiteClient = new ShogiEngineClient(whiteClientPath);  
  blackClient.on('engine_response', (event) => {
    console.log(`\x1b[36m[BLACK]\x1b[0m ${event.data}`);
  });
  whiteClient.on('engine_response', (event) => {
    console.log(`\x1b[35m[WHITE]\x1b[0m ${event.data}`);
  });

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
      console.error(`Failed to append move: ${move.toString()}`);
      break;
      // throw new Error(`Failed to append move: ${move.toString()}`);
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

const black: AutoPlayColorConfig = {
  engineName: "latest",
  displayName: "~~並列2~~ハッシュ"
};
const white: AutoPlayColorConfig = {
  // engineName: "latest",
  // displayName: "~~並列2~~ハッシュ"
  engineName: "alphabeta-v2-aarch64",
  // displayName: ""
};
console.time('autoPlay');
Promise.all([
  // autoPlay({
  //   limitStep: 100,
  //   black,
  //   white,
  // }),
  // 自動対局を逆向きに実行（白番先手）
  autoPlay({
    limitStep: 100,
    black: white,
    white: black,
  }),
]).then(records => {
  console.timeEnd('autoPlay');
  const dir = './data/record/';
  fs.mkdirSync(dir, { recursive: true });

  records.forEach((record, i) => {
    const kif = exportKIF(record);
    const filePath = path.join(dir, `record-${Date.now()}-${i}.kif`);
    fs.writeFileSync(filePath, kif);
    console.log(`Record saved to ${filePath}`);
  });
}).catch(error => {
    console.error("An error occurred during auto play:", error);
    console.timeEnd('autoPlay');
});
