import ShogiEngineClient from "./shogiEngineClient";

/**
 * 将棋エンジンクライアントの使用例
 *
 * コマンドライン引数からエンジンパスを受け取り、
 * USIプロトコルでエンジンと通信する
 */
if (import.meta.url === `file://${process.argv[1]}`) {
  // エンジンパスを取得（デフォルト: ../source/minishogi-by-gcc）
  const enginePath = process.argv[2] || '../source/minishogi-by-gcc';
  const client = new ShogiEngineClient(enginePath);

  // エンジンからの応答を監視
  client.on('response', (response) => {
    console.log('Engine:', response);
  });

  // エラーを監視
  client.on('error', (error) => {
    console.error('Client error:', error);
  });

  // USIプロトコルを開始
  setTimeout(async () => {
    console.log('Starting USI protocol...');

    // USIコマンドを送信
    const usiResponses = await client.usi();
    console.log('USI responses:', usiResponses);

    // 準備完了を確認
    const readyResponses = await client.isReady();
    console.log('Ready responses:', readyResponses);
  }, 1000);

  // Ctrl+Cで正常終了
  process.on('SIGINT', () => {
    console.log('\nShutting down...');
    client.quit();
    process.exit(0);
  });
}