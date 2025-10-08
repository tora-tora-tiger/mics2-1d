/**
 * WebSocket関連の型定義
 */

/** WebSocketメッセージタイプ */
export type WebSocketMessageType =
  | 'engine_response'
  | 'game_state'
  | 'error'
  | 'game_created'
  | 'game_started'
  | 'game_stopped'
  | 'game_ended';

/** WebSocketメッセージ */
export interface WebSocketMessage {
  type: WebSocketMessageType;
  gameId: string;
  data: any;
  timestamp: Date;
}

/** WebSocket経由の対局作成リクエスト */
export interface WSCreateGameRequest {
  type: 'create_game';
  data: {
    player: string;
    enginePath?: string;
    position?: string;
    timeLimit?: number;
    byoyomi?: number;
  };
}

/** WebSocket接続情報 */
export interface WSConnection {
  id: string;
  ws: WebSocket;
  createdAt: Date;
  lastActivity: Date;
}