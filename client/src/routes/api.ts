import { OpenAPIHono } from '@hono/zod-openapi';
import { swaggerUI } from '@hono/swagger-ui';
import { logger } from 'hono/logger'
import { GameController } from '../controllers/gameController.js';
import { health } from '../controllers/healthController.js';


/**
 * APIルート定義
 */
export function createApiRoutes(gameController: GameController) {
  const app = new OpenAPIHono();
  app.use(logger());

  // OpenAPIの基本情報設定
  app.doc("/specification", {
    openapi: "3.1.1",
    info: {
      title: "Vector API",
      version: "1.0.0",
      description: "Vector API for Rapid Answer",
    },
  });

app.get(
  "/docs",
  swaggerUI({
    url: "/specification", // OpenAPIドキュメントのURL
  }),
);

  // ヘルスチェックエンドポイント
  app.route('/health', health);

  // 対局関連エンドポイント
  app.get('/games', (c) => gameController.getGames(c));
  app.get('/games/:gameId', (c) => gameController.getGame(c));
  app.post('/games', (c) => gameController.createGame(c));
  app.post('/games/:gameId/start', (c) => gameController.startGame(c));
  app.post('/games/:gameId/stop', (c) => gameController.stopGame(c));
  app.delete('/games/:gameId', (c) => gameController.endGame(c));

  return app;
}