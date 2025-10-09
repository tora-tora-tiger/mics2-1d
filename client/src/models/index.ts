import { z } from '@hono/zod-openapi';
import {
  ApiErrorSchema,
  ApiResponseSchema,
} from './common.js';
import { HealthResponseSchema } from './health.js';
import {
  GameStateSchema,
  GameSchema,
  GamesResponseSchema,
  GameResponseSchema,
  CreateGameRequestSchema,
} from './game.js';
import { EngineResponseSchema } from './engine.js';
import { TagHealthCheck, TagGames } from './tags.js';

// 型エクスポート
export type ApiResponse = z.infer<typeof ApiResponseSchema>;
export type ApiError = z.infer<typeof ApiErrorSchema>;
export type HealthResponse = z.infer<typeof HealthResponseSchema>;
export type Game = z.infer<typeof GameSchema>;
export type GameState = z.infer<typeof GameStateSchema>;
export type GamesResponse = z.infer<typeof GamesResponseSchema>;
export type GameResponse = z.infer<typeof GameResponseSchema>;
export type CreateGameRequest = z.infer<typeof CreateGameRequestSchema>;
export type EngineResponse = z.infer<typeof EngineResponseSchema>;

// スキーマエクスポート
export { ApiResponseSchema, ApiErrorSchema } from './common.js';
export { HealthResponseSchema } from './health.js';
export {
  GameStateSchema,
  GameSchema,
  GamesResponseSchema,
  GameResponseSchema,
  CreateGameRequestSchema,
} from './game.js';
export { EngineResponseSchema } from './engine.js';
export { TagHealthCheck, TagGames } from './tags.js';