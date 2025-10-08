import { Hono } from 'hono';
import { GameController } from '../controllers/gameController.js';
import { HealthController } from '../controllers/healthController.js';

/**
 * APIルート定義
 */
export function createApiRoutes(
  gameController: GameController,
  healthController: HealthController
): Hono {
  const app = new Hono();

  // ヘルスチェック
  app.get('/health', (c) => healthController.health(c));

  // 対局関連エンドポイント
  app.get('/games', (c) => gameController.getGames(c));
  app.get('/games/:gameId', (c) => gameController.getGame(c));
  app.post('/games', (c) => gameController.createGame(c));
  app.post('/games/:gameId/start', (c) => gameController.startGame(c));
  app.post('/games/:gameId/stop', (c) => gameController.stopGame(c));
  app.delete('/games/:gameId', (c) => gameController.endGame(c));

  return app;
}