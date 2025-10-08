import { CreateGameRequest } from '../types/game.js';

/**
 * バリデーションユーティリティ
 */

/**
 * 対局作成リクエストを検証
 * @param request 対局作成リクエスト
 * @returns 検証結果
 */
export function validateCreateGameRequest(request: CreateGameRequest): {
  isValid: boolean;
  errors: string[];
} {
  const errors: string[] = [];

  // 必須項目チェック
  if (!request.player) {
    errors.push('Player name is required');
  } else if (typeof request.player !== 'string' || request.player.trim().length === 0) {
    errors.push('Player name must be a non-empty string');
  } else if (request.player.length > 100) {
    errors.push('Player name must be less than 100 characters');
  }

  // オプション項目チェック
  if (request.timeLimit !== undefined) {
    if (typeof request.timeLimit !== 'number' || request.timeLimit <= 0) {
      errors.push('Time limit must be a positive number');
    } else if (request.timeLimit > 3600000) {
      errors.push('Time limit must be less than 1 hour');
    }
  }

  if (request.byoyomi !== undefined) {
    if (typeof request.byoyomi !== 'number' || request.byoyomi <= 0) {
      errors.push('Byoyomi must be a positive number');
    } else if (request.byoyomi > 60000) {
      errors.push('Byoyomi must be less than 1 minute');
    }
  }

  if (request.enginePath !== undefined) {
    if (typeof request.enginePath !== 'string' || request.enginePath.trim().length === 0) {
      errors.push('Engine path must be a non-empty string');
    }
  }

  if (request.position !== undefined) {
    if (typeof request.position !== 'string' || request.position.trim().length === 0) {
      errors.push('Position must be a non-empty string');
    }
  }

  return {
    isValid: errors.length === 0,
    errors
  };
}

/**
 * UUIDを検証
 * @param uuid UUID文字列
 * @returns 検証結果
 */
export function isValidUUID(uuid: string): boolean {
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  return uuidRegex.test(uuid);
}