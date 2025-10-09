import { z } from '@hono/zod-openapi'

/**
 * APIレスポンスの共通スキーマ
 */
export const ApiResponseSchema = z.object({
  status: z.literal('success').openapi({
    example: 'success',
    description: 'レスポンスステータス'
  }),
  data: z.any().openapi({
    description: 'レスポンスデータ'
  }),
  timestamp: z.iso.datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: 'タイムスタンプ'
  })
});

/**
 * エラーレスポンススキーマ
 */
export const ApiErrorSchema = z.object({
  error: z.string().openapi({
    example: 'Error message',
    description: 'エラーメッセージ'
  }),
  timestamp: z.iso.datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: 'タイムスタンプ'
  })
});