import { z } from '@hono/zod-openapi';

/**
 * 対局状態スキーマ
 */
export const GameStateSchema = z.enum(['waiting', 'playing', 'ended']).openapi({
  description: '対局の状態',
  example: 'waiting'
});

/**
 * 対局情報スキーマ
 */
export const GameSchema = z.object({
  id: z.uuid().openapi({
    example: '123e4567-e89b-12d3-a456-426614174000',
    description: '対局ID'
  }),
  player: z.string().min(1).max(100).openapi({
    example: 'Player1',
    description: 'プレイヤー名'
  }),
  enginePath: z.string().openapi({
    example: '../source/minishogi-by-gcc',
    description: 'エンジンパス'
  }),
  state: GameStateSchema,
  position: z.string().openapi({
    example: 'startpos',
    description: '局面（USI形式）'
  }),
  timeLimit: z.number().min(1000).max(3600000).openapi({
    example: 60000,
    description: '持ち時間（ミリ秒）'
  }),
  byoyomi: z.number().min(1000).max(60000).openapi({
    example: 10000,
    description: '秒読み（ミリ秒）'
  }),
  createdAt: z.string().datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: '作成日時'
  }),
  updatedAt: z.string().datetime().openapi({
    example: '2025-01-01T00:00:00.000Z',
    description: '更新日時'
  })
});

/**
 * 対局一覧レスポンススキーマ
 */
export const GamesResponseSchema = z.object({
  games: z.array(GameSchema).openapi({
    description: '対局リスト'
  })
});

/**
 * 対局単体レスポンススキーマ
 */
export const GameResponseSchema = z.object({
  game: GameSchema.openapi({
    description: '対局情報'
  })
});

/**
 * 対局作成リクエストスキーマ
 */
export const CreateGameRequestSchema = z.object({
  player: z.string().min(1).max(100).openapi({
    example: 'TestPlayer',
    description: 'プレイヤー名（必須、最大100文字）'
  }),
  enginePath: z.string().optional().openapi({
    example: '../source/minishogi-by-gcc',
    description: 'エンジンパス（任意）'
  }),
  position: z.string().optional().openapi({
    example: 'startpos',
    description: '局面（任意、デフォルト: startpos）'
  }),
  timeLimit: z.number().min(1000).max(3600000).optional().openapi({
    example: 60000,
    description: '持ち時間（ms、1-3,600,000）'
  }),
  byoyomi: z.number().min(1000).max(60000).optional().openapi({
    example: 10000,
    description: '秒読み（ms、1-60,000）'
  })
});