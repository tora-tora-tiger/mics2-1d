/**
 * API関連の型定義
 */

import { Game } from './game.js';

/** API成功レスポンス */
export interface ApiResponse<T = any> {
  status: 'success';
  data: T;
  timestamp: Date;
}

/** APIエラーレスポンス */
export interface ApiError {
  error: string;
  timestamp: Date;
}

/** 対局一覧レスポンス */
export interface GamesResponse {
  games: Game[];
}

/** 対局情報レスポンス */
export interface GameResponse {
  game: Game;
}

/** ヘルスチェックレスポンス */
export interface HealthResponse {
  status: 'ok';
  timestamp: Date;
}