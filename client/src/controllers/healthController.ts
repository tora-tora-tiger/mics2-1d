import { Context } from 'hono';
import { HealthResponse } from '../types/api.js';

/**
 * ヘルスチェックコントローラー
 */
export class HealthController {
  /**
   * ヘルスチェック
   */
  health(c: Context): Response {
    const response: HealthResponse = {
      status: 'ok',
      timestamp: new Date()
    };
    return c.json(response);
  }
}