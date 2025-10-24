#ifndef _SEARCH_H_
#define _SEARCH_H_

#include "misc.h"
#include "position.h"
#include "tt.h"
#include "mate.h"
#include "thread_pool.h"
#include <vector>
#include <memory>


namespace Search {
// root(探索開始局面)での指し手として使われる。それぞれのroot moveに対して、
// その指し手で進めたときのscore(評価値)とPVを持っている。(PVはfail
// lowしたときには信用できない)
// scoreはnon-pvの指し手では-VALUE_INFINITEで初期化される。
struct RootMove {
  // pv[0]には、コンストラクタの引数で渡されたmを設定する。
  explicit RootMove(Move m) : pv(1, m) {}

  bool operator==(const Move &m) const { return pv[0] == m; }

  bool operator<(const RootMove &m) const {
    return m.score != score ? m.score < score : m.previousScore < previousScore;
  }

  // 今回のスコア
  Value score = -VALUE_INFINITE;

  // 前回のスコア
  Value previousScore = -VALUE_INFINITE;

  // rootから最大、何手目まで探索したか(選択深さの最大)
  int selDepth = 0;

  // この指し手で進めたときのpv
  std::vector<Move> pv;
};

typedef std::vector<RootMove> RootMoves;

// 探索開始局面で思考対象とする指し手の集合。
extern RootMoves rootMoves;

// 今回のgoコマンドでの探索ノード数。
extern std::atomic<uint64_t> Nodes;

// 探索中にこれがtrueになったら探索を即座に終了すること。
extern std::atomic<bool> Stop;

// goコマンドでの探索時に用いる、持ち時間設定などが入った構造体
struct LimitsType {
  LimitsType() {
    time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = movetime =
        TimePoint(0);
    depth = perft = infinite = 0;
    nodes = 0;
    byoyomi[WHITE] = byoyomi[BLACK] = TimePoint(0);
  }

  // 時間制御を行うのか。
  // 詰み専用探索、思考時間0、探索深さが指定されている、探索ノードが指定されている、思考時間無制限
  // であるときは、時間制御に意味がないのでやらない。
  bool use_time_management() const {
    return !(movetime | depth | nodes | perft | infinite);
  }

  // time[]   : 残り時間(ms換算で)
  // inc[]    : 1手ごとに増加する時間(フィッシャールール)
  // movetime : 思考時間固定(0以外が指定してあるなら) : 単位は[ms]
  TimePoint time[COLOR_NB], inc[COLOR_NB], movetime;

  // depth    : 探索深さ固定(0以外を指定してあるなら)
  // perft    : perft(performance
  // test)中であるかのフラグ。非0なら、perft時の深さが入る。 infinite :
  // 思考時間無制限かどうかのフラグ。非0なら無制限。
  int depth, perft, infinite;

  // 今回のgoコマンドでの探索ノード数
  int64_t nodes;

  // 秒読み(ms換算で)
  TimePoint byoyomi[COLOR_NB];
};

extern LimitsType Limits;

// 探索部の初期化
void init();

// 探索部のclear
void clear();

// 探索を開始する
void start_thinking(const Position &rootPos, StateListPtr &states,
                    LimitsType limits);

// 探索本体
void search(Position &rootPos);
Value negamax_search(::Position &pos, std::vector<Move> &pv, int depth, int ply_from_root);
Value alphabeta_search(Position &pos, std::vector<Move> &pv, Value alpha, Value beta, int depth,
                       int ply_from_root);

// 並列探索管理
class ParallelSearchManager;

class SearchTaskManager {
private:
    std::unique_ptr<Threading::ThreadPool> thread_pool;
    std::atomic<bool> search_stopped{false};

public:
    SearchTaskManager() = default;
    ~SearchTaskManager() = default;

    // スレッドプールの初期化
    void initialize(size_t num_threads = std::thread::hardware_concurrency());

    // 探索タスクの実行
    template<class F>
    void run_search_task(const std::string& task_type, F&& f);

    // 全ての探索タスクを停止
    void stop_all_searches();

    // 探索停止フラグの管理
    void set_search_stopped(bool stopped) { search_stopped = stopped; }
    bool is_search_stopped() const { return search_stopped; }

    // アクティブなスレッド数を取得
    size_t get_active_threads() const {
        return thread_pool ? thread_pool->size() : 0;
    }

    // ThreadPoolへのアクセス
    Threading::ThreadPool* get_thread_pool() const {
        return thread_pool.get();
    }
};

class ParallelSearchManager {
private:
    std::unique_ptr<SearchTaskManager> task_manager;
    std::unique_ptr<Mate::MateSearcher> mate_searcher;
    std::atomic<bool> mate_search_active{false};
    Mate::MateResult latest_mate_result;

public:
    ParallelSearchManager();
    ~ParallelSearchManager();

    // 並列探索の初期化
    void initialize(size_t num_threads = std::thread::hardware_concurrency());

    // 並列探索の開始
    void start_parallel_search(Position &rootPos, int max_depth, TimePoint time_limit);

    // ルートノードの並列探索
    void search_root_moves_parallel(Position &pos, int depth, Value alpha, Value beta);

    // 詰み探索の開始
    void start_mate_search(Position &rootPos, int mate_depth);

    // 詰み探索結果の確認
    bool check_mate_result();

    // 全探索の停止
    void stop_all_searches();

    // 統計情報の取得
    struct SearchStats {
        uint64_t total_nodes;
        uint64_t mate_nodes;
        int active_threads;
        bool mate_found;
        TimePoint search_time;
    };

    SearchStats get_search_stats() const;

private:
    void cleanup_searches();
    void merge_mate_results();
};

// グローバルな並列探索マネージャー
extern std::unique_ptr<ParallelSearchManager> parallelManager;

// SearchTaskManagerのテンプレート実装
template<class F>
void Search::SearchTaskManager::run_search_task(const std::string& task_type, F&& f) {
  if (!thread_pool || search_stopped) return;

  thread_pool->run_custom_jobs([&](size_t thread_id) {
    if (search_stopped) return;
    f(thread_id);
  });
}

} // namespace Search

#endif // !SEARCH_H_
