import { Hono } from 'hono';
import { GameController } from '../controllers/gameController.js';
import { HealthController } from '../controllers/healthController.js';
import { upgradeWebSocket } from '@hono/node-ws';

/**
 * APIルート定義
 */
export function createApiRoutes(
  gameController: GameController,
  healthController: HealthController,
  wsCallback: (ws: WebSocket) => void
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

  // WebSocketエンドポイント
  app.get('/ws', upgradeWebSocket(() => ({
    onOpen(event, ws) {
      wsCallback(ws);
    },
    onMessage(event, ws) {
      // WebSocketメッセージはWebSocketServiceで処理
    },
    onClose(event, ws) {
      // 接続終了処理はWebSocketServiceで管理
    }
  })));

  return app;
}