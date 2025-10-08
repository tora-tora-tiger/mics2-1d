import { Hono } from 'hono';
import { createNodeWebSocket } from '@hono/node-ws';
import { serve } from '@hono/node-server';
import { GameManager } from './services/gameManager.js';
import { WebSocketService } from './services/websocketService.js';
import { GameController } from './controllers/gameController.js';
import { HealthController } from './controllers/healthController.js';
import { createApiRoutes } from './routes/api.js';
import { logInfo, logError } from './utils/logger.js';
import { config } from './utils/config.js';

/**
 * アプリケーションクラス
 *
 * APIサーバーのエントリーポイント
 */
export class App {
  private app: Hono;
  private gameManager: GameManager;
  private wsService: WebSocketService;
  private gameController: GameController;
  private healthController: HealthController;

  constructor() {
    this.app = new Hono();
    this.gameManager = new GameManager();
    this.wsService = new WebSocketService(this.gameManager);
    this.gameController = new GameController(this.gameManager);
    this.healthController = new HealthController();

    this.setupRoutes();
    this.setupErrorHandling();
    this.setupGracefulShutdown();
  }

  /**
   * ルートを設定
   */
  private setupRoutes(): void {
    // WebSocket統合
    const { injectWebSocket } = createNodeWebSocket({ app: this.app });

    // APIルートを設定
    const apiRoutes = createApiRoutes(
      this.gameController,
      this.healthController,
      (ws) => this.handleWebSocketConnection(ws)
    );

    this.app.route('/', apiRoutes);

    // WebSocketを統合
    injectWebSocket(this.app);
  }

  /**
   * エラーハンドリングを設定
   */
  private setupErrorHandling(): void {
    this.app.onError((err, c) => {
      logError('Unhandled error in application', { error: err.message, stack: err.stack });
      return c.json(
        {
          error: 'Internal server error',
          timestamp: new Date()
        },
        500
      );
    });

    this.app.notFound((c) => {
      return c.json(
        {
          error: 'Endpoint not found',
          timestamp: new Date()
        },
        404
      );
    });
  }

  /**
   * Graceful shutdownを設定
   */
  private setupGracefulShutdown(): void {
    process.on('SIGINT', () => {
      logInfo('Received SIGINT, shutting down gracefully...');
      this.shutdown();
    });

    process.on('SIGTERM', () => {
      logInfo('Received SIGTERM, shutting down gracefully...');
      this.shutdown();
    });
  }

  /**
   * WebSocket接続を処理
   * @param ws WebSocket接続
   */
  private handleWebSocketConnection(ws: WebSocket): void {
    const connectionId = this.wsService.addConnection(ws);
    logInfo('WebSocket connection established', { connectionId });
  }

  /**
   * アプリケーションを起動
   */
  public async start(): Promise<void> {
    try {
      logInfo(`Starting server on port ${config.port}...`);

      await new Promise<void>((resolve) => {
        serve({
          fetch: this.app.fetch,
          port: config.port,
        }, () => {
          logInfo(`Server running on http://localhost:${config.port}`);
          logInfo(`WebSocket available at ws://localhost:${config.port}/ws`);
          resolve();
        });
      });

    } catch (error) {
      logError('Failed to start server', { error });
      process.exit(1);
    }
  }

  /**
   * アプリケーションをシャットダウン
   */
  private shutdown(): void {
    logInfo('Shutting down server...');

    // すべての対局を終了
    this.gameManager.terminateAllGames();

    // すべてのWebSocket接続を終了
    this.wsService.closeAllConnections();

    process.exit(0);
  }

  /**
   * Honoアプリインスタンスを取得
   */
  public getApp(): Hono {
    return this.app;
  }
}