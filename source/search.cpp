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
void Search::init() {}

// isreadyコマンドの応答中に呼び出される。時間のかかる処理はここに書くこと。
void Search::clear() {}

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
    /* 時間制御 */
    Color us = pos.side_to_move();
    std::thread *timerThread = nullptr;

    // 今回は秒読み以外の設定は考慮しない
    s64 endTime = Limits.byoyomi[us] - 150;

    timerThread = new std::thread([&] {
      while (Time.elapsed() < endTime && !Stop)
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
      Stop = true;
    });

    /* 探索開始 */
    Value maxValue = -VALUE_INFINITE; // 初期値はマイナス∞
    StateInfo si;
    int rootDepth = 5; // 探索深さ
    for (int i = 0; i < rootMoves.size(); ++i) {
      Move move = rootMoves[i].pv[0];           // 合法手のi番目. スライド4枚目参照
      pos.do_move(move, si);                    // 局面を1手進める
      Value value = (-1) * negamax_search(pos, rootDepth-1, 0); // 評価関数を呼び出す
      pos.undo_move(move);                      // 局面を1手戻す

      // rootMovesのスコアを更新
      rootMoves[i].score = value;

      if (value > maxValue) {
        maxValue = value;
        bestMove = move;
      }
    }
    /* 探索終了 */

    // // 評価値順にrootMovesをソート
    // std::stable_sort(rootMoves.begin(), rootMoves.end());

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
Value Search::negamax_search(Position &pos, int depth, int ply_from_root) {
  // 探索ノード数をインクリメント
  ++Nodes;

  // 探索打ち切り
  if (Stop)
    return Eval::evaluate(pos);

  // 探索深さに達したら評価関数を呼び出して終了
  if (depth == 0)
    return Eval::evaluate(pos);

  Value maxValue = -VALUE_INFINITE; // 初期値はマイナス∞

  StateInfo si;
  const auto legalMoves = MoveList<LEGAL_ALL>(pos);
  if(legalMoves.size() == 0) {
    // 合法手が存在しない -> 詰み
    return mated_in(ply_from_root);
  }

  for (ExtMove move : legalMoves) {
    pos.do_move(move.move, si); // 局面を1手進める
    Value value = (-1) * negamax_search(pos, depth - 1, ply_from_root + 1); // 再帰的に呼び出し
    pos.undo_move(move.move);

    if (value > maxValue)
      maxValue = value;
  }

  return maxValue;
}

// アルファ・ベータ法(apha-beta method)
