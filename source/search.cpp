#include <algorithm>
#include <thread>

#include "evaluate.h"
#include "misc.h"
#include "search.h"
#include "usi.h"

struct MovePicker {
  // 静止探索で使用
  MovePicker(const Position &pos_, Square recapSq) : pos(pos_) {
    if (pos.in_check())
      endMoves = generateMoves<EVASIONS>(pos, currentMoves);
    else
      endMoves = generateMoves<RECAPTURES>(pos, currentMoves, recapSq);
  }

  Move nextMove() {
    if (currentMoves == endMoves)
      return MOVE_NONE;
    return *currentMoves++;
  }

private:
  const Position &pos;
  ExtMove moves[MAX_MOVES], *currentMoves = moves, *endMoves = moves;
};

namespace Search {
// 探索開始局面で思考対象とする指し手の集合。
RootMoves rootMoves;

// 持ち時間設定など。
LimitsType Limits;

// 今回のgoコマンドでの探索ノード数。
uint64_t Nodes;

// 探索中にこれがtrueになったら探索を即座に終了すること。
bool Stop;

} // namespace Search

// 起動時に呼び出される。時間のかからない探索関係の初期化処理はここに書くこと。
void Search::init() {
#ifdef USE_TRANSPOSITION_TABLE
  // 置換表を初期化
  TT.resize(DEFAULT_TT_SIZE);
#endif
}

// isreadyコマンドの応答中に呼び出される。時間のかかる処理はここに書くこと。
void Search::clear() {
#ifdef USE_TRANSPOSITION_TABLE
  // 置換表をクリア
  TT.clear();
#endif
}

// 同じ関数名で引数が異なる関数をオーバーロードという。
// Value search(Position &pos, int depth, int ply_from_root);
// Value Search::search(Position &pos, Value alpha, Value beta, int depth,
//              int ply_from_root);

// 探索を開始する
void Search::start_thinking(const Position &rootPos, StateListPtr &states,
                            LimitsType limits) {
  Limits = limits;
  rootMoves.clear();
  Nodes = 0;
  Stop = false;

  for (Move move : MoveList<LEGAL_ALL>(rootPos))
    rootMoves.emplace_back(move);

  ASSERT_LV3(states.get());

  Position *pos_ptr = const_cast<Position *>(&rootPos);
  search(*pos_ptr);
}

// 探索本体
void Search::search(Position &pos) {
  // 探索で返す指し手
  Move bestMove = MOVE_RESIGN;

  if (rootMoves.size() == 0) {
    // 合法手が存在しない
    Stop = true;
    goto END;
  }

  /* ここから探索部を記述する */
  {
#ifdef USE_TRANSPOSITION_TABLE
    // 置換表の新しい探索セッションを開始
    TT.new_search();
#endif

    /* 時間制御 */
    Color us = pos.side_to_move();
    std::thread *timerThread = nullptr;

    // 時間制御のための終了時刻を計算
    s64 endTime = 0;
    if (Limits.use_time_management()) {
      // 秒読みから終了時刻を計算（150ms余裕を持たせる）
      endTime = Limits.byoyomi[us] - 150;
    }

    // タイマースレッドの起動（時間制御が必要な場合のみ）
    if (Limits.use_time_management()) {
      timerThread = new std::thread([&] {
        while (Time.elapsed() < endTime && !Stop)
          std::this_thread::sleep_for(std::chrono::milliseconds(9990));
        Stop = true;
      });
    }

    /* 探索開始 - 反復深化探索 */
    Value maxValue = -VALUE_INFINITE; // 初期値はマイナス∞
    Value alpha = -VALUE_INFINITE;
    Value beta = VALUE_INFINITE;
    
    StateInfo si;
    // debug
    int maxDepth = 50;
    // int maxDepth = Limits.depth ? Limits.depth : 20; // goコマンドで指定された深さ、なければ20

    // 反復深化探索
    for (int depth = 1; depth <= maxDepth && !Stop; ++depth) {
      // ノード数制限のチェック
      if (Limits.nodes && Nodes >= (uint64_t)Limits.nodes) {
        Stop = true;
        break;
      }
      Value currentMaxValue = -VALUE_INFINITE;
      Move currentBestMove = MOVE_NONE;

      // 各合法手について探索
      for (size_t i = 0; i < rootMoves.size(); ++i) {
        Move move = rootMoves[i].pv[0];           // 合法手のi番目
        pos.do_move(move, si);                    // 局面を1手進める
        std::vector<Move> pv;
        Value value = (-1) * alphabeta_search(pos, pv, alpha, beta, depth-1, 0); // 指定深さで探索
        // PVの更新：探索から得られたPVを尊重（破壊しない）
        if (!pv.empty()) {
            rootMoves[i].pv.clear();
            rootMoves[i].pv.emplace_back(move);
            rootMoves[i].pv.insert(rootMoves[i].pv.end(), pv.begin(), pv.end());
        }
        pos.undo_move(move);               
               // 局面を1手戻す

        // rootMovesのスコアを更新
        rootMoves[i].score = value;
        rootMoves[i].selDepth = depth; // 選択深さを設定

        if(chmax(currentMaxValue, value)) {
          currentBestMove = move;
        }

        // [TODO] debug ソートが多すぎるので本来は深化するごとに一回だけ
        // 評価値順にrootMovesをソート
        std::stable_sort(rootMoves.begin(), rootMoves.end());
        std::cout << USI::pv(pos, depth) << std::endl;
      }
      
      if (currentMaxValue > maxValue) {
        maxValue = currentMaxValue;
        bestMove = currentBestMove;
      }
    }
    /* 探索終了 */

    std::stable_sort(rootMoves.begin(), rootMoves.end());

    // タイマースレッド終了
    Stop = true;
    if (timerThread != nullptr) {
      timerThread->join();
      delete timerThread;
    }
  }
  /* 探索部ここまで */

END:;
  std::cout << "bestmove " << bestMove << std::endl;
}

// ネガマックス法(nega-max method)
Value Search::negamax_search(Position &pos, std::vector<Move> &pv, int depth, int ply_from_root) {
  // 探索ノード数をインクリメント
  ++Nodes;

  // 探索打ち切り
  if (Stop) {
    pv.clear();
    return Eval::evaluate(pos);
  }

  // 探索深さに達したら評価関数を呼び出して終了
  if (depth == 0) {
    pv.clear();
    return Eval::evaluate(pos);
  }

  Value maxValue = -VALUE_INFINITE; // 初期値はマイナス∞
  // 初期探索範囲は[-∞, +∞]
  Value alpha = -VALUE_INFINITE;
  Value beta = VALUE_INFINITE;
  std::vector<Move> bestPv;

  StateInfo si;
  const auto legalMoves = MoveList<LEGAL_ALL>(pos);
  if(legalMoves.size() == 0) {
    // 合法手が存在しない -> 詰み
    pv.clear();
    return mated_in(ply_from_root);
  }

  for (ExtMove move : legalMoves) {
    std::vector<Move> childPv;
    pos.do_move(move.move, si); // 局面を1手進める
    Value value = (-1) * alphabeta_search(pos, childPv, alpha, beta, depth - 1, ply_from_root + 1); // 再帰的に呼び出し
    pos.undo_move(move.move);

    if (value > maxValue) {
      maxValue = value;
      // 最適なPVを構築
      bestPv.clear();
      bestPv.emplace_back(move.move);
      bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
    }
  }

  pv = bestPv;
  return maxValue;
}

// アルファ・ベータ法(alpha-beta method)
Value Search::alphabeta_search(Position &pos, std::vector<Move> &pv, Value alpha, Value beta, int depth, int ply_from_root) {
  // 探索ノード数をインクリメント
  ++Nodes;

  // 探索打ち切り
  if (Stop) {
    pv.clear();
    return Eval::evaluate(pos);
  }

#ifdef USE_TRANSPOSITION_TABLE
  // 置換表を参照
  bool ttHit;
  TTData ttd(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false);
  TTWriter ttWriter;

  // 置換表を検索
  auto tt_result = TT.probe(pos.key());
  ttHit = std::get<0>(tt_result);
  ttd = std::get<1>(tt_result);
  ttWriter = std::get<2>(tt_result);

  // 置換表にヒットした場合
  if (ttHit && ttd.depth >= depth) {
    if (ttd.bound == BOUND_EXACT) {
      pv.assign(1, ttd.move);
      return ttd.value;
    } else if (ttd.bound == BOUND_LOWER && ttd.value >= beta) {
      pv.assign(1, ttd.move);
      return ttd.value;
    } else if (ttd.bound == BOUND_UPPER && ttd.value <= alpha) {
      return ttd.value;
    }
  }
#endif

  // 探索深さに達したら評価関数を呼び出して終了
  if (depth == 0) {
    pv.clear();
    return Eval::evaluate(pos);
  }

  Value maxValue = -VALUE_INFINITE;
  std::vector<Move> bestPv;
  StateInfo si;
  const auto legalMoves = MoveList<LEGAL_ALL>(pos);

  if(legalMoves.size() == 0) {
    // 合法手が存在しない -> 詰み
    pv.clear();
    return mated_in(ply_from_root);
  }
  
  /**
   * 千日手判定
   * 先手負け
   * 行ったり来たりするにしても16手かかる
   * 後手が番のときは次の先手が詰みという処理をする
   * [TODO] 千日手用の値があると思うから置き換える
   */
  if(pos.is_repetition(16)) {
    pv.clear();
    return mated_in(ply_from_root + (pos.side_to_move() == WHITE));
  }

#ifdef USE_TRANSPOSITION_TABLE
  // 探索順序の最適化：置換表の最善手を優先
  std::deque<ExtMove> orderedMoves;
  for (const ExtMove &move : legalMoves) {
    if (ttHit && move.move == ttd.move) {
      // 置換表の最善手を最初に
      orderedMoves.push_front(move);
    } else {
      orderedMoves.emplace_back(move);
    }
  }
#else
  std::vector<ExtMove> orderedMoves;
  for (ExtMove move : legalMoves) {
    orderedMoves.push_back(move);
  }
#endif

  for (ExtMove &move : orderedMoves) {
    std::vector<Move> childPv;

    pos.do_move(move.move, si); // 局面を1手進める
    // [TODO] ExtMoveにはvalueがあるけどそれを使うべきかを検討(そのまま置換するとエラーが出る)
    Value value = (-1) * alphabeta_search(pos, childPv, -beta, -alpha, depth - 1, ply_from_root + 1); // 再帰的に呼び出し
    pos.undo_move(move.move);

    // アルファ・ベータカット
    if(value >= beta) {
      // betaカットの場合でも最適なPVを返す
      pv.clear();
      pv.emplace_back(move.move);
      pv.insert(pv.end(), childPv.begin(), childPv.end());
      return value;
    }

    if(value > maxValue) {
      maxValue = value;
      // 最適なPVを構築
      bestPv.clear();
      bestPv.emplace_back(move.move);
      bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
    }

    if(value > alpha) {
      alpha = value;
    }
  }

#ifdef USE_TRANSPOSITION_TABLE
  // 置換表に探索結果を保存
  if (!Stop) {
    Bound bound;
    if (maxValue >= beta) {
      bound = BOUND_LOWER;
    } else if (maxValue <= alpha) {
      bound = BOUND_UPPER;
    } else {
      bound = BOUND_EXACT;
    }

    const Move &bestMove = bestPv.empty() ? MOVE_NONE : bestPv[0];
    const Value &evalValue = Eval::evaluate(pos);

    ttWriter.write(pos.key(), maxValue, true, bound, depth, bestMove, evalValue, TT.generation());
  }
#endif

  pv = bestPv;
  return maxValue;
}