#include "mate.h"
#include "evaluate.h"
#include "misc.h"
#include <algorithm>
#include <chrono>

namespace Mate {

// グローバル統計
Mate::MateSearcher::MateStats global_mate_stats;

// MateSearcherクラスの実装
Value MateSearcher::search_mate(Position &pos, std::vector<Move> &pv, int depth, int ply_from_root) {
    // mukou
    return -VALUE_INFINITE;
    ++nodes;

    if (should_stop()) {
        pv.clear();
        return VALUE_ZERO;
    }

    // 深さ0なら詰み判定のみ
    if (depth == 0) {
        pv.clear();
        return is_mated_position(pos) ? mated_in(ply_from_root) : VALUE_ZERO;
    }

    // 早期詰み判定
    if (is_obvious_mate(pos)) {
        pv.clear();
        return mate_in(ply_from_root + 1);
    }

    // 王手のみを生成
    auto check_moves = MoveList<CHECKS>(pos);
    if (check_moves.size() == 0) {
        pv.clear();
        return VALUE_ZERO; // 王手なしで詰みなし
    }

    Value best_value = -VALUE_INFINITE;
    std::vector<Move> best_pv;
    StateInfo si;

    // 王手を評価値順にソート（簡易的なオーダリング）
    std::vector<std::pair<Move, Value>> ordered_moves;
    for (const auto& move : check_moves) {
        Value move_value = VALUE_ZERO;
        // 簡単な評価：captureなら優先（簡易実装）
        if (move.move != MOVE_NONE) {
            move_value = Value(1);
        }
        ordered_moves.emplace_back(move.move, move_value);
    }

    std::sort(ordered_moves.begin(), ordered_moves.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [move, _] : ordered_moves) {
        if (should_stop()) break;

        std::vector<Move> child_pv;

        pos.do_move(move, si);

        // 相手の逃げ手を考慮
        if (!pos.in_check()) {
            // 逃げ手があるかチェック
            auto evasion_moves = MoveList<EVASIONS>(pos);
            if (evasion_moves.size() == 0) {
                // 逃げ手なし → 詰み
                pos.undo_move(move);
                pv.clear();
                pv.emplace_back(move);
                return mate_in(ply_from_root + 1);
            }
        }

        Value value = -search_mate_recursive(pos, child_pv, depth - 1, ply_from_root + 1, -best_value);
        pos.undo_move(move);

        if (value > best_value) {
            best_value = value;
            best_pv.clear();
            best_pv.emplace_back(move);
            best_pv.insert(best_pv.end(), child_pv.begin(), child_pv.end());
        }

        // 詰みが見つかったら即座に返す
        if (value > VALUE_ZERO) {
            pv = best_pv;
            return value;
        }
    }

    pv = best_pv;
    return best_value;
}

Value MateSearcher::search_mate_recursive(Position &pos, std::vector<Move> &pv,
                                         int depth, int ply_from_root, Value alpha) {
    ++nodes;

    if (should_stop()) {
        pv.clear();
        return VALUE_ZERO;
    }

    // 深さ制限
    if (depth <= 0) {
        pv.clear();
        return VALUE_ZERO; // この深さでは詰みを判定しない
    }

    // 早期千日手チェック
    if (pos.is_repetition(16)) {
        pv.clear();
        return VALUE_ZERO;
    }

    // 王手されている場合
    if (pos.in_check()) {
        // 詰みチェック
        auto evasion_moves = MoveList<EVASIONS>(pos);
        if (evasion_moves.size() == 0) {
            pv.clear();
            return mated_in(ply_from_root);
        }

        // 逃げ手の中に詰み回避があるかチェック
        StateInfo si;
        for (const auto& move : evasion_moves) {
            if (should_stop()) break;

            std::vector<Move> child_pv;
            pos.do_move(move.move, si);

            // 相手の攻撃手を探索
            auto check_moves = MoveList<CHECKS>(pos);
            bool has_escape = true;

            if (!check_moves.size() == 0) {
                Value escape_value = -search_mate_recursive(pos, child_pv, depth - 1, ply_from_root + 1, -alpha);
                if (escape_value <= VALUE_ZERO) {
                    has_escape = false; // 逃げ成功
                }
            }

            pos.undo_move(move.move);

            if (has_escape) {
                pv.clear();
                return VALUE_ZERO; // 逃げ手あり
            }
        }

        // 逃げ手なし
        pv.clear();
        return mated_in(ply_from_root);
    }

    // 王手を生成して探索
    auto check_moves = MoveList<CHECKS>(pos);
    if (check_moves.size() == 0) {
        pv.clear();
        return VALUE_ZERO; // 王手なし
    }

    Value best_value = -VALUE_INFINITE;
    std::vector<Move> best_pv;
    StateInfo si;

    for (const auto& move : check_moves) {
        if (should_stop()) break;

        std::vector<Move> child_pv;
        pos.do_move(move.move, si);

        // 相手の応手を探索
        Value value = -search_mate_recursive(pos, child_pv, depth - 1, ply_from_root + 1, -alpha);
        pos.undo_move(move.move);

        if (value > best_value) {
            best_value = value;
            best_pv.clear();
            best_pv.emplace_back(move.move);
            best_pv.insert(best_pv.end(), child_pv.begin(), child_pv.end());
        }

        if (value > alpha) {
            alpha = value;
        }

        // 詰みが見つかった
        if (value > VALUE_ZERO) {
            pv = best_pv;
            return value;
        }
    }

    pv = best_pv;
    return best_value;
}

bool MateSearcher::is_mate_in_n(Position &pos, int n) {
    if (n <= 0) return false;

    std::vector<Move> pv;
    Value result = search_mate(pos, pv, n, 0);
    return result > VALUE_ZERO;
}

MoveList<CHECKS> MateSearcher::generate_check_moves(const Position &pos) {
    return MoveList<CHECKS>(pos);
}

bool MateSearcher::is_obvious_mate(const Position &pos) {
    // 簡単な詰み判定
    // 例：王が1手で詰む場合など

    if (!pos.in_check()) {
        return false;
    }

    // 逃げ手のチェック
    auto evasion_moves = MoveList<EVASIONS>(pos);
    return evasion_moves.size() == 0;
}

bool MateSearcher::is_mated_position(const Position &pos) {
    return pos.in_check() && MoveList<EVASIONS>(pos).size() == 0;
}

bool MateSearcher::is_effective_check(const Position &pos, Move move) {
    // 簡単な実装：合法手なら実効的な王手とみなす
    return pos.legal(move);
}

// Utils名前空間の実装
namespace Utils {

int calculate_mate_depth(TimePoint remaining_time, int time_divisor) {
    if (remaining_time <= 0) return 3;

    // 残り時間に応じて詰み探索深さを調整
    int max_time_per_search = remaining_time / time_divisor;

    if (max_time_per_search < 100) return 3;      // 100ms未満: 3手詰みまで
    if (max_time_per_search < 1000) return 5;     // 1秒未満: 5手詰みまで
    if (max_time_per_search < 5000) return 7;     // 5秒未満: 7手詰みまで
    return 9;                                      // それ以上: 9手詰みまで
}

bool should_search_mate(const Position &pos, int game_phase) {
    // 王手されている場合は詰み探索の価値が高い
    if (pos.in_check()) return true;

    // 盤面の駒の配置によって詰みの可能性を判断
    // ここでは簡易的にgame_phaseを使用
    return game_phase >= 2; // 中盤〜終盤で詰み探索
}

int calculate_mate_priority(const Position &pos, Value current_eval) {
    int priority = 0;

    // 評価値が極端な場合に詰み探索の優先度を上げる
    if (current_eval > VALUE_MATE_IN_MAX_PLY) {
        priority += 100; // 自分が勝っている
    } else if (current_eval < -VALUE_MATE_IN_MAX_PLY) {
        priority += 100; // 相手が勝っている
    }

    // 王手されている場合
    if (pos.in_check()) {
        priority += 50;
    }

    return priority;
}

} // namespace Utils

} // namespace Mate