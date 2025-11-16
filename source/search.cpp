#include <algorithm>
#include <thread>

#include "evaluate.h"
#include "misc.h"
#include "search.h"
#include "parallel_debug.h"
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
std::atomic<uint64_t> Nodes{0};

// 探索中にこれがtrueになったら探索を即座に終了すること。
std::atomic<bool> Stop{false};

// 並列探索マネージャー
std::unique_ptr<ParallelSearchManager> parallelManager;

} // namespace Search

// 起動時に呼び出される。時間のかからない探索関係の初期化処理はここに書くこと。
void Search::init() {
#ifdef USE_TRANSPOSITION_TABLE
  // 置換表を初期化
  TT.resize(DEFAULT_TT_SIZE);
#endif

  // 並列探索マネージャーの初期化
  parallelManager = std::make_unique<ParallelSearchManager>();
  parallelManager->initialize();
}

// isreadyコマンドの応答中に呼び出される。時間のかかる処理はここに書くこと。
void Search::clear() {
#ifdef USE_TRANSPOSITION_TABLE
  // 置換表をクリア
  TT.clear();
#endif

  // 並列探索マネージャーのクリア
  if (parallelManager) {
    parallelManager->stop_all_searches();
  }
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

  // 並列探索の開始
  // if (parallelManager) {
  //   int mate_depth = Mate::Utils::calculate_mate_depth(
  //       Limits.time[rootPos.side_to_move()],
  //       Limits.use_time_management() ? 20 : 10
  //   );
  //   parallelManager->start_parallel_search(*pos_ptr,
  //       Limits.depth ? Limits.depth : 20,
  //       Limits.byoyomi[rootPos.side_to_move()]
  //   );
  // }

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
          std::this_thread::sleep_for(std::chrono::milliseconds(9900));
        Stop = true;
      });
    }

    /* 探索開始 - 反復深化探索 */
    Value maxValue = -VALUE_INFINITE; // 初期値はマイナス∞
    Value alpha = -VALUE_INFINITE;
    Value beta = VALUE_INFINITE;
    
    StateInfo si;
    int maxDepth = Limits.depth ? Limits.depth : 20; // goコマンドで指定された深さ、なければ20

    // 反復深化探索
    for (int depth = 1; depth <= maxDepth && !Stop; ++depth) {
      // ノード数制限のチェック
      if (Limits.nodes && Nodes >= (uint64_t)Limits.nodes) {
        Stop = true;
        break;
      }
      Value currentMaxValue = -VALUE_INFINITE;
      Move currentBestMove = MOVE_NONE;

      // 並列探索で各合法手について探索
      /*if (parallelManager && rootMoves.size() > 1 && depth >= 3) {
        // 並列探索（3手目以降、複数の合法手がある場合）
        parallelManager->search_root_moves_parallel(pos, depth, alpha, beta);

        // 並列探索の結果からbestMoveを更新
        for (size_t i = 0; i < rootMoves.size(); ++i) {
          if(chmax(currentMaxValue, rootMoves[i].score)) {
            currentBestMove = rootMoves[i].pv[0];
          }
        }

        // 評価値順にrootMovesをソート
        std::stable_sort(rootMoves.begin(), rootMoves.end());
        std::cout << USI::pv(pos, depth) << std::endl;

        // 詰み探索結果のチェック
        if (parallelManager->check_mate_result()) {
          break; // 詰みが見つかったので探索終了
        }
      } else*/ {
        // 逐次探索（従来通り）
        for (size_t i = 0; i < rootMoves.size(); ++i) {
          Move move = rootMoves[i].pv[0];           // 合法手のi番目
          pos.do_move(move, si);                    // 局面を1手進める
          Value value = VALUE_NONE;
          std::vector<Move> pv;
          // 千日手(5五将棋ルール)は種類ごとの評価値で返す
          // pos.do_move()しているため、評価値の符号に注意
          const RepetitionState &repetitionState = pos.is_repetition(16);
          if (repetitionState != REPETITION_NONE) {
            value = -draw_value(repetitionState, pos.side_to_move());
          } else {
            // 1手進めた状態で探索を行っているため、ply_from_rootは1
            value = (-1) * alphabeta_search(pos, pv, alpha, beta, depth-1, 1); // 指定深さで探索
          }
          if(!Stop) {
            // PVの更新：探索から得られたPVを尊重（破壊しない）
            rootMoves[i].pv.clear();
            rootMoves[i].pv.emplace_back(move);
            if (!pv.empty()) {
                rootMoves[i].pv.insert(rootMoves[i].pv.end(), pv.begin(), pv.end());
            }
            
            // rootMovesのスコアを更新
            rootMoves[i].score = value;
            rootMoves[i].selDepth = depth; // 選択深さを設定
            
          }
          // 局面を1手戻す
          pos.undo_move(move);
          
          if(is_valid_value(value) && chmax(currentMaxValue, value)) {
            currentBestMove = move;
            // [TODO] debug ソートが多すぎるので本来は深化するごとに一回だけ
            // 評価値順にrootMovesをソート
            std::stable_sort(rootMoves.begin(), rootMoves.begin()+i+1);
            std::cout << USI::pv(pos, depth) << std::endl;
          }
        }
      }
      
      if (currentMaxValue > maxValue) {
        maxValue = currentMaxValue;
        bestMove = currentBestMove;
      }
    }
    /* 探索終了 */

    // 並列探索の停止
    if (parallelManager) {
      parallelManager->stop_all_searches();
    }

    // 最終ソートとbestMove更新
    std::stable_sort(rootMoves.begin(), rootMoves.end());
    bestMove = rootMoves[0].pv[0];  // ソート済みの先頭が最善手

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

// アルファ・ベータ法(alpha-beta method)
Value Search::alphabeta_search(Position &pos, std::vector<Move> &pv, Value alpha, Value beta, int depth, int ply_from_root) {
  // 千日手(5五将棋ルール)は種類ごとの評価値で返す
  // pos.do_move()しているため、評価値の符号に注意
  const RepetitionState &repetitionState = pos.is_repetition(16);
  if (repetitionState != REPETITION_NONE) {
    pv.clear();
    return draw_value(repetitionState, pos.side_to_move());
  }

  // 探索ノード数をインクリメント
  Nodes++;

  // 探索打ち切り
  if (Stop) {
    pv.clear();
    return VALUE_NONE;
  }

#ifdef USE_TRANSPOSITION_TABLE
  // 置換表を参照
  bool ttHit;
  TTData ttd(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false, 0);
  TTWriter ttWriter;

  // 置換表を検索
  auto tt_result = TT.probe(pos.key());
  ttHit = std::get<0>(tt_result);
  ttd = std::get<1>(tt_result);
  ttWriter = std::get<2>(tt_result);

  // 置換表にヒットした場合
  if (ttHit) {
    // デバッグ：悪手検出用
    if (depth >= 8 && ttd.bound == BOUND_EXACT && ttd.value < -1000) {
      std::cout << "DEBUG: 置換表から悪手を検出 depth=" << depth
                << " move=" << move_from16(ttd.move)
                << " value=" << ttd.value
                << " gen_diff=" << ((TT.generation() - ttd.generation) & 0x7f)
                << std::endl;
    }

    // 世代チェック：現在か前の世代のエントリのみ使用
    uint8_t current_generation = TT.generation();
    uint8_t entry_generation = ttd.generation;
    uint8_t gen_diff = (current_generation - entry_generation) & 0x7f;

    if (gen_diff <= 1) {  // 現在または前の世代のみ使用
      if (ttd.bound == BOUND_EXACT) {
        pv.assign(1, move_from16(ttd.move));
        return ttd.value;
      } else if (ttd.bound == BOUND_LOWER && ttd.value >= beta) {
        pv.assign(1, move_from16(ttd.move));
        return ttd.value;
      } else if (ttd.bound == BOUND_UPPER && ttd.value <= alpha) {
        return ttd.value;
      }
    }
    // 深さチェックを少し緩和：深さが足りなくても、1手浅いなら許容
    else if (ttd.depth >= depth - 1) {
      if (ttd.bound == BOUND_EXACT) {
        pv.assign(1, move_from16(ttd.move));
        return ttd.value;
      }
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

  const int alphaOrig = alpha;
  for (ExtMove &move : orderedMoves) {
    std::vector<Move> childPv;

    pos.do_move(move.move, si); // 局面を1手進める
    
    // [TODO] ExtMoveにはvalueがあるけどそれを使うべきかを検討(そのまま置換するとエラーが出る)
    Value value = (-1) * alphabeta_search(pos, childPv, -beta, -alpha, depth - 1, ply_from_root + 1); // 再帰的に呼び出し
    
    pos.undo_move(move.move);

    if(!is_valid_value(value)) {
      // 探索打ち切られ
      break;
    }

    // アルファ・ベータカット
    if(value >= beta) {
      // betaカットの場合でも最適なPVを返す
      pv.clear();
      pv.emplace_back(move.move);
      pv.insert(pv.end(), childPv.begin(), childPv.end());
      bestPv = pv;
      maxValue = value;
      break;
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

    if(Stop) break;
  }

#ifdef USE_TRANSPOSITION_TABLE
  // 置換表に探索結果を保存
  if (!Stop) {
    Bound bound;
    if (maxValue >= beta) {
      bound = BOUND_LOWER;
    } else if (maxValue <= alphaOrig){
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
  if(maxValue == -VALUE_INFINITE) {
    // 探索打ち切られている
    return VALUE_NONE;
  }
  return maxValue;
}
// ParallelSearchManagerの実装

Search::ParallelSearchManager::ParallelSearchManager() {
}

Search::ParallelSearchManager::~ParallelSearchManager() {
  stop_all_searches();
}

void Search::ParallelSearchManager::initialize(size_t num_threads) {
  task_manager = std::make_unique<SearchTaskManager>();
  task_manager->initialize(num_threads);
  mate_searcher = std::make_unique<Mate::MateSearcher>();
}

void Search::ParallelSearchManager::start_parallel_search(Position &rootPos, int max_depth, TimePoint time_limit) {
  // 詰み探索の開始
  int mate_depth = Mate::Utils::calculate_mate_depth(time_limit, 20);
  start_mate_search(rootPos, mate_depth);
}

void Search::ParallelSearchManager::search_root_moves_parallel(Position &pos, int depth, Value alpha, Value beta) {
  std::cout << "\n=== 並列α探索開始チェック (depth=" << depth << ", root_moves="
            << Search::rootMoves.size() << ") ===" << std::endl;

  std::cout << "task_manager: " << (task_manager ? "有効" : "null") << std::endl;
  if (task_manager) {
    std::cout << "active_threads: " << task_manager->get_active_threads() << std::endl;
  }
  std::cout << "rootMoves.empty(): " << (Search::rootMoves.empty() ? "true" : "false") << std::endl;

  if (!task_manager || Search::rootMoves.empty()) {
    std::cout << "並列探索をスキップします" << std::endl;
    return;
  }

  std::cout << "\n=== 並列α探索開始 (depth=" << depth << ", root_moves="
            << Search::rootMoves.size() << ") ===" << std::endl;

  // 直接スレッドを生成して並列探索（ThreadPoolの問題を回避）
  size_t num_threads = std::min(task_manager->get_active_threads(), Search::rootMoves.size());
  std::vector<std::thread> threads;
  std::cout << "直接スレッド生成: " << num_threads << "個のスレッドを起動" << std::endl;

  for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
    threads.emplace_back([&, thread_id]() {
      std::cout << "[Thread " << thread_id << "] α探索スレッド開始（直接起動）" << std::endl;

      // 実際の探索処理
      for (size_t i = thread_id; i < Search::rootMoves.size(); i += num_threads) {
        if (Search::Stop || Search::Limits.nodes && Search::Nodes >= (uint64_t)Search::Limits.nodes) {
          break;
        }

        Move move = Search::rootMoves[i].pv[0];
        std::cout << "[Thread " << thread_id << "] rootMove[" << i << "] (" << move << ") を探索開始" << std::endl;

        Position thread_pos = pos; // スレッドごとにコピー
        StateInfo si;

        thread_pos.do_move(move, si);
        std::vector<Move> pv;
        Value value = (-1) * alphabeta_search(thread_pos, pv, alpha, beta, depth-1, 0);
        thread_pos.undo_move(move);

        // 結果の更新（スレッドセーフ）
        Search::rootMoves[i].score = value;
        Search::rootMoves[i].selDepth = depth;
        if (!pv.empty()) {
          Search::rootMoves[i].pv.clear();
          Search::rootMoves[i].pv.emplace_back(move);
          Search::rootMoves[i].pv.insert(Search::rootMoves[i].pv.end(), pv.begin(), pv.end());
        }

        std::cout << "[Thread " << thread_id << "] rootMove[" << i << "] 終了 value=" << value << std::endl;
      }

      std::cout << "[Thread " << thread_id << "] α探索スレッド終了" << std::endl;
    });
  }

  // 全スレッドの完了を待機
  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  std::cout << "=== 並列α探索終了 ===\n" << std::endl;

  // 探索結果のデバッグ表示
  std::cout << "--- 並列探索結果 ---" << std::endl;
  for (size_t i = 0; i < std::min(Search::rootMoves.size(), (size_t)5); ++i) {
    std::cout << "rootMoves[" << i << "] move=" << Search::rootMoves[i].pv[0]
              << " score=" << Search::rootMoves[i].score << std::endl;
  }
  std::cout << "--------------------" << std::endl;

  // 簡単な同期処理（ThreadPoolの問題を回避）
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::cout << "=== 簡易同期完了 ===" << std::endl;
}

void Search::ParallelSearchManager::start_mate_search(Position &rootPos, int mate_depth) {
  std::cout << "\n=== 詰み探索開始チェック (depth=" << mate_depth << ") ===" << std::endl;
  std::cout << "mate_searcher: " << (mate_searcher ? "有効" : "null") << std::endl;
  std::cout << "task_manager: " << (task_manager ? "有効" : "null") << std::endl;

  if (!mate_searcher || mate_depth <= 0) {
    std::cout << "詰み探索をスキップします" << std::endl;
    return;
  }

  std::cout << "\n=== 並列詰み探索開始 (depth=" << mate_depth << ") ===" << std::endl;

  mate_search_active = true;
  mate_searcher->reset();

  // 詰み探索を直接スレッドで実行
  std::thread mate_thread([&]() {
    std::cout << "[Mate Thread] 詰み探索スレッド開始" << std::endl;
    ParallelDebug::g_monitor.mate_search_started(0);

    std::vector<Move> pv;
    Position mate_pos = rootPos; // コピーして使用
    Value mate_value = mate_searcher->search_mate(mate_pos, pv, mate_depth, 0);

    bool found_mate = mate_value > VALUE_ZERO;
    ParallelDebug::g_monitor.mate_search_finished(0, found_mate, mate_depth);

    if (found_mate) {
      // 詰み発見
      latest_mate_result.found = true;
      latest_mate_result.value = mate_value;
      latest_mate_result.depth = mate_depth;
      latest_mate_result.nodes_searched = mate_searcher->get_nodes();
      if (!pv.empty()) {
        latest_mate_result.best_move = pv[0];
        latest_mate_result.pv = pv;
      }
      Search::Stop = true; // 全探索を停止
      std::cout << "*** 詰み発見! 全探索を停止します ***" << std::endl;
    }
    mate_search_active = false;
    std::cout << "[Mate Thread] 詰み探索スレッド終了" << std::endl;
  });

  // 詰み探索スレッドをデタッチ（バックグラウンドで実行）
  mate_thread.detach();

  std::cout << "=== 並列詰み探索終了 ===\n" << std::endl;
}

bool Search::ParallelSearchManager::check_mate_result() {
  return mate_search_active == false && latest_mate_result.found;
}

void Search::ParallelSearchManager::stop_all_searches() {
  Search::Stop = true;

  if (mate_searcher) {
    mate_searcher->stop();
  }

  if (task_manager) {
    task_manager->stop_all_searches();
  }

  mate_search_active = false;
}

Search::ParallelSearchManager::SearchStats Search::ParallelSearchManager::get_search_stats() const {
  SearchStats stats;
  stats.total_nodes = Search::Nodes;
  stats.mate_nodes = mate_searcher ? mate_searcher->get_nodes() : 0;
  stats.active_threads = task_manager ? task_manager->get_active_threads() : 0;
  stats.mate_found = latest_mate_result.found;
  stats.search_time = TimePoint(0); // 実装する場合は計測を追加
  return stats;
}

void Search::ParallelSearchManager::cleanup_searches() {
  if (task_manager) {
    task_manager->stop_all_searches();
  }
}

void Search::ParallelSearchManager::merge_mate_results() {
  if (latest_mate_result.found && !latest_mate_result.pv.empty()) {
    // 詰み結果をrootMovesに反映
    for (auto& rootMove : Search::rootMoves) {
      if (rootMove.pv[0] == latest_mate_result.best_move) {
        rootMove.score = latest_mate_result.value;
        rootMove.pv = latest_mate_result.pv;
        rootMove.selDepth = latest_mate_result.depth;
        break;
      }
    }
  }
}

// SearchTaskManagerの非テンプレート実装
void Search::SearchTaskManager::initialize(size_t num_threads) {
  thread_pool = std::make_unique<Threading::ThreadPool>(num_threads);
}

void Search::SearchTaskManager::stop_all_searches() {
  search_stopped = true;
  if (thread_pool) {
    thread_pool->stop_searching();
  }
}
