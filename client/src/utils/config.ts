/**
 * 設定ユーティリティ
 */

/** 設定インターフェース */
export interface Config {
  port: number;
  defaultEnginePath: string;
  defaultTimeLimit: number;
  defaultByoyomi: number;
  nodeEnv: string;
  logLevel: 'info' | 'warn' | 'error' | 'debug';
}

/**
 * 環境変数から設定を読み込み
 * @returns 設定オブジェクト
 */
export function loadConfig(): Config {
  return {
    port: Number(process.env.PORT) || 3000,
    defaultEnginePath: process.env.DEFAULT_ENGINE_PATH || '../source/minishogi-by-gcc',
    defaultTimeLimit: Number(process.env.DEFAULT_TIME_LIMIT) || 60000,
    defaultByoyomi: Number(process.env.DEFAULT_BYOYOMI) || 10000,
    nodeEnv: process.env.NODE_ENV || 'development',
    logLevel: (process.env.LOG_LEVEL as any) || 'info'
  };
}

/**
 * デフォルト設定を取得
 */
export const config = loadConfig();