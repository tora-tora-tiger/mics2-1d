# 5五将棋思考エンジン クラスシーケンス図

やねうら王をベースとした5五将棋用思考エンジンの主要なクラス間の相互作用を示すシーケンス図です。

## 1. エンジン起動シーケンス

```mermaid
sequenceDiagram
    participant Main
    participant Bitboards
    participant Position
    participant Search
    participant USI

    Main->>Bitboards: init()
    Bitboards-->>Main: 初期化完了

    Main->>Position: init()
    Position-->>Main: Zobristテーブル初期化完了

    Main->>Search: init()
    Search-->>Main: 探索部初期化完了

    Main->>USI: loop(argc, argv)
    USI->>USI: コマンド待機
```

## 2. USIプロトコル処理シーケンス

```mermaid
sequenceDiagram
    participant GUI
    participant USI
    participant Position
    participant StateList
    participant Search

    GUI->>USI: "usi"コマンド
    USI->>GUI: engine_info() + "usiok"

    GUI->>USI: "isready"コマンド
    USI->>Position: set_hirate()
    USI->>StateList: 新規StateList作成
    USI->>Search: clear()
    USI->>GUI: "readyok"

    GUI->>USI: "position startpos moves ..."
    USI->>Position: set(sfen)
    USI->>StateList: 新規StateList作成
    loop 各指し手
        USI->>Position: do_move(move, state)
        Position->>StateList: StateInfo追加
    end

    GUI->>USI: "go"コマンド
    USI->>Search: start_thinking(pos, states, limits)
    Search->>Search: search()
    Search->>GUI: "bestmove move"
```

## 3. 探索処理シーケンス

```mermaid
sequenceDiagram
    participant Search
    participant Position
    participant Eval
    participant MoveList
    participant Timer

    Search->>Search: start_thinking()
    Search->>Position: 合法手取得
    Search->>MoveList: MoveList<LEGAL_ALL>(pos)
    MoveList-->>Search: 全合法手

    Search->>Timer: reset()
    Search->>Search: タイマースレッド起動

    loop 各合法手
        Search->>Position: do_move(move, si)
        Position->>Position: 局面更新
        Search->>Eval: evaluate(pos)
        Eval-->>Search: 評価値
        Search->>Position: undo_move(move)
        Position->>Position: 局面復元
        Search->>Search: rootMoves[i].score更新
    end

    Search->>Search: rootMoves評価値順ソート
    Search->>Search: タイマースレッド停止
    Search-->>GUI: bestmove出力
```

## 4. 局面管理シーケンス

```mermaid
sequenceDiagram
    participant Position
    participant StateInfo
    participant Bitboard
    participant Zobrist

    Position->>Position: set(sfen, si)
    Position->>Position: 盤面解析
    Position->>Bitboard: put_piece()駒配置
    Position->>Bitboard: update_bitboards()
    Position->>Position: update_kingSquare()
    Position->>StateInfo: set_state()
    StateInfo->>Zobrist: hash key計算
    StateInfo->>StateInfo: 王手情報設定
    StateInfo->>StateInfo: pin情報設定

    Position->>Position: do_move(move, newSt)
    Position->>StateInfo: 状態保存
    Position->>Bitboard: 駒移動処理
    Position->>Zobrist: hash key更新
    Position->>StateInfo: 王手情報更新
    Position->>Position: 手番変更
```

## 5. 評価関数シーケンス

```mermaid
sequenceDiagram
    participant Search
    participant Eval
    participant Position

    Search->>Eval: evaluate(pos)
    Eval->>Eval: score = VALUE_ZERO

    loop 盤上の全升
        Eval->>Position: piece_on(sq)
        Position-->>Eval: 駒情報
        Eval->>Eval: score += PieceValue[pc]
    end

    loop 先手後手
        loop 手駒種類
            Eval->>Position: hand_count(hand, pc)
            Position-->>Eval: 駒数
            Eval->>Eval: score += 駒数×価値
        end
    end

    Eval->>Position: side_to_move()
    Position-->>Eval: 手番
    Eval-->>Search: 手番側評価値
```

## 6. 指し手生成シーケンス

```mermaid
sequenceDiagram
    participant Position
    participant MoveList
    participant MoveGenerator

    Position->>MoveList: MoveList<LEGAL_ALL>(pos)
    MoveList->>MoveGenerator: generateMoves<LEGAL_ALL>()

    alt 王手されている場合
        MoveGenerator->>MoveGenerator: EVASIONS手生成
    else 王手されていない場合
        MoveGenerator->>MoveGenerator: NON_EVASIONS手生成
    end

    loop 各駒種
        MoveGenerator->>Position: pieces(pc)
        Position-->>MoveGenerator: 駒位置Bitboard
        MoveGenerator->>Bitboard: 利き計算
        MoveGenerator->>Position: legal()チェック
        Position-->>MoveGenerator: 合法性判定
    end

    MoveGenerator-->>MoveList: ExtMove配列
    MoveList-->>Position: 生成完了
```

## 7. 千日手判定シーケンス

```mermaid
sequenceDiagram
    participant Position
    participant StateInfo
    participant RepetitionState

    Position->>Position: is_repetition()

    loop 遡り手数分（最大16手）
        Position->>StateInfo: board_key()比較
        alt board_keyが一致
            Position->>StateInfo: hand()比較
            alt handも一致
                Position->>RepetitionState: 連続王手チェック
                RepetitionState-->>Position: REPETITION_WIN/LOSE/DRAW
            else handが異なる
                Position->>RepetitionState: 優等・劣等判定
                RepetitionState-->>Position: REPETITION_SUPERIOR/INFERIOR
            end
        end
    end

    Position-->>Rep: REPETITION_NONE
```

## 主要クラスの役割

- **Main**: エンジンのエントリーポイント、初期化処理
- **USI**: USIプロトコルのメッセージ応答、コマンド処理
- **Position**: 局面情報の管理、指し手の実行・取消
- **Search**: 探索処理の実行、時間管理、最善手の決定
- **Eval**: 局面の評価値計算
- **StateInfo**: 局面の状態情報（ハッシュキー、王手情報など）
- **Bitboard**: 盤面の駒配置管理、利き計算
- **MoveList**: 指し手の生成と管理

これらのシーケンス図は、エンジンの主要な処理フローにおけるクラス間の相互作用を示しています。