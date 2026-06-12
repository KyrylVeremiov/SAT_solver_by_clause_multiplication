//Iterative version of filling DNF forest, with some optimizations. 
// The first option is to take a clause with the most amount of confilcts with all patterns and insert it.
// The second opion to peack at each step a clause with the highest average conflict fraction,
//  then insert it and update the tree.
//  

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include "../sorting_clauses_min_index.h"
#include "filling_sorting.h"

using namespace std;
namespace fs = std::filesystem;

struct ThreadPool {
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> tasks;
    std::mutex tasksMutex;
    std::condition_variable taskAvailable;
    std::condition_variable doneCondition;
    std::atomic<size_t> activeTasks{0};
    bool stop = false;

    explicit ThreadPool(size_t threadCount) {
        threadCount = std::max<size_t>(1, threadCount);
        workers.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(tasksMutex);
                        taskAvailable.wait(lock, [this]() {
                            return stop || !tasks.empty();
                        });
                        if (stop && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop_front();
                        activeTasks.fetch_add(1, std::memory_order_relaxed);
                    }
                    task();
                    {
                        std::unique_lock<std::mutex> lock(tasksMutex);
                        activeTasks.fetch_sub(1, std::memory_order_relaxed);
                        if (tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0)
                            doneCondition.notify_all();
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(tasksMutex);
            stop = true;
        }
        taskAvailable.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable())
                worker.join();
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(tasksMutex);
            tasks.emplace_back(std::move(task));
        }
        taskAvailable.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(tasksMutex);
        doneCondition.wait(lock, [this]() {
            return tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0;
        });
    }
};

struct Literal {
    int var;
    bool val;
};

struct Node {
    int var;
    vector<pair<int, Node*>> childTrue;
    vector<pair<int, Node*>> childFalse;
};

using Clause = vector<Literal>;

void freeTree(Node* node);

#include "iterative_sorting.h"

Node* anyNode = nullptr;
Node* root    = nullptr;

bool hasEmptyPattern = true;

inline bool literalLess(const Literal& a, const Literal& b) {
    return a.var < b.var;
}

inline bool containsVar(const Clause& pat, int var) {
    auto it = lower_bound(pat.begin(), pat.end(), var,
                          [](const Literal& lit, int value) {
                              return lit.var < value;
                          });
    return it != pat.end() && it->var == var;
}

Clause addLiteralSorted(const Clause& pat, Literal lit) {
    Clause res;
    res.reserve(pat.size() + 1);
    auto it = lower_bound(pat.begin(), pat.end(), lit,
                          [](const Literal& a, const Literal& b) {
                              return a.var < b.var;
                          });
    res.insert(res.end(), pat.begin(), it);
    res.push_back(lit);
    res.insert(res.end(), it, pat.end());
    return res;
}

static Node* findChild(Node* node, int var, bool val) {
    auto& children = val ? node->childTrue : node->childFalse;
    for (auto& [k, child] : children) {
        if (k == var)
            return child;
    }
    return nullptr;
}

static Node* insertChild(Node* node, int var, bool val) {
    auto& children = val ? node->childTrue : node->childFalse;
    for (auto& [k, child] : children) {
        if (k == var)
            return child;
    }
    Node* nxt = new Node{var};
    children.emplace_back(var, nxt);
    return nxt;
}

void insertPattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = true;
        return;
    }

    Node* cur = root;
    for (auto& lit : pat) {
        cur = insertChild(cur, lit.var, lit.val);
    }

    auto& children = pat.back().val ? cur->childTrue : cur->childFalse;
    for (auto& [k, child] : children) {
        if (k == -1)
            return;
    }
    children.emplace_back(-1, anyNode);
}

bool removePatternRec(Node* node, const Clause& pat, size_t pos) {
    if (pos == pat.size()) {
        auto& children = pat.back().val ? node->childTrue : node->childFalse;
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i].first == -1) {
                children.erase(children.begin() + i);
                break;
            }
        }
        return node->childTrue.empty() && node->childFalse.empty();
    }

    auto& children = pat[pos].val ? node->childTrue : node->childFalse;
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i].first == pat[pos].var) {
            Node* child = children[i].second;
            if (removePatternRec(child, pat, pos + 1)) {
                freeTree(child);
                children.erase(children.begin() + i);
            }
            break;
        }
    }
    return node->childTrue.empty() && node->childFalse.empty();
}

void removePattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = false;
        return;
    }

    removePatternRec(root, pat, 0);
}

void collectPatterns(Node* node, vector<Literal>& cur, vector<Clause>& out) {
    for (auto& [var, child] : node->childTrue) {
        if (var == -1 && child == anyNode) {
            out.push_back(cur);
        } else {
            cur.push_back({var, true});
            collectPatterns(child, cur, out);
            cur.pop_back();
        }
    }

    for (auto& [var, child] : node->childFalse) {
        if (var == -1 && child == anyNode) {
            out.push_back(cur);
        } else {
            cur.push_back({var, false});
            collectPatterns(child, cur, out);
            cur.pop_back();
        }
    }
}

static void collectPatternsFromChild(Node* child, int var, bool val, vector<Clause>& out) {
    vector<Literal> cur;
    cur.push_back({var, val});
    collectPatterns(child, cur, out);
}

vector<Clause> getAllPatterns(ThreadPool& pool) {
    vector<Clause> patterns;
    if (hasEmptyPattern)
        patterns.push_back(Clause{});

    size_t totalChildren = root->childTrue.size() + root->childFalse.size();
    if (totalChildren == 0)
        return patterns;

    if (totalChildren == 1) {
        if (!root->childTrue.empty())
            collectPatternsFromChild(root->childTrue[0].second, root->childTrue[0].first, true, patterns);
        else
            collectPatternsFromChild(root->childFalse[0].second, root->childFalse[0].first, false, patterns);
        return patterns;
    }

    vector<vector<Clause>> partialResults(totalChildren);
    size_t taskIndex = 0;
    for (auto& [var, child] : root->childTrue) {
        size_t index = taskIndex++;
        pool.enqueue([&partialResults, index, child, var]() {
            collectPatternsFromChild(child, var, true, partialResults[index]);
        });
    }
    for (auto& [var, child] : root->childFalse) {
        size_t index = taskIndex++;
        pool.enqueue([&partialResults, index, child, var]() {
            collectPatternsFromChild(child, var, false, partialResults[index]);
        });
    }

    pool.wait();

    for (auto& bucket : partialResults) {
        patterns.insert(patterns.end(), std::make_move_iterator(bucket.begin()), std::make_move_iterator(bucket.end()));
    }

    return patterns;
}

vector<Clause> getAllPatterns() {
    vector<Clause> patterns;
    vector<Literal> cur;
    if (hasEmptyPattern)
        patterns.push_back(Clause{});
    collectPatterns(root, cur, patterns);
    return patterns;
}

bool containsSameLiteral(const Clause& T, const Clause& C) {
    size_t i = 0, j = 0;
    while (i < T.size() && j < C.size()) {
        if (T[i].var < C[j].var) {
            ++i;
        } else if (T[i].var > C[j].var) {
            ++j;
        } else {
            if (T[i].val == C[j].val)
                return true;
            ++i;
            ++j;
        }
    }
    return false;
}

bool containsOppositeLiteral(const Clause& T, int var, bool val) {
    auto it = lower_bound(T.begin(), T.end(), var,
                          [](const Literal& lit, int value) {
                              return lit.var < value;
                          });
    return it != T.end() && it->var == var && it->val != val;
}

inline int countPatternMatches(const Clause& clause, const Clause& pattern) {
    int matches = 0;
    size_t i = 0, j = 0;
    while (i < clause.size() && j < pattern.size()) {
        if (clause[i].var < pattern[j].var) {
            ++i;
        } else if (clause[i].var > pattern[j].var) {
            ++j;
        } else {
            if (clause[i].val == pattern[j].val)
                ++matches;
            ++i;
            ++j;
        }
    }
    return matches;
}

inline double countClauseAverageMatchFraction(const Clause& clause, const std::vector<Clause>& patterns) {
    if (clause.empty() || patterns.empty())
        return 0.0;

    double totalFraction = 0.0;
    for (auto const& pattern : patterns) {
        totalFraction += static_cast<double>(countPatternMatches(clause, pattern)) / static_cast<double>(clause.size());
    }
    return totalFraction / static_cast<double>(patterns.size());
}

inline size_t selectWeightedConflictMatchClauseIndexParallel(const std::vector<Clause>& clauses,
                                                             const std::vector<Clause>& patterns,
                                                             ThreadPool& pool) {
    if (clauses.empty())
        return 0;

    size_t workers = std::min(pool.workers.size(), clauses.size());
    struct LocalBest { double score; size_t index; };
    std::vector<LocalBest> bestResults(workers, {-1.0, 0});
    std::atomic<size_t> nextIndex{0};
    size_t chunkSize = std::max<size_t>(16, (clauses.size() + workers * 8 - 1) / (workers * 8));

    for (size_t worker = 0; worker < workers; ++worker) {
        pool.enqueue([&clauses, &patterns, &bestResults, &nextIndex, worker, chunkSize]() {
            double localBestScore = -1.0;
            size_t localBestIndex = 0;
            while (true) {
                size_t begin = nextIndex.fetch_add(chunkSize, std::memory_order_relaxed);
                if (begin >= clauses.size())
                    break;
                size_t end = std::min(clauses.size(), begin + chunkSize);
                for (size_t i = begin; i < end; ++i) {
                    const Clause& clause = clauses[i];
                    double avgConflict = 0.0;
                    double avgMatch = 0.0;
                    if (!patterns.empty()) {
                        for (auto const& pattern : patterns) {
                            avgConflict += static_cast<double>(countPatternConflicts(clause, pattern)) / static_cast<double>(clause.size());
                            avgMatch += static_cast<double>(countPatternMatches(clause, pattern)) / static_cast<double>(clause.size());
                        }
                        avgConflict /= static_cast<double>(patterns.size());
                        avgMatch /= static_cast<double>(patterns.size());
                    }
                    double score = avgConflict * 2.0 + avgMatch;
                    if (score > localBestScore) {
                        localBestScore = score;
                        localBestIndex = i;
                    }
                }
            }
            bestResults[worker] = {localBestScore, localBestIndex};
        });
    }

    pool.wait();

    double globalBestScore = -1.0;
    size_t globalBestIndex = 0;
    for (auto const& result : bestResults) {
        if (result.score > globalBestScore) {
            globalBestScore = result.score;
            globalBestIndex = result.index;
        }
    }
    return globalBestIndex;
}

inline bool clauseLess(const Clause& a, const Clause& b);
inline bool clauseEqual(const Clause& a, const Clause& b);

vector<Clause> updatePatterns(const std::vector<Clause>& patterns,
                              const Clause& clause,
                              ThreadPool& pool) {
    if (patterns.empty())
        return {};

    size_t workers = std::min(pool.workers.size(), patterns.size());
    std::vector<std::vector<Clause>> threadResults(workers);
    std::atomic<size_t> nextIndex{0};
    size_t chunkSize = std::max<size_t>(16, (patterns.size() + workers * 8 - 1) / (workers * 8));

    for (size_t worker = 0; worker < workers; ++worker) {
        pool.enqueue([&patterns, &clause, &threadResults, &nextIndex, worker, chunkSize]() {
            auto& localResults = threadResults[worker];
            localResults.reserve(chunkSize * std::min<size_t>(clause.size(), 4));
            while (true) {
                size_t begin = nextIndex.fetch_add(chunkSize, std::memory_order_relaxed);
                if (begin >= patterns.size())
                    break;
                size_t end = std::min(patterns.size(), begin + chunkSize);
                for (size_t i = begin; i < end; ++i) {
                    const Clause& T = patterns[i];
                    if (T.empty()) {
                        for (auto const& lit : clause)
                            localResults.emplace_back(Clause{lit});
                        continue;
                    }

                    if (containsSameLiteral(T, clause)) {
                        localResults.emplace_back(T);
                        continue;
                    }

                    size_t tpos = 0;
                    for (auto const& lit : clause) {
                        while (tpos < T.size() && T[tpos].var < lit.var)
                            ++tpos;
                        if (tpos < T.size() && T[tpos].var == lit.var)
                            continue;
                        localResults.emplace_back(addLiteralSorted(T, lit));
                    }
                }
            }
        });
    }

    pool.wait();

    std::vector<Clause> nextPatterns;
    nextPatterns.reserve(patterns.size() * std::min<size_t>(clause.size(), 2));
    for (auto& chunk : threadResults) {
        nextPatterns.insert(nextPatterns.end(), std::make_move_iterator(chunk.begin()), std::make_move_iterator(chunk.end()));
    }

    if (nextPatterns.empty())
        return nextPatterns;

    std::sort(nextPatterns.begin(), nextPatterns.end(), clauseLess);
    nextPatterns.erase(std::unique(nextPatterns.begin(), nextPatterns.end(), clauseEqual), nextPatterns.end());
    return nextPatterns;
}

inline bool clauseLess(const Clause& a, const Clause& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i].var != b[i].var)
            return a[i].var < b[i].var;
        if (a[i].val != b[i].val)
            return a[i].val < b[i].val;
    }
    return a.size() < b.size();
}

inline bool clauseEqual(const Clause& a, const Clause& b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].var != b[i].var || a[i].val != b[i].val)
            return false;
    }
    return true;
}

struct ClauseUpdateBatch {
    std::vector<Clause> toInsert;
    std::vector<size_t> removeIndices;
    bool removeEmptyPattern = false;
};

ClauseUpdateBatch computeClausePatternUpdates(const Clause& clause,
                                             const std::vector<Clause>& patterns,
                                             ThreadPool& pool) {
    ClauseUpdateBatch batch;
    if (patterns.empty())
        return batch;

    size_t workers = std::min(pool.workers.size(), patterns.size());
    std::vector<std::vector<Clause>> threadNew(workers);
    std::vector<std::vector<size_t>> threadRemoveIndices(workers);
    std::vector<bool> threadRemoveEmpty(workers, false);
    std::atomic<size_t> nextPatternIndex{0};
    size_t chunkSize = std::max<size_t>(16, (patterns.size() + workers * 8 - 1) / (workers * 8));

    for (size_t worker = 0; worker < workers; ++worker) {
        pool.enqueue([&patterns, &clause, &threadNew, &threadRemoveIndices, &threadRemoveEmpty, &nextPatternIndex, chunkSize, worker]() {
            auto& localNew = threadNew[worker];
            auto& localRemove = threadRemoveIndices[worker];
            localNew.reserve(64);
            localRemove.reserve(64);

            while (true) {
                size_t begin = nextPatternIndex.fetch_add(chunkSize, std::memory_order_relaxed);
                if (begin >= patterns.size())
                    break;
                size_t end = std::min(patterns.size(), begin + chunkSize);
                for (size_t i = begin; i < end; ++i) {
                    const Clause& T = patterns[i];
                    if (T.empty()) {
                        threadRemoveEmpty[worker] = true;
                        for (auto const& lit : clause)
                            localNew.emplace_back(Clause{lit});
                        continue;
                    }

                    if (containsSameLiteral(T, clause))
                        continue;

                    localRemove.emplace_back(i);
                    size_t tpos = 0;
                    for (auto const& lit : clause) {
                        while (tpos < T.size() && T[tpos].var < lit.var)
                            ++tpos;
                        if (tpos < T.size() && T[tpos].var == lit.var)
                            continue;

                        localNew.emplace_back(addLiteralSorted(T, lit));
                    }
                }
            }
        });
    }

    pool.wait();

    for (size_t worker = 0; worker < workers; ++worker) {
        if (threadRemoveEmpty[worker])
            batch.removeEmptyPattern = true;
        for (auto& inserted : threadNew[worker])
            batch.toInsert.emplace_back(std::move(inserted));
        for (auto& removedIndex : threadRemoveIndices[worker])
            batch.removeIndices.emplace_back(removedIndex);
    }

    if (!batch.toInsert.empty()) {
        std::sort(batch.toInsert.begin(), batch.toInsert.end(), clauseLess);
        batch.toInsert.erase(std::unique(batch.toInsert.begin(), batch.toInsert.end(), clauseEqual), batch.toInsert.end());
    }

    std::sort(batch.removeIndices.begin(), batch.removeIndices.end());
    batch.removeIndices.erase(std::unique(batch.removeIndices.begin(), batch.removeIndices.end()), batch.removeIndices.end());

    return batch;
}

void updateTreeParallel(Node* root, const Clause& clause, const std::vector<Clause>& patterns, ThreadPool& pool) {
    ClauseUpdateBatch batch = computeClausePatternUpdates(clause, patterns, pool);

    if (batch.removeEmptyPattern)
        removePattern(root, Clause{});

    for (size_t idx : batch.removeIndices)
        removePattern(root, patterns[idx]);

    for (auto& inserted : batch.toInsert)
        insertPattern(root, inserted);
}

inline size_t selectWeightedConflictMatchClauseIndex(const std::vector<Clause>& clauses,
                                                     const std::vector<Clause>& patterns) {
    size_t bestIndex = 0;
    double bestScore = -1.0;

    for (size_t i = 0; i < clauses.size(); ++i) {
        const Clause& clause = clauses[i];
        double avgConflict = 0.0;
        double avgMatch = 0.0;

        if (!patterns.empty()) {
            for (auto const& pattern : patterns) {
                avgConflict += static_cast<double>(countPatternConflicts(clause, pattern)) / static_cast<double>(clause.size());
                avgMatch += static_cast<double>(countPatternMatches(clause, pattern)) / static_cast<double>(clause.size());
            }
            avgConflict /= static_cast<double>(patterns.size());
            avgMatch /= static_cast<double>(patterns.size());
        }

        double score = avgConflict * 2.0 + avgMatch;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    return bestIndex;
}

void updateTree(Node* root, const Clause& clause, const vector<Clause>& patterns) {
    for (auto const& T : patterns) {
        if (T.empty()) {
            removePattern(root, T);
            for (auto const& lit : clause)
                insertPattern(root, Clause{lit});
            continue;
        }

        bool satisfied = containsSameLiteral(T, clause);
        if (satisfied)
            continue;

        bool inserted = false;
        for (auto const& lit : clause) {
            if (containsVar(T, lit.var))
                continue;
            if (containsOppositeLiteral(T, lit.var, lit.val))
                continue;

            Clause newPat = addLiteralSorted(T, lit);
            insertPattern(root, newPat);
            inserted = true;
        }

        removePattern(root, T);
    }
}

void updateTree(Node* root, const Clause& clause) {
    updateTree(root, clause, getAllPatterns());
}

Clause readClause(const string& line) {
    Clause c;
    c.reserve(16);
    const char* p = line.c_str();
    while (*p) {
        while (*p && isspace(static_cast<unsigned char>(*p)))
            ++p;
        if (!*p)
            break;

        int sign = 1;
        if (*p == '-') {
            sign = -1;
            ++p;
        } else if (*p == '+') {
            ++p;
        }

        if (!isdigit(static_cast<unsigned char>(*p))) {
            while (*p && !isspace(static_cast<unsigned char>(*p)))
                ++p;
            continue;
        }

        int x = 0;
        while (isdigit(static_cast<unsigned char>(*p))) {
            x = x * 10 + (*p - '0');
            ++p;
        }
        x *= sign;
        if (x == 0)
            break;
        c.push_back({x > 0 ? x : -x, x > 0});
    }
    sort(c.begin(), c.end(), literalLess);
    return c;
}

void freeTree(Node* node) {
    if (!node || node == anyNode)
        return;
    for (auto& [_, child] : node->childTrue)
        freeTree(child);
    for (auto& [_, child] : node->childFalse)
        freeTree(child);
    delete node;
}

fs::path resolveInputPath(int argc, char* argv[]) {
    vector<fs::path> candidates;
    if (argc > 1)
        candidates.emplace_back(argv[1]);

    // candidates.emplace_back("../test_cases/test_test.cnf");
    // candidates.emplace_back("../test_cases/uf20-01.cnf");
    // candidates.emplace_back("../test_cases/uf20-05.cnf");
    // candidates.emplace_back("../test_cases/uuf50-01.cnf");
    candidates.emplace_back("../test_cases/uf75-098.cnf");
    // candidates.emplace_back("../test_cases/uuf75-097.cnf");
    

    // candidates.emplace_back("../../NewFolder/test_cases/test_test.cnf");
    // candidates.emplace_back("../NewFolder/test_cases/test_test.cnf");

    for (auto const& candidate : candidates) {
        if (fs::exists(candidate))
            return candidate;
    }
    return fs::path();
}

int main(int argc, char* argv[]) {
    fs::path inputPath = resolveInputPath(argc, argv);
    if (inputPath.empty()) {
        cerr << "Cannot locate input CNF file\n";
        return 1;
    }

    fs::path folderPath = inputPath.parent_path();
    string folderName = folderPath.string();
    if (folderName.empty())
        folderName = "./";
    else if (folderName.back() != '/' && folderName.back() != '\\')
        folderName += fs::path::preferred_separator;

    string filename = inputPath.filename().string();

    // Alternative sorting method using filling_sorting.h
    
    // int sortResult = sortFileClauses(folderName, filename, "max") ? 0 : 1;
    // Для альтернативы можно раскомментировать следующую строку и закомментировать вызов sortFileClauses:
    int sortResult = sort_clauses(folderName, filename);
    
    if (sortResult != 0) {
        return 1;
    }

    fs::path sortedPath = folderPath / ("sorted_" + filename);
    ifstream fin(sortedPath);
    if (!fin) {
        cerr << "Cannot open sorted file: " << sortedPath.string() << "\n";
        return 1;
    }

    vector<Clause> remainingClauses;
    string line;
    while (getline(fin, line)) {
        if (line.empty())
            continue;
        if (line[0] == 'c' || line[0] == 'p')
            continue;

        Clause c = readClause(line);
        if (!c.empty())
            remainingClauses.emplace_back(std::move(c));
    }

    int totalClauses = static_cast<int>(remainingClauses.size());
    int clauseIndex = 0;
    const size_t threadCount = std::max<size_t>(1, std::thread::hardware_concurrency());
    ThreadPool pool(threadCount);
    vector<Clause> patterns;
    patterns.emplace_back();
    while (!remainingClauses.empty()) {
        size_t next = selectWeightedConflictMatchClauseIndexParallel(remainingClauses, patterns, pool);

        Clause c = std::move(remainingClauses[next]);
        if (next + 1 < remainingClauses.size())
            remainingClauses[next] = std::move(remainingClauses.back());
        remainingClauses.pop_back();

        ++clauseIndex;
        cout << "Processing clause " << clauseIndex << " / " << totalClauses << ": ";
        for (auto& lit : c)
            cout << (lit.val ? "" : "-") << lit.var << " ";
        cout << "0\n";

        patterns = updatePatterns(patterns, c, pool);
    }

    // patterns already contains the final set of patterns

    auto isSubset = [&](const Clause& small, const Clause& big) {
        if (small.size() >= big.size()) return false;
        size_t i = 0, j = 0;
        while (i < small.size() && j < big.size()) {
            if (small[i].var == big[j].var && small[i].val == big[j].val) {
                i++; j++;
            } else if (small[i].var > big[j].var) {
                j++;
            } else {
                return false;
            }
        }
        return i == small.size();
    };

    vector<Clause> minimal;
    for (size_t i = 0; i < patterns.size(); ++i) {
        bool keep = true;
        for (size_t j = 0; j < patterns.size(); ++j) {
            if (i == j) continue;
            if (isSubset(patterns[j], patterns[i])) {
                keep = false;
                break;
            }
        }
        if (keep && !patterns[i].empty())
            minimal.push_back(patterns[i]);
    }

    sort(minimal.begin(), minimal.end(), [](const Clause& a, const Clause& b) {
        size_t n = min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            if (a[i].var != b[i].var)
                return a[i].var < b[i].var;
            if (a[i].val != b[i].val)
                return b[i].val; // false before true
        }
        return a.size() < b.size();
    });

    cout << "Final patterns:\n";
    for (auto& pat : minimal) {
        for (auto& lit : pat)
            cout << (lit.val ? "" : "-") << lit.var << " ";
        cout << "0\n";
    }

    return 0;
}
