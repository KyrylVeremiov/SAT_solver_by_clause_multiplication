#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include "sorting_clauses_num.h"
// #include "sorting_clauses_greedy.h"
#include "sorting_clauses_greedy_v2.h"
// #include <unordered_set>
using namespace std;

// string filename = "small_sat.cnf";
// string filename = "small.cnf";


string file_dir = "test_cases/";


// string filename = "big_unsat.cnf";
// string filename = "big_sat.cnf";

// string filename = "uf75-098.cnf";
string filename = "uf75-098.cnf";





atomic<unsigned long long> counter{0};
unsigned long long  treshold=1e7;
atomic<unsigned long long> I{0};

atomic<bool> SAT{false};
vector<string> res;
mutex res_mutex;
mutex cout_mutex;

bool multiply_recursive(const vector<vector<string>>& C, vector<string> prev, int i, int clausesCount);
bool multiply(const vector<vector<string>>& C, vector<string> prev, int i, int clausesCount);

// Parallelism tuning
int PARALLEL_DEPTH = 8; // increase to parallelize deeper levels
unsigned int MAX_TASKS = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;

// Gather initial prefixes up to `depth` levels to spawn tasks for
static void gather_prefixes(const vector<vector<string>>& C, int depth, int i, vector<string> prev, vector<pair<vector<string>,int>>& out, int clausesCount) {
    if (depth <= 0 || i >= clausesCount) {
        out.emplace_back(std::move(prev), i);
        return;
    }

    for (const string& value : C[i]) {
        if ((int)out.size() >= (int)MAX_TASKS * 8) break; // cap number of starters

        string match_value = (value.size() && value[0] == '-') ? value.substr(1) : string("-") + value;
        if (std::find(prev.begin(), prev.end(), match_value) == prev.end()) {
            vector<string> new_prev = prev;
            new_prev.push_back(value);
            gather_prefixes(C, depth - 1, i + 1, std::move(new_prev), out, clausesCount);
        }
    }
}

int main() {
    // Fixed filename instead of user input

    ifstream fin(file_dir+filename);
    if (!fin.is_open()) {
        cerr << "Error: cannot open file\n";
        return 1;
    }

    int vars = 0, clauses = 0;
    string line;

    // Search for the "p cnf" line to read number of variables and clauses
    while (getline(fin, line)) {
        if (!line.empty() && line[0] == 'p') {
            string tmp;
            stringstream ss(line);
            ss >> tmp >> tmp >> vars >> clauses;
            break;
        }
    }

    // Create a vector of vectors (one vector per clause)
    vector<vector<string>> C(clauses);

    int clauseIndex = 0;

    // Read the rest of the file
    while (getline(fin, line) && clauseIndex < clauses) {
        if (line.empty()) continue;
        if (line[0] == 'c' || line[0] == 'p') continue; // skip comments and header

        stringstream ss(line);
        string token;

        // Read literals as strings until "0"
        while (ss >> token) {
            if (token == "0") break;
            C[clauseIndex].push_back(token);
        }

        if (!C[clauseIndex].empty())
            clauseIndex++;
    }

    fin.close();

    // Output for verification
    cout << "\nLoaded clauses:\n";
    for (int i = 0; i < clauses; i++) {
        cout << "Clause " << i + 1 << ": ";
        for (const string& s : C[i]) {
            cout << s << " ";
        }
        cout << "\n";
    }


    
    // unordered_set<string>* newC = std::unordered_set<int> C(s.begin(), s.end());;

    //  vector<set<string>> newC=sortClausesByConflicts(C);
    //  vector<set<string>> newC=orderByConflictClustering(C);
    vector<vector<string>> newC=orderByConflictClusteringB(C);
    
    cout << multiply(newC, {}, 0, clauses) << "\n";

    for (const string& s : res) {
        cout << s << " ";
    }
    cout << "\n";

    return 0;
}





bool multiply_recursive(const vector<vector<string>>& C, vector<string> prev, int i, int clausesCount) {

    if (SAT.load(memory_order_relaxed)) {
        return true;
    }

    if (i >= clausesCount) {
        bool expected = false;
        if (SAT.compare_exchange_strong(expected, true, memory_order_acq_rel)) {
            lock_guard<mutex> lock(res_mutex);
            res = std::move(prev);
        }
        return true;
    }

    for (const string& value : C[i]) {
        if (SAT.load(memory_order_relaxed)) {
            return true;
        }

        if (i == 0) {
            I.fetch_add(1, memory_order_relaxed);
        }
        string match_value;

        if (value[0] == '-') {
            match_value = value.substr(1);
        }
        else {
            match_value = "-" + value;
        }

        int current = counter.fetch_add(1, memory_order_relaxed) + 1;
        if (current > treshold) {
            lock_guard<mutex> lock(cout_mutex);
            cout << "Counter: " << I.load(memory_order_relaxed) << " / " << clausesCount << "\n";
            counter.store(0, memory_order_relaxed);
        }

        if (std::find(prev.begin(), prev.end(), match_value) == prev.end()) {
            vector<string> new_prev = prev;
            new_prev.push_back(value);
            bool next = multiply_recursive(C, std::move(new_prev), i + 1, clausesCount);
            if (next) {
                return true;
            }
        }
    }
    return false;
}

bool multiply(const vector<vector<string>>& C, vector<string> prev, int i, int clausesCount) {
    if (SAT.load(memory_order_relaxed)) {
        return true;
    }

    if (i >= clausesCount) {
        bool expected = false;
        if (SAT.compare_exchange_strong(expected, true, memory_order_acq_rel)) {
            lock_guard<mutex> lock(res_mutex);
            res = std::move(prev);
        }
        return true;
    }

    if (i == 0) {
        // build initial prefixes up to PARALLEL_DEPTH levels
        vector<pair<vector<string>,int>> starters;
        gather_prefixes(C, PARALLEL_DEPTH, 0, prev, starters, clausesCount);

        if (starters.empty()) return false;
        if (starters.size() == 1) {
            // nothing to parallelize beyond single branch
            return multiply_recursive(C, std::move(starters[0].first), starters[0].second, clausesCount);
        }

        vector<future<bool>> futures;
        futures.reserve(starters.size());
        for (auto &st : starters) {
            futures.emplace_back(std::async(std::launch::async,
                [&, st]() mutable {
                    return multiply_recursive(C, std::move(st.first), st.second, clausesCount);
                }
            ));
        }

        for (auto& fut : futures) {
            if (fut.get()) return true;
            if (SAT.load(memory_order_relaxed)) return true;
        }
        return false;
    }

    return multiply_recursive(C, std::move(prev), i, clausesCount);
}