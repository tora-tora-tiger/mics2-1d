import { EventEmitter } from 'node:events';
import ShogiEngineClient from './shogiEngineClient.js';
import { Game, CreateGameRequest, GameState, EngineResponse } from './types/game.js';

/**
 * 将棋対局マネージャー
 *
 * 複数の対局を管理し、エンジンとの通信を仲介する
 */
export class GameManager extends EventEmitter {
  /** 実行中の対局マップ */
  private games: Map<string, Game> = new Map();
  /** エンジンクライアントマップ */
  private engines: Map<string, ShogiEngineClient> = new Map();

  constructor() {
    super();
  }

  /**
   * 新規対局を作成
   * @param request 対局作成リクエスト
   * @returns 作成された対局
   */
  createGame(request: CreateGameRequest): Game {
    const game: Game = {
      id: crypto.randomUUID(),
      player: request.player,
      enginePath: request.enginePath || '../source/minishogi-by-gcc',
      state: 'waiting',
      position: request.position || 'startpos',
      timeLimit: request.timeLimit || 60000,
      byoyomi: request.byoyomi || 10000,
      createdAt: new Date(),
      updatedAt: new Date()
    };

    this.games.set(game.id, game);
    this.emit('game_created', game);

    return game;
  }

  /**
   * 対局を開始
   * @param gameId 対局ID
   * @returns 成功したかどうか
   */
  async startGame(gameId: string): Promise<boolean> {
    const game = this.games.get(gameId);
    if (!game || game.state !== 'waiting') {
      return false;
    }

    try {
      // エンジンを初期化
      const engine = new ShogiEngineClient(game.enginePath);
      this.engines.set(gameId, engine);

      // イベントリスナーを設定
      this.setupEngineListeners(gameId, engine);

      // USIプロトコルを開始
      await engine.usi();
      await engine.isReady();

      // 対局を開始
      game.state = 'playing';
      game.updatedAt = new Date();

      // USIコマンドを送信
      engine.sendCommand('usinewgame');
      engine.sendCommand(`position ${game.position}`);
      engine.sendCommand(`go btime ${game.timeLimit} wtime ${game.timeLimit} binc ${game.byoyomi} winc ${game.byoyomi}`);

      this.emit('game_started', game);
      return true;

    } catch (error) {
      console.error(`Failed to start game ${gameId}:`, error);
      this.emit('error', { gameId, error: error.message });
      return false;
    }
  }

  /**
   * 対局を停止
   * @param gameId 対局ID
   * @returns 成功したかどうか
   */
  stopGame(gameId: string): boolean {
    const game = this.games.get(gameId);
    const engine = this.engines.get(gameId);

    if (!game || !engine) {
      return false;
    }

    // エンジンに停止コマンドを送信
    engine.sendCommand('stop');

    // 状態を更新
    game.state = 'ended';
    game.updatedAt = new Date();

    this.emit('game_stopped', game);
    return true;
  }

  /**
   * 対局を終了
   * @param gameId 対局ID
   */
  endGame(gameId: string): void {
    const game = this.games.get(gameId);
    const engine = this.engines.get(gameId);

    if (engine) {
      engine.quit();
      this.engines.delete(gameId);
    }

    if (game) {
      game.state = 'ended';
      game.updatedAt = new Date();
      this.emit('game_ended', game);
    }
  }

  /**
   * エンジンリスナーを設定
   * @param gameId 対局ID
   * @param engine エンジンクライアント
   */
  private setupEngineListeners(gameId: string, engine: ShogiEngineClient): void {
    engine.on('response', (response: string) => {
      const engineResponse: EngineResponse = {
        gameId,
        command: response,
        response,
        timestamp: new Date()
      };

      this.emit('engine_response', engineResponse);

      // 対局の最善手応答を処理
      if (response.startsWith('bestmove')) {
        const game = this.games.get(gameId);
        if (game && game.state === 'playing') {
          // 実際の対局ではここで棋譜更新などを行う
          console.log(`Game ${gameId}: Best move received - ${response}`);
        }
      }
    });

    engine.on('error', (error: Error) => {
      this.emit('error', { gameId, error: error.message });
      this.endGame(gameId);
    });

    engine.on('close', (code: number) => {
      console.log(`Engine for game ${gameId} closed with code ${code}`);
      this.endGame(gameId);
    });
  }

  /**
   * 対局情報を取得
   * @param gameId 対局ID
   * @returns 対局情報
   */
  getGame(gameId: string): Game | undefined {
    return this.games.get(gameId);
  }

  /**
   * すべての対局を取得
   * @returns 対局リスト
   */
  getAllGames(): Game[] {
    return Array.from(this.games.values());
  }

  /**
   * すべての対局を終了
   */
  terminateAllGames(): void {
    for (const gameId of this.engines.keys()) {
      this.endGame(gameId);
    }
  }
}