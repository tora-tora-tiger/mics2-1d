#ifndef _PARALLEL_DEBUG_H_
#define _PARALLEL_DEBUG_H_

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include "types.h"

namespace ParallelDebug {

// 並列化の状態を表示するクラス
class ParallelMonitor {
private:
    std::atomic<int> active_mate_searches{0};
    std::atomic<int> active_alpha_searches{0};
    std::chrono::steady_clock::time_point start_time;

public:
    ParallelMonitor() {
        start_time = std::chrono::steady_clock::now();
    }

    // 詰み探索開始
    void mate_search_started(size_t thread_id) {
        active_mate_searches++;
        std::cout << "[Thread " << thread_id << "] 詰み探索開始 (active: "
                  << active_mate_searches << ")" << std::endl;
    }

    // 詰み探索終了
    void mate_search_finished(size_t thread_id, bool found_mate, int depth) {
        active_mate_searches--;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        std::cout << "[Thread " << thread_id << "] 詰み探索終了 - ";
        if (found_mate) {
            std::cout << "詰み発見! depth=" << depth;
        } else {
            std::cout << "詰みなし depth=" << depth;
        }
        std::cout << " (elapsed: " << elapsed << "ms, active: "
                  << active_mate_searches << ")" << std::endl;
    }

    // α探索開始
    void alpha_search_started(size_t thread_id, Move move) {
        active_alpha_searches++;
        std::cout << "[Thread " << thread_id << "] α探索開始 move=" << move
                  << " (active: " << active_alpha_searches << ")" << std::endl;
    }

    // α探索終了
    void alpha_search_finished(size_t thread_id, Move move, Value value) {
        active_alpha_searches--;
        std::cout << "[Thread " << thread_id << "] α探索終了 move=" << move
                  << " value=" << value << " (active: " << active_alpha_searches << ")" << std::endl;
    }

    // 現在の状態表示
    void print_status() {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        std::cout << "\n=== 並列化状態 (経過時間: " << elapsed << "ms) ===" << std::endl;
        std::cout << "Active 詰み探索: " << active_mate_searches << std::endl;
        std::cout << "Active α探索: " << active_alpha_searches << std::endl;
        std::cout << "合計アクティブスレッド: " << (active_mate_searches + active_alpha_searches) << std::endl;
        std::cout << "========================================\n" << std::endl;
    }

    void reset() {
        active_mate_searches = 0;
        active_alpha_searches = 0;
        start_time = std::chrono::steady_clock::now();
    }
};

// グローバルモニター
extern ParallelMonitor g_monitor;

} // namespace ParallelDebug

#endif // !_PARALLEL_DEBUG_H_