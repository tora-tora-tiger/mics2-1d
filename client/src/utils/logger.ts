/**
 * ログ出力ユーティリティ
 */

/** ログレベル */
export type LogLevel = 'info' | 'warn' | 'error' | 'debug';

/**
 * ログを出力
 * @param level ログレベル
 * @param message メッセージ
 * @param data 追加データ
 */
export function log(level: LogLevel, message: string, data?: any): void {
  const timestamp = new Date().toISOString();
  const logEntry = {
    timestamp,
    level,
    message,
    data
  };

  switch (level) {
    case 'info':
      console.info(`[${timestamp}] INFO: ${message}`, data || '');
      break;
    case 'warn':
      console.warn(`[${timestamp}] WARN: ${message}`, data || '');
      break;
    case 'error':
      console.error(`[${timestamp}] ERROR: ${message}`, data || '');
      break;
    case 'debug':
      if (process.env.NODE_ENV === 'development') {
        console.debug(`[${timestamp}] DEBUG: ${message}`, data || '');
      }
      break;
  }
}

/**
 * infoログ
 */
export const logInfo = (message: string, data?: any) => log('info', message, data);

/**
 * warnログ
 */
export const logWarn = (message: string, data?: any) => log('warn', message, data);

/**
 * errorログ
 */
export const logError = (message: string, data?: any) => log('error', message, data);

/**
 * debugログ
 */
export const logDebug = (message: string, data?: any) => log('debug', message, data);