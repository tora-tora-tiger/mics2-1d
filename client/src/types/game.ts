/**
 * 対局関連の型定義
 */

/** 対局の状態 */
export type GameState = 'waiting' | 'playing' | 'ended';

/** USIコマンド */
export interface USICommand {
  type: 'usi' | 'isready' | 'usinewgame' | 'position' | 'go' | 'stop' | 'quit';
  params?: string[];
}

/** 対局情報 */
export interface Game {
  id: string;
  player: string;
  enginePath: string;
  state: GameState;
  position: string;
  timeLimit: number;
  byoyomi: number;
  createdAt: Date;
  updatedAt: Date;
}

/** 対局リクエスト */
export interface CreateGameRequest {
  player: string;
  enginePath?: string;
  position?: string;
  timeLimit?: number;
  byoyomi?: number;
}

/** エンジン応答 */
export interface EngineResponse {
  gameId: string;
  command: string;
  response: string;
  timestamp: Date;
}

/** WebSocketメッセージ */
export interface WebSocketMessage {
  type: 'engine_response' | 'game_state' | 'error' | 'game_created';
  gameId: string;
  data: any;
  timestamp: Date;
}