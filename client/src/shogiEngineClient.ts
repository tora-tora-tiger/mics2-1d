import { spawn } from 'node:child_process';
import { EventEmitter } from 'node:events';

/**
 * USIプロトコル対応の将棋エンジンクライアント
 *
 * 外部の将棋エンジンプロセスと通信し、USIコマンドを送受信する
 * イベント駆動型アーキテクチャで非同期応答を処理
 */
export default class ShogiEngineClient extends EventEmitter {
  /** エンジンプロセス */
  private engine!: ReturnType<typeof spawn>;
  /** 応答データの一時バッファ */
  private responseBuffer: string = '';

  /**
   * コンストラクタ
   * @param enginePath 将棋エンジンの実行ファイルパス
   */
  constructor(private enginePath: string) {
    super();
    this.initializeEngine();
  }

  /**
   * エンジンプロセスを初期化し、イベントハンドラを設定
   */
  private initializeEngine() {
    // エンジンプロセスを起動
    this.engine = spawn(this.enginePath, [], {
      stdio: ['pipe', 'pipe', 'pipe']
    });

    // stdin/stdoutが利用可能かチェック
    if (!this.engine.stdin || !this.engine.stdout) {
      throw new Error('Failed to initialize engine process');
    }

    // 標準出力データを処理
    this.engine.stdout.on('data', (data: Buffer) => {
      this.handleResponse(data.toString());
    });

    // 標準エラー出力を処理（存在する場合）
    if (this.engine.stderr) {
      this.engine.stderr.on('data', (data: Buffer) => {
        console.error('Engine error:', data.toString());
      });
    }

    // プロセス終了イベント
    this.engine.on('close', (code: number) => {
      console.log(`Engine process exited with code ${code}`);
      this.emit('close', code);
    });

    // プロセスエラーイベント
    this.engine.on('error', (error: Error) => {
      console.error('Engine error:', error);
      this.emit('error', error);
    });
  }

  /**
   * エンジンからの応答を一行ずつ処理
   * @param data エンジンからの生データ
   */
  private handleResponse(data: string) {
    this.responseBuffer += data;
    const lines = this.responseBuffer.split('\n');
    this.responseBuffer = lines.pop() || '';

    // 各行をトリムしてイベント発火
    for (const line of lines) {
      if (line.trim()) {
        this.emit('response', line.trim());
      }
    }
  }

  /**
   * エンジンにコマンドを送信
   * @param command USIコマンド
   */
  sendCommand(command: string): void {
    if (this.engine.stdin) {
      this.engine.stdin.write(command + '\n');
    }
  }

  /**
   * エンジンを終了
   */
  quit(): void {
    this.sendCommand('quit');
  }

  /**
   * エンジンの準備完了を確認
   * @returns 応答文字列の配列
   */
  async isReady(): Promise<string[]> {
    return new Promise((resolve) => {
      const responses: string[] = [];
      const onResponse = (response: string) => {
        responses.push(response);
        if (response === 'readyok') {
          this.off('response', onResponse);
          resolve(responses);
        }
      };
      this.on('response', onResponse);
      this.sendCommand('isready');
    });
  }

  /**
   * USIプロトコルを開始
   * @returns 応答文字列の配列
   */
  async usi(): Promise<string[]> {
    return new Promise((resolve) => {
      const responses: string[] = [];
      const onResponse = (response: string) => {
        responses.push(response);
        if (response === 'usiok') {
          this.off('response', onResponse);
          resolve(responses);
        }
      };
      this.on('response', onResponse);
      this.sendCommand('usi');
    });
  }
}