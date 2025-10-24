#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Threading {

// やねurao王風のスレッド管理クラス
class Thread {
private:
    std::mutex mutex;
    std::condition_variable cv;
    std::function<void()> jobFunc;
    std::atomic<bool> searching{false};
    std::atomic<bool> exit{false};
    std::thread native_thread;

public:
    // ThreadPoolがnative_threadにアクセスするためのfriend宣言
    friend class ThreadPool;

public:
    explicit Thread(size_t thread_id);
    ~Thread();

    // スレッドの起動
    void start();

    // カスタムジョブの実行
    void run_custom_job(std::function<void()> f);

    // 探索中かどうかのチェック
    bool is_searching() const { return searching; }

    // スレッドの停止
    void terminate();

private:
    void idle_loop();
    size_t thread_id;
};

// スレッドプール（やねurao王風の設計）
class ThreadPool {
private:
    std::vector<std::unique_ptr<Thread>> threads;
    std::atomic<bool> search_running{false};
    std::atomic<size_t> active_threads{0};

    // スレッド同期用
    std::mutex threads_mutex;
    std::condition_variable search_finished_cv;

public:
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // スレッド数の設定
    void set_size(size_t num_threads);

    // 現在のスレッド数を取得
    size_t size() const { return threads.size(); }

    // 全スレッドで探索を開始
    template<class Func>
    void start_searching(Func search_func);

    // 全スレッドの探索完了を待機
    void wait_for_search_finished();

    // 全探索の停止
    void stop_searching();

    // 全スレッドでカスタムジョブを実行
    template<class Func>
    void run_custom_jobs(Func job_func);

private:
    void create_threads(size_t num_threads);
    void destroy_threads();
};

// スレッドセーフなカウンタ（やねurao王風）
template<typename T>
class ThreadSafeCounter {
private:
    std::atomic<T> value{0};

public:
    ThreadSafeCounter() = default;
    explicit ThreadSafeCounter(T initial_value) : value(initial_value) {}

    // 原子的なインクリメント
    T operator++() { return ++value; }
    T operator++(int) { return value++; }

    // 原子的なデクリメント
    T operator--() { return --value; }
    T operator--(int) { return value--; }

    // 値の取得
    T get() const { return value; }

    // 値の設定
    void set(T new_value) { value = new_value; }

    // リセット
    void reset() { value = 0; }

    // 比較演算子
    bool operator==(T other) const { return value == other; }
    bool operator!=(T other) const { return value != other; }
    bool operator<(T other) const { return value < other; }
    bool operator<=(T other) const { return value <= other; }
    bool operator>(T other) const { return value > other; }
    bool operator>=(T other) const { return value >= other; }
};

// 探索タスク用の同期機構
class SearchSync {
private:
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> search_ended{false};
    std::atomic<int> completed_threads{0};
    int total_threads{0};

public:
    explicit SearchSync(int num_threads) : total_threads(num_threads) {}

    // スレッド完了の通知
    void thread_completed();

    // 全スレッドの完了を待機
    void wait_for_all_threads();

    // 検索終了の通知
    void end_search();

    // 検索が終了したかチェック
    bool is_search_ended() const { return search_ended; }

    // 状態のリセット
    void reset(int num_threads);
};

// スレッドローカルストレージ（やねurao王風）
template<typename T>
class ThreadLocal {
private:
    static thread_local std::unordered_map<std::string, T> storage;

public:
    // 値の取得
    static T& get(const std::string& key) {
        return storage[key];
    }

    // 値の設定
    static void set(const std::string& key, const T& value) {
        storage[key] = value;
    }

    // 値が存在するかチェック
    static bool has(const std::string& key) {
        return storage.find(key) != storage.end();
    }

    // 値の削除
    static void erase(const std::string& key) {
        storage.erase(key);
    }

    // 全クリア
    static void clear() {
        storage.clear();
    }
};

// タスクディスパッチャ（やねurao王風）
class TaskDispatcher {
private:
    ThreadPool& thread_pool;
    std::atomic<bool> stopped{false};

public:
    explicit TaskDispatcher(ThreadPool& pool) : thread_pool(pool) {}

    // 並列タスクの実行
    template<class TaskFunc, class ResultFunc>
    void dispatch_parallel_tasks(const std::vector<TaskFunc>& tasks, ResultFunc result_handler);

    // 停止フラグの設定
    void stop() { stopped = true; }

    // 停止フラグのリセット
    void reset() { stopped = false; }

    // 停止中かチェック
    bool is_stopped() const { return stopped; }
};

} // namespace Threading

// テンプレート関数の実装
template<class Func>
void Threading::ThreadPool::start_searching(Func search_func) {
    search_running = true;
    active_threads = threads.size();

    // 全スレッドで探索関数を実行
    run_custom_jobs(search_func);
}

template<class Func>
void Threading::ThreadPool::run_custom_jobs(Func job_func) {
    if (threads.empty()) return;

    search_running = true;
    active_threads = threads.size();

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->run_custom_job([&, i]() {
            job_func(i);

            // 探索完了通知
            if (--active_threads == 0) {
                std::unique_lock<std::mutex> lock(threads_mutex);
                search_running = false;
                search_finished_cv.notify_all();
            }
        });
    }
}

template<class TaskFunc, class ResultFunc>
void Threading::TaskDispatcher::dispatch_parallel_tasks(
    const std::vector<TaskFunc>& tasks,
    ResultFunc result_handler) {

    if (tasks.empty()) return;

    std::vector<std::tuple<decltype(tasks[0]()), size_t>> results(tasks.size());

    thread_pool.run_custom_jobs([&](size_t thread_id) {
        for (size_t i = thread_id; i < tasks.size(); i += thread_pool.size()) {
            if (is_stopped()) break;

            results[i] = std::make_tuple(tasks[i](), i);
        }
    });

    thread_pool.wait_for_search_finished();

    // 結果の処理
    for (auto& result : results) {
        if (std::get<1>(result) < tasks.size()) {
            result_handler(std::get<0>(result), std::get<1>(result));
        }
    }
}

// スレッドローカルストレージの実装
template<typename T>
thread_local std::unordered_map<std::string, T> Threading::ThreadLocal<T>::storage;

#endif // !_THREAD_POOL_H_