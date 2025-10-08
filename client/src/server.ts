import { Hono } from 'hono';
import { createNodeWebSocket } from '@hono/node-ws';
import { serve } from '@hono/node-server';
import { GameManager } from './gameManager.js';
import { CreateGameRequest, WebSocketMessage } from './types/game.js';

/**
 * 将棋対局APIサーバー
 *
 * Hono + WebSocketで実装された対局管理サーバー
 */
const app = new Hono();
const gameManager = new GameManager();

// WebSocketを初期化
const { injectWebSocket, upgradeWebSocket } = createNodeWebSocket({ app });

// WebSocketコネクション管理
const wsConnections = new Map<string, WebSocket>();

/**
 * WebSocketブロードキャスト
 * @param gameId 対局ID
 * @param message メッセージ
 */
function broadcastToGame(gameId: string, message: WebSocketMessage): void {
  for (const [id, ws] of wsConnections) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(message));
    }
  }
}

// ヘルスチェック
app.get('/health', (c) => {
  return c.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// 対局一覧取得
app.get('/games', (c) => {
  const games = gameManager.getAllGames();
  return c.json({ games });
});

// 対局情報取得
app.get('/games/:gameId', (c) => {
  const gameId = c.req.param('gameId');
  const game = gameManager.getGame(gameId);

  if (!game) {
    return c.json({ error: 'Game not found' }, 404);
  }

  return c.json({ game });
});

// 新規対局作成
app.post('/games', async (c) => {
  try {
    const request: CreateGameRequest = await c.req.json();

    // バリデーション
    if (!request.player) {
      return c.json({ error: 'Player name is required' }, 400);
    }

    const game = gameManager.createGame(request);
    return c.json({ game }, 201);

  } catch (error) {
    return c.json({ error: 'Invalid request' }, 400);
  }
});

// 対局開始
app.post('/games/:gameId/start', async (c) => {
  const gameId = c.req.param('gameId');
  const success = await gameManager.startGame(gameId);

  if (!success) {
    return c.json({ error: 'Failed to start game' }, 400);
  }

  return c.json({ message: 'Game started' });
});

// 対局停止
app.post('/games/:gameId/stop', (c) => {
  const gameId = c.req.param('gameId');
  const success = gameManager.stopGame(gameId);

  if (!success) {
    return c.json({ error: 'Failed to stop game' }, 400);
  }

  return c.json({ message: 'Game stopped' });
});

// 対局終了
app.delete('/games/:gameId', (c) => {
  const gameId = c.req.param('gameId');
  gameManager.endGame(gameId);

  return c.json({ message: 'Game ended' });
});

// WebSocketエンドポイント
app.get(
  '/ws',
  upgradeWebSocket((c) => {
    const connectionId = crypto.randomUUID();
    const ws = c.req.ws();

    wsConnections.set(connectionId, ws);

    console.log(`WebSocket connected: ${connectionId}`);

    ws.on('message', (message) => {
      try {
        const data = JSON.parse(message.toString());

        // WebSocket経由の対局リクエストを処理
        if (data.type === 'create_game') {
          const game = gameManager.createGame(data.data);

          ws.send(JSON.stringify({
            type: 'game_created',
            gameId: game.id,
            data: game,
            timestamp: new Date()
          } as WebSocketMessage));
        }

      } catch (error) {
        console.error('WebSocket message error:', error);
      }
    });

    ws.on('close', () => {
      wsConnections.delete(connectionId);
      console.log(`WebSocket disconnected: ${connectionId}`);
    });

    return new Response();
  })
);

// GameManagerイベントリスナー
gameManager.on('game_created', (game) => {
  const message: WebSocketMessage = {
    type: 'game_created',
    gameId: game.id,
    data: game,
    timestamp: new Date()
  };
  broadcastToGame(game.id, message);
});

gameManager.on('game_started', (game) => {
  const message: WebSocketMessage = {
    type: 'game_state',
    gameId: game.id,
    data: { state: 'started', game },
    timestamp: new Date()
  };
  broadcastToGame(game.id, message);
});

gameManager.on('game_stopped', (game) => {
  const message: WebSocketMessage = {
    type: 'game_state',
    gameId: game.id,
    data: { state: 'stopped', game },
    timestamp: new Date()
  };
  broadcastToGame(game.id, message);
});

gameManager.on('game_ended', (game) => {
  const message: WebSocketMessage = {
    type: 'game_state',
    gameId: game.id,
    data: { state: 'ended', game },
    timestamp: new Date()
  };
  broadcastToGame(game.id, message);
});

gameManager.on('engine_response', (response) => {
  const message: WebSocketMessage = {
    type: 'engine_response',
    gameId: response.gameId,
    data: response,
    timestamp: new Date()
  };
  broadcastToGame(response.gameId, message);
});

gameManager.on('error', (error) => {
  const message: WebSocketMessage = {
    type: 'error',
    gameId: error.gameId,
    data: { error: error.error },
    timestamp: new Date()
  };
  broadcastToGame(error.gameId, message);
});

// サーバー起動
const port = process.env.PORT || 3000;
console.log(`Starting server on port ${port}...`);

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down server...');
  gameManager.terminateAllGames();
  process.exit(0);
});

// HonoサーバーとWebSocketを統合して起動
injectWebSocket(app);
serve({
  fetch: app.fetch,
  port: Number(port),
});

console.log(`Server running on http://localhost:${port}`);
console.log(`WebSocket available at ws://localhost:${port}/ws`);