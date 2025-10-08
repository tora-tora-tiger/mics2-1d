import { GameManager } from './gameManager.js';
import { WebSocketMessage, WSConnection, WSCreateGameRequest } from '../types/websocket.js';

/**
 * WebSocketサービス
 *
 * WebSocket接続とメッセージングを管理
 */
export class WebSocketService {
  private connections: Map<string, WSConnection> = new Map();

  constructor(private gameManager: GameManager) {
    this.setupGameManagerListeners();
  }

  /**
   * 新しいWebSocket接続を追加
   * @param ws WebSocket接続
   * @returns 接続ID
   */
  addConnection(ws: WebSocket): string {
    const connectionId = crypto.randomUUID();
    const connection: WSConnection = {
      id: connectionId,
      ws,
      createdAt: new Date(),
      lastActivity: new Date()
    };

    this.connections.set(connectionId, connection);

    // WebSocketイベントを設定
    this.setupWebSocketListeners(connectionId, ws);

    return connectionId;
  }

  /**
   * WebSocket接続を削除
   * @param connectionId 接続ID
   */
  removeConnection(connectionId: string): void {
    this.connections.delete(connectionId);
  }

  /**
   * 特定の対局に関連するすべての接続にメッセージをブロードキャスト
   * @param gameId 対局ID
   * @param message メッセージ
   */
  broadcastToGame(gameId: string, message: WebSocketMessage): void {
    const messageString = JSON.stringify(message);

    for (const [connectionId, connection] of this.connections) {
      if (connection.ws.readyState === WebSocket.OPEN) {
        try {
          connection.ws.send(messageString);
          connection.lastActivity = new Date();
        } catch (error) {
          console.error(`Failed to send message to connection ${connectionId}:`, error);
          this.removeConnection(connectionId);
        }
      }
    }
  }

  /**
   * すべての接続にメッセージをブロードキャスト
   * @param message メッセージ
   */
  broadcastToAll(message: WebSocketMessage): void {
    const messageString = JSON.stringify(message);

    for (const [connectionId, connection] of this.connections) {
      if (connection.ws.readyState === WebSocket.OPEN) {
        try {
          connection.ws.send(messageString);
          connection.lastActivity = new Date();
        } catch (error) {
          console.error(`Failed to send message to connection ${connectionId}:`, error);
          this.removeConnection(connectionId);
        }
      }
    }
  }

  /**
   * アクティブな接続数を取得
   * @returns 接続数
   */
  getActiveConnectionsCount(): number {
    return Array.from(this.connections.values()).filter(
      connection => connection.ws.readyState === WebSocket.OPEN
    ).length;
  }

  /**
   * WebSocketリスナーを設定
   * @param connectionId 接続ID
   * @param ws WebSocket接続
   */
  private setupWebSocketListeners(connectionId: string, ws: WebSocket): void {
    ws.on('message', (data) => {
      this.handleMessage(connectionId, data);
    });

    ws.on('close', () => {
      this.removeConnection(connectionId);
    });

    ws.on('error', (error) => {
      console.error(`WebSocket error for connection ${connectionId}:`, error);
      this.removeConnection(connectionId);
    });
  }

  /**
   * 受信メッセージを処理
   * @param connectionId 接続ID
   * @param data メッセージデータ
   */
  private handleMessage(connectionId: string, data: Buffer): void {
    try {
      const message = JSON.parse(data.toString());
      const connection = this.connections.get(connectionId);

      if (connection) {
        connection.lastActivity = new Date();
      }

      // 対局作成リクエストを処理
      if (message.type === 'create_game') {
        this.handleCreateGameRequest(connectionId, message as WSCreateGameRequest);
      }

    } catch (error) {
      console.error(`Failed to parse message from connection ${connectionId}:`, error);

      // エラーメッセージを送信
      const errorMessage: WebSocketMessage = {
        type: 'error',
        gameId: '',
        data: { error: 'Invalid message format' },
        timestamp: new Date()
      };

      this.broadcastToGame('', errorMessage);
    }
  }

  /**
   * 対局作成リクエストを処理
   * @param connectionId 接続ID
   * @param request 対局作成リクエスト
   */
  private handleCreateGameRequest(connectionId: string, request: WSCreateGameRequest): void {
    try {
      const game = this.gameManager.createGame(request.data);

      // 成功応答を送信
      const responseMessage: WebSocketMessage = {
        type: 'game_created',
        gameId: game.id,
        data: game,
        timestamp: new Date()
      };

      this.broadcastToGame(game.id, responseMessage);

    } catch (error) {
      console.error(`Failed to create game from WebSocket request:`, error);

      // エラーメッセージを送信
      const errorMessage: WebSocketMessage = {
        type: 'error',
        gameId: '',
        data: { error: 'Failed to create game' },
        timestamp: new Date()
      };

      this.broadcastToGame('', errorMessage);
    }
  }

  /**
   * GameManagerのイベントリスナーを設定
   */
  private setupGameManagerListeners(): void {
    this.gameManager.on('game_created', (game) => {
      const message: WebSocketMessage = {
        type: 'game_created',
        gameId: game.id,
        data: game,
        timestamp: new Date()
      };
      this.broadcastToGame(game.id, message);
    });

    this.gameManager.on('game_started', (game) => {
      const message: WebSocketMessage = {
        type: 'game_started',
        gameId: game.id,
        data: { state: 'started', game },
        timestamp: new Date()
      };
      this.broadcastToGame(game.id, message);
    });

    this.gameManager.on('game_stopped', (game) => {
      const message: WebSocketMessage = {
        type: 'game_stopped',
        gameId: game.id,
        data: { state: 'stopped', game },
        timestamp: new Date()
      };
      this.broadcastToGame(game.id, message);
    });

    this.gameManager.on('game_ended', (game) => {
      const message: WebSocketMessage = {
        type: 'game_ended',
        gameId: game.id,
        data: { state: 'ended', game },
        timestamp: new Date()
      };
      this.broadcastToGame(game.id, message);
    });

    this.gameManager.on('engine_response', (response) => {
      const message: WebSocketMessage = {
        type: 'engine_response',
        gameId: response.gameId,
        data: response,
        timestamp: new Date()
      };
      this.broadcastToGame(response.gameId, message);
    });

    this.gameManager.on('error', (error) => {
      const message: WebSocketMessage = {
        type: 'error',
        gameId: error.gameId,
        data: { error: error.error },
        timestamp: new Date()
      };
      this.broadcastToGame(error.gameId, message);
    });
  }

  /**
   * すべての接続を終了
   */
  closeAllConnections(): void {
    for (const [connectionId, connection] of this.connections) {
      try {
        connection.ws.close();
      } catch (error) {
        console.error(`Failed to close connection ${connectionId}:`, error);
      }
    }
    this.connections.clear();
  }
}