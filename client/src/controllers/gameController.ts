import { Context } from 'hono';
import { GameManager } from '../services/gameManager.js';
import { CreateGameRequest, Game } from '../types/game.js';
import { ApiResponse, ApiError, GamesResponse, GameResponse } from '../types/api.js';

/**
 * 対局APIコントローラー
 */
export class GameController {
  constructor(private gameManager: GameManager) {}

  /**
   * 対局一覧を取得
   */
  getGames(c: Context): Response {
    const games = this.gameManager.getAllGames();

    const response: GamesResponse = { games };
    return c.json(response);
  }

  /**
   * 特定の対局情報を取得
   */
  getGame(c: Context): Response {
    const gameId = c.req.param('gameId');
    const game = this.gameManager.getGame(gameId);

    if (!game) {
      const error: ApiError = {
        error: 'Game not found',
        timestamp: new Date()
      };
      return c.json(error, 404);
    }

    const response: GameResponse = { game };
    return c.json(response);
  }

  /**
   * 新規対局を作成
   */
  async createGame(c: Context): Promise<Response> {
    try {
      const request: CreateGameRequest = await c.req.json();

      // バリデーション
      if (!request.player) {
        const error: ApiError = {
          error: 'Player name is required',
          timestamp: new Date()
        };
        return c.json(error, 400);
      }

      const game = this.gameManager.createGame(request);

      const response: GameResponse = { game };
      return c.json(response, 201);

    } catch (error) {
      const apiError: ApiError = {
        error: 'Invalid request format',
        timestamp: new Date()
      };
      return c.json(apiError, 400);
    }
  }

  /**
   * 対局を開始
   */
  async startGame(c: Context): Promise<Response> {
    const gameId = c.req.param('gameId');
    const success = await this.gameManager.startGame(gameId);

    if (!success) {
      const error: ApiError = {
        error: 'Failed to start game. Game not found or already started.',
        timestamp: new Date()
      };
      return c.json(error, 400);
    }

    const response: ApiResponse = {
      status: 'success',
      data: { message: 'Game started successfully' },
      timestamp: new Date()
    };
    return c.json(response);
  }

  /**
   * 対局を停止
   */
  async stopGame(c: Context): Promise<Response> {
    const gameId = c.req.param('gameId');
    const success = await this.gameManager.stopGame(gameId);

    if (!success) {
      const error: ApiError = {
        error: 'Failed to stop game. Game not found or not running.',
        timestamp: new Date()
      };
      return c.json(error, 400);
    }

    const response: ApiResponse = {
      status: 'success',
      data: { message: 'Game stopped successfully' },
      timestamp: new Date()
    };
    return c.json(response);
  }

  /**
   * 対局を終了
   */
  async endGame(c: Context): Promise<Response> {
    const gameId = c.req.param('gameId');
    await this.gameManager.endGame(gameId);

    const response: ApiResponse = {
      status: 'success',
      data: { message: 'Game ended successfully' },
      timestamp: new Date()
    };
    return c.json(response);
  }
}