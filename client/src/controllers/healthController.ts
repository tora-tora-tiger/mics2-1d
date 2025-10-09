import { createRoute, OpenAPIHono, z, type RouteHandler } from '@hono/zod-openapi';
import { HealthResponseSchema, TagHealthCheck } from '../models/index.js';

/**
 * ヘルスチェックエンドポイント定義
 */
const healthRoute = createRoute({
  method: 'get',
  path: '/health',
  tags: [TagHealthCheck.name],
  summary: 'ヘルスチェック',
  description: 'APIサーバーの稼働状態を確認します',
  responses: {
    200: {
      description: 'サーバーが正常に稼働中',
      content: {
        'application/json': {
          schema: HealthResponseSchema
        }
      }
    }
  },
});

/**
 * ヘルスチェック処理
 */
const healthHandler: RouteHandler<typeof healthRoute> = async (c) => {
  const startTime = process.uptime();
  
  const response = {
    status: 'ok' as const,
    timestamp: new Date().toISOString(),
    uptime: Math.floor(startTime),
    version: '1.0.0'
  };
  
  return c.json(response);
}

export const health = new OpenAPIHono().openapi(healthRoute, healthHandler);