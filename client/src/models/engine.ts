import { z } from '@hono/zod-openapi';

/**
 * エンジン応答スキーマ
 */
export const EngineResponseSchema = z.object({
  gameId: z.uuid().openapi({
    example: '123e4567-e89b-12d3-a456-426614174000',
    description: '対局ID'
  }),
  command: z.string().openapi({
    example: 'go btime 60000 wtime 60000',
    description: 'エンジンコマンド'
  }),
  response: z.string().openapi({
    example: 'bestmove 7g7f',
    description: 'エンジン応答'
  }),
  timestamp: z.iso.datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: 'タイムスタンプ'
  })
});