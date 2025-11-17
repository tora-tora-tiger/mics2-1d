#ifndef _MATE_H_
#define _MATE_H_

#include "position.h"
#include "types.h"
#include "misc.h"
#include <vector>
#include <atomic>
#include <unordered_map>

namespace Mate {

// 詰み探索結果
struct MateResult {
    bool found;                    // 詰みが見つかったか
    Value value;                   // 詰みの評価値
    Move best_move;                // 最善の詰み手
    std::vector<Move> pv;          // PV
    int depth;                     // 見つかった詰みの深さ
    uint64_t nodes_searched;       // 探索ノード数

    MateResult() : found(false), value(VALUE_ZERO), best_move(MOVE_NONE),
                   depth(0), nodes_searched(0) {}
};

// 詰み探索クラス
class MateSearcher {
private:
    std::atomic<bool> stop_flag{false};
    std::atomic<uint64_t> nodes{0};

public:
    // 単純な詰み探索
    Value search_mate(Position &pos, std::vector<Move> &pv, int depth, int ply_from_root);

    // N手詰みチェック
    bool is_mate_in_n(Position &pos, int n);

    // 王手手生成
    MoveList<CHECKS> generate_check_moves(const Position &pos);

    // 早期詰み判定
    bool is_obvious_mate(const Position &pos);

    // 停止フラグ管理
    void stop() { stop_flag = true; }
    void reset() { stop_flag = false; nodes = 0; }
    bool should_stop() const { return stop_flag; }

    // ノード数取得
    uint64_t get_nodes() const { return nodes; }

      // 詰み探索の統計情報（スレッドセーフ）
    struct MateStats {
        std::atomic<uint64_t> total_searches{0};
        std::atomic<uint64_t> mates_found{0};
        std::atomic<uint64_t> positions_checked{0};
        std::atomic<double> average_depth{0.0};
    };

private:
    // 再帰的な詰み探索（内部使用）
    Value search_mate_recursive(Position &pos, std::vector<Move> &pv,
                               int depth, int ply_from_root, Value alpha);

    // 局面の詰み判定
    bool is_mated_position(const Position &pos);

    // 実効的な王手かどうかの判定
    bool is_effective_check(const Position &pos, Move move);
};

// 詰み探索のユーティリティ関数
namespace Utils {
    // 持ち時間から詰み探索深さを決定
    int calculate_mate_depth(TimePoint remaining_time, int time_divisor = 20);

    // 詰み探索を行うべき局面か判定
    bool should_search_mate(const Position &pos, int game_phase);

    // 詰み探索の優先度を計算
    int calculate_mate_priority(const Position &pos, Value current_eval);
}

// グローバルな詰み探索統計
extern Mate::MateSearcher::MateStats global_mate_stats;

} // namespace Mate

#endif // !MATE_H_