/**
 * サーバーエントリーポイント
 */

import { App } from './app.js';

/**
 * アプリケーションを起動
 */
async function main(): Promise<void> {
  const app = new App();
  await app.start();
}

// アプリケーションを起動
main().catch((error) => {
  console.error('Failed to start application:', error);
  process.exit(1);
});