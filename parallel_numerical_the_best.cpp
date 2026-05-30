//g++ main.cpp -std=c++20 -O3 -pthread -o app.exe

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <queue>
#include <functional>
#include <condition_variable>

#include "sorting_v3_optimizable.h"

using namespace std;

string file_dir = "test_cases/";


// string filename = "big_unsat.cnf";
string filename = "small_sat.cnf";

// string filename = "uuf75-097.cnf";
// string filename = "uf75-098.cnf";

atomic<bool> SAT{false};
vector<int> res;
mutex res_mutex;

int PARALLEL_DEPTH = 8;
unsigned int MAX_THREADS = thread::hardware_concurrency() ? thread::hardware_concurrency() : 4;


atomic<unsigned long long> counter{0};
unsigned long long treshold = 1e8;
atomic<unsigned long long> I{0};
mutex cout_mutex;



// ======================= Thread pool =========================

class ThreadPool {
public:
    explicit ThreadPool(size_t threads) : stop(false) {
        workers.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this]() {
                for (;;) {
                    std::function<void()> task;
                    {
                        unique_lock<mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]() {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            unique_lock<mutex> lock(queue_mutex);
            if (stop) return;
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& w : workers)
            if (w.joinable()) w.join();
    }

private:
    vector<thread> workers;
    queue<std::function<void()>> tasks;
    mutex queue_mutex;
    condition_variable condition;
    bool stop;
};

// =============================================================

bool multiply_recursive(const vector<vector<int>>& C,
                        unordered_set<int> prev,
                        int i, int clausesCount);

bool multiply(const vector<vector<int>>& C,
              unordered_set<int> prev,
              int i, int clausesCount,
              ThreadPool& pool);

static void gather_prefixes(const vector<vector<int>>& C, int depth, int i,
                            unordered_set<int> prev,
                            vector<pair<unordered_set<int>,int>>& out,
                            int clausesCount)
{
    if (depth <= 0 || i >= clausesCount) {
        out.emplace_back(std::move(prev), i);
        return;
    }

    for (int value : C[i]) {
        if ((int)out.size() >= (int)MAX_THREADS * 8) break;

        int match_value = -value;

        if (!prev.count(match_value)) {
            auto new_prev = prev;
            new_prev.insert(value);
            gather_prefixes(C, depth - 1, i + 1, std::move(new_prev), out, clausesCount);
        }
    }
}

int main() {
    ifstream fin(file_dir + filename);
    if (!fin.is_open()) {
        cerr << "Error: cannot open file\n";
        return 1;
    }

    int vars = 0, clauses = 0;
    string line;

    while (getline(fin, line)) {
        if (!line.empty() && line[0] == 'p') {
            string tmp;
            stringstream ss(line);
            ss >> tmp >> tmp >> vars >> clauses;
            break;
        }
    }

    vector<vector<int>> C(clauses);
    int clauseIndex = 0;
    int maxVar = 0;

    while (getline(fin, line) && clauseIndex < clauses) {
        if (line.empty()) continue;
        if (line[0] == 'c' || line[0] == 'p') continue;

        stringstream ss(line);
        string token;

        while (ss >> token) {
            if (token == "0") break;
            int lit = std::stoi(token);
            C[clauseIndex].push_back(lit);
            maxVar = std::max(maxVar, std::abs(lit));
        }

        if (!C[clauseIndex].empty())
            clauseIndex++;
    }

    fin.close();

    cout << "\nLoaded clauses:\n";
    for (int i = 0; i < clauses; i++) {
        cout << "Clause " << i + 1 << ": ";
        for (int x : C[i]) cout << x << " ";
        cout << "\n";
    }

    vector<vector<int>> newC = orderByConflictClusteringB(C, maxVar);

    cout << "\n==============================\nAfter sorting:\n";
    for (int i = 0; i < clauses; i++) {
        cout << "Clause " << i + 1 << ": ";
        for (int x : newC[i]) cout << x << " ";
        cout << "\n";
    }
    cout << "==============================\n";

    ThreadPool pool(MAX_THREADS);
    cout << std::boolalpha << multiply(newC, {}, 0, clauses, pool) << "\n";

    {
        lock_guard<mutex> lock(res_mutex);
        for (int x : res) cout << x << " ";
        cout << "\n";
    }

    return 0;
}

bool multiply_recursive(const vector<vector<int>>& C,
                        unordered_set<int> prev,
                        int i, int clausesCount)
{
    if (SAT.load()) return true;

    if (i >= clausesCount) {
        bool expected = false;
        if (SAT.compare_exchange_strong(expected, true)) {
            lock_guard<mutex> lock(res_mutex);
            res.assign(prev.begin(), prev.end());
        }
        return true;
    }

    for (int value : C[i]) {
    if (SAT.load()) return true;

    if (i == 0) {
        I.fetch_add(1, memory_order_relaxed);
    }

    unsigned long long current = counter.fetch_add(1, memory_order_relaxed) + 1;
    if (current > treshold) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Counter: " << I.load(memory_order_relaxed)
             << " / " << clausesCount << "\n";
        counter.store(0, memory_order_relaxed);
    }

    int match_value = -value;

    if (!prev.count(match_value)) {
        auto new_prev = prev;
        new_prev.insert(value);
        if (multiply_recursive(C, std::move(new_prev), i + 1, clausesCount))
            return true;
    }
}

    return false;
}

bool multiply(const vector<vector<int>>& C,
              unordered_set<int> prev,
              int i, int clausesCount,
              ThreadPool& pool)
{
    if (SAT.load()) return true;

    if (i >= clausesCount) {
        bool expected = false;
        if (SAT.compare_exchange_strong(expected, true)) {
            lock_guard<mutex> lock(res_mutex);
            res.assign(prev.begin(), prev.end());
        }
        return true;
    }

    if (i == 0) {
        vector<pair<unordered_set<int>,int>> starters;
        gather_prefixes(C, PARALLEL_DEPTH, 0, prev, starters, clausesCount);

        if (starters.empty()) return false;
        if (starters.size() == 1)
            return multiply_recursive(C, std::move(starters[0].first), starters[0].second, clausesCount);

        vector<std::future<bool>> futures;
        futures.reserve(starters.size());

        for (auto &st : starters) {
            auto task_ptr = std::make_shared<std::packaged_task<bool()>>(
                [&, st]() mutable {
                    return multiply_recursive(C, std::move(st.first), st.second, clausesCount);
                }
            );
            futures.emplace_back(task_ptr->get_future());
            pool.enqueue([task_ptr]() { (*task_ptr)(); });
        }

        for (auto& fut : futures) {
            if (fut.get()) return true;
            if (SAT.load()) return true;
        }

        return false;
    }

    return multiply_recursive(C, std::move(prev), i, clausesCount);
}
