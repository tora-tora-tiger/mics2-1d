import { z } from '@hono/zod-openapi';

/**
 * ヘルスチェックレスポンススキーマ
 */
export const HealthResponseSchema = z.object({
  status: z.literal('ok').openapi({
    example: 'ok',
    description: 'サーバーステータス'
  }),
  timestamp: z.iso.datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: 'タイムスタンプ'
  }),
  uptime: z.number().openapi({
    example: 3600,
    description: 'サーバー稼働時間（秒）'
  }),
  version: z.string().openapi({
    example: '1.0.0',
    description: 'APIバージョン'
  })
});
