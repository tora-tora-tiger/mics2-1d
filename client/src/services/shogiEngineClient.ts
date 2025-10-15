import { spawn } from 'node:child_process';
import { EventEmitter } from 'node:events';

/**
 * 保留中のコマンド情報
 */
interface PendingCommand {
  id: string;
  command: string;
  resolve: (value: string[]) => void;
  reject: (error: Error) => void;
  terminator?: string;
  responses: string[];
}

/**
 * USIプロトコル対応の将棋エンジンクライアント
 *
 * 外部の将棋エンジンプロセスと通信し、USIコマンドを送受信する
 * イベント駆動型アーキテクチャで非同期応答を処理
 * コマンドとレスポンスの相関を管理
 */
export default class ShogiEngineClient extends EventEmitter {
  /** エンジンプロセス */
  private engine!: ReturnType<typeof spawn>;
  /** 応答データの一時バッファ */
  private responseBuffer: string = '';
  /** 保留中のコマンドキュー */
  private pendingCommands: Map<string, PendingCommand> = new Map();
  /** コマンドシーケンス番号 */
  private commandSequence = 0;

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
      this.cancelAllCommands();
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

    // 各行をトリムして処理
    for (const line of lines) {
      const trimmedLine = line.trim();
      if (!trimmedLine) continue;

      // コマンドとの相関をチェック
      this.processCommandResponse(trimmedLine);

      // ストリーミングもつけとく
      this.emit('engine_response', { type: 'stream', data: trimmedLine });
    }
  }

  /**
   * コマンドの応答を処理
   * @param line 応答行
   */
  private processCommandResponse(line: string) {
    // 最新の保留中コマンドを取得（FIFO）
    const pending = this.getLatestPendingCommand();
    if (!pending) return;

    // 応答を記録
    pending.responses.push(line);

    // 終端文字列で完了判定
    if (pending.terminator && line === pending.terminator) {
      this.completeCommand(pending.id);
    }
  }

  /**
   * 最新の保留中コマンドを取得
   */
  private getLatestPendingCommand(): PendingCommand | undefined {
    const commands = Array.from(this.pendingCommands.values());
    return commands.length > 0 ? commands[0] : undefined;
  }

  /**
   * コマンドを完了処理
   */
  private completeCommand(commandId: string) {
    const pending = this.pendingCommands.get(commandId);
    if (pending) {
      this.pendingCommands.delete(commandId);
      pending.resolve(pending.responses);
    }
  }

  /**
   * すべての保留中コマンドをキャンセル
   */
  private cancelAllCommands() {
    for (const pending of this.pendingCommands.values()) {
      pending.reject(new Error('Engine connection closed'));
    }
    this.pendingCommands.clear();
  }

  /**
   * エンジンにコマンドを送信（Promiseベース）
   * @param command USIコマンド
   * @param options オプション
   */
  async sendCommand(command: string, options?: {
    terminator?: string;
    timeout?: number;
    expectResponse?: boolean;
  }): Promise<string[]> {
    if (!this.engine || !this.engine.stdin) {
      throw new Error('エンジンが起動していません');
    }

    const commandId = `cmd_${++this.commandSequence}`;
    const timeout = options?.timeout || 20000;

    // console.log(`[DEBUG] Engine process PID: ${this.engine.pid}`);
    // console.log(`[DEBUG] Engine stdin writable: ${this.engine.stdin.writable}`);
    // console.log(`[DEBUG] Sending command: ${command}`);
    

    return new Promise((resolve, reject) => {
      if (!this.engine || !this.engine.stdin) {
        throw new Error('エンジンが起動していません');
      }
      // 応答を期待しない場合は即座に解決
      if (options?.expectResponse === false) {
        try {
          this.engine.stdin.write(command + '\n');
          // console.log(`[DEBUG] Command sent successfully (no response expected)`);
          resolve([]);
        } catch (error) {
          console.error(`[DEBUG] Error sending command: ${error}`);
          reject(error);
        }
        return;
      }

      const pending: PendingCommand = {
        id: commandId,
        command,
        resolve,
        reject,
        terminator: options?.terminator,
        responses: []
      };

      this.pendingCommands.set(commandId, pending);

      // タイムアウト処理
      const timeoutId = setTimeout(() => {
        this.pendingCommands.delete(commandId);
        reject(new Error(`Command timeout: ${command}`));
      }, timeout);

      // Promise解決時にタイムアウトをクリア
      const originalResolve = resolve;
      pending.resolve = (responses) => {
        clearTimeout(timeoutId);
        originalResolve(responses);
      };

      try {
        this.engine.stdin!.write(command + '\n');
        // console.log(`[DEBUG] Command sent successfully`);
      } catch (error) {
        this.pendingCommands.delete(commandId);
        clearTimeout(timeoutId);
        reject(error);
      }
    });
  }

  
  
  /**
   * エンジンを終了
   */
  async quit(): Promise<string[]> {
    return this.sendCommand('quit', { expectResponse: false });
  }

  /**
   * エンジンの準備完了を確認
   * @returns 応答文字列の配列
   */
  async isReady(): Promise<string[]> {
    return this.sendCommand('isready', { terminator: 'readyok' });
  }

  /**
   * USIプロトコルを開始
   * @returns 応答文字列の配列
   */
  async usi(): Promise<string[]> {
    return this.sendCommand('usi', { terminator: 'usiok' });
  }

  /**
   * 新しい対局を開始
   * @returns 応答文字列の配列
   */
  async usinewgame(): Promise<string[]> {
    return this.sendCommand('usinewgame', { expectResponse: false });
  }

  /**
   * 局面を設定
   * @param position 局面文字列（USI形式）
   * @returns 応答文字列の配列
   */
  async position(position: string): Promise<string[]> {
    // positionコマンドはエンジンからの応答がないため、expectResponse: falseを指定
    return this.sendCommand(`position ${position}`, { expectResponse: false });
  }

  /**
   * 思考開始
   * @param params goコマンドのパラメータ
   * @returns Promise（bestmove待機用）
   */
  async go(params: string): Promise<string> {
    return new Promise((resolve, reject) => {

      // bestmoveが返された時
      const bestmoveListener = (event: { type: string; data: string }) => {
        if(event.data.startsWith('bestmove')) {
          this.off('engine_response', bestmoveListener);
          resolve(event.data);
        }
      };

      this.on('engine_response', bestmoveListener);

      // goコマンドを送信（非同期で実行）
      this.sendCommand(`go ${params}`, { expectResponse: false }).catch(error => {
        this.off('engine_response', bestmoveListener);
        reject(error);
      });

      // タイムアウト設定（思考時間に応じて調整）
      setTimeout(() => {
        this.off('engine_response', bestmoveListener);
        reject(new Error('Go command timeout'));
      }, 60000); // 60秒
    });
  }

  /**
   * 思考を停止
   * @returns 応答文字列の配列
   */
  async stop(): Promise<string[]> {
    return this.sendCommand('stop');
  }
}

