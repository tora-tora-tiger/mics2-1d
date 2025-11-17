#include "thread_pool.h"
#include <chrono>
#include <iostream>

namespace Threading {

// Threadクラスの実装
Thread::Thread(size_t id) : thread_id(id) {
    native_thread = std::thread(&Thread::idle_loop, this);
}

Thread::~Thread() {
    terminate();
    if (native_thread.joinable()) {
        native_thread.join();
    }
}

void Thread::start() {
    // スレッドはコンストラクタで自動的に開始される
}

void Thread::run_custom_job(std::function<void()> f) {
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&]{ return !searching; });

    jobFunc = std::move(f);
    searching = true;
    cv.notify_one();
}

void Thread::terminate() {
    {
        std::unique_lock<std::mutex> lk(mutex);
        exit = true;
        searching = true;
    }
    cv.notify_one();
}

void Thread::idle_loop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mutex);
        searching = false;
        cv.notify_one(); // 起動を待っているスレッドを起こす

        cv.wait(lk, [&]()->bool { return searching.load(); });

        if (exit) {
            break;
        }

        // ジョブがあれば実行
        if (jobFunc) {
            jobFunc();
            jobFunc = nullptr;
        }
    }
}

// ThreadPoolクラスの実装
ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads > 0) {
        create_threads(num_threads);
    }
}

ThreadPool::~ThreadPool() {
    destroy_threads();
}

void ThreadPool::set_size(size_t num_threads) {
    if (num_threads == size()) {
        return; // 変更不要
    }

    destroy_threads();
    if (num_threads > 0) {
        create_threads(num_threads);
    }
}

// テンプレート関数の明示的実装化は不要（ヘッダで実装）

void ThreadPool::wait_for_search_finished() {
    if (!search_running) {
        return;
    }

    std::unique_lock<std::mutex> lock(threads_mutex);
    search_finished_cv.wait(lock, [&]{ return !search_running; });
}

void ThreadPool::stop_searching() {
    search_running = false;

    // 全スレッドに停止通知
    for (auto& thread : threads) {
        thread->run_custom_job([]{}); // 空のジョブで割り込み
    }
}

// テンプレート関数の明示的実装化は不要（ヘッダで実装）

void ThreadPool::create_threads(size_t num_threads) {
    threads.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(std::make_unique<Thread>(i));
    }

    // 全スレッドの起動完了を待機
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void ThreadPool::destroy_threads() {
    if (!threads.empty()) {
        stop_searching();

        // 全スレッドを終了
        for (auto& thread : threads) {
            thread->terminate();
        }

        // スレッドの終了を待機
        for (auto& thread : threads) {
            std::thread* native = &thread->native_thread;
            if (native->joinable()) {
                native->join();
            }
        }

        threads.clear();
    }
}

// SearchSyncクラスの実装
void SearchSync::thread_completed() {
    std::unique_lock<std::mutex> lock(mutex);

    if (++completed_threads >= total_threads) {
        search_ended = true;
        cv.notify_all();
    }
}

void SearchSync::wait_for_all_threads() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]()->bool { return search_ended.load(); });
}

void SearchSync::end_search() {
    std::unique_lock<std::mutex> lock(mutex);
    search_ended = true;
    cv.notify_all();
}

void SearchSync::reset(int num_threads) {
    std::unique_lock<std::mutex> lock(mutex);
    total_threads = num_threads;
    completed_threads = 0;
    search_ended = false;
}

} // namespace Threading