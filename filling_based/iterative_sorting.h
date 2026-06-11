#ifndef ITERATIVE_SORTING_H
#define ITERATIVE_SORTING_H

#include <algorithm>
#include <thread>
#include <vector>

inline int countPatternConflicts(const Clause& clause, const Clause& pattern) {
    int conflicts = 0;
    size_t i = 0, j = 0;
    while (i < clause.size() && j < pattern.size()) {
        if (clause[i].var < pattern[j].var) {
            ++i;
        } else if (clause[i].var > pattern[j].var) {
            ++j;
        } else {
            if (clause[i].val != pattern[j].val)
                ++conflicts;
            ++i;
            ++j;
        }
    }
    return conflicts;
}

inline int countClauseConflicts(const Clause& clause, const std::vector<Clause>& patterns) {
    int total = 0;
    for (auto const& pattern : patterns)
        total += countPatternConflicts(clause, pattern);
    return total;
}

inline double countClauseAverageConflictFraction(const Clause& clause, const std::vector<Clause>& patterns) {
    if (clause.empty() || patterns.empty())
        return 0.0;

    int totalConflicts = 0;
    for (auto const& pattern : patterns)
        totalConflicts += countPatternConflicts(clause, pattern);

    return static_cast<double>(totalConflicts) / (static_cast<double>(patterns.size()) * static_cast<double>(clause.size()));
}

inline size_t selectMostConflictingClauseIndex(const std::vector<Clause>& clauses,
                                               const std::vector<Clause>& patterns) {
    size_t bestIndex = 0;
    int bestScore = -1;
    for (size_t i = 0; i < clauses.size(); ++i) {
        int score = countClauseConflicts(clauses[i], patterns);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    return bestIndex;
}

inline size_t selectHighestAverageConflictClauseIndex(const std::vector<Clause>& clauses,
                                                      const std::vector<Clause>& patterns) {
    size_t bestIndex = 0;
    double bestFraction = -1.0;
    for (size_t i = 0; i < clauses.size(); ++i) {
        double fraction = countClauseAverageConflictFraction(clauses[i], patterns);
        if (fraction > bestFraction) {
            bestFraction = fraction;
            bestIndex = i;
        }
    }
    return bestIndex;
}

inline size_t selectMostConflictingClauseIndexParallel(const std::vector<Clause>& clauses,
                                                       const std::vector<Clause>& patterns,
                                                       size_t threadCount = 8) {
    if (clauses.empty())
        return 0;

    size_t workers = std::min(threadCount, clauses.size());
    std::vector<std::thread> threads;
    threads.reserve(workers);
    std::vector<std::pair<int, size_t>> bestResults(workers, {-1, 0});

    size_t begin = 0;
    size_t baseChunk = clauses.size() / workers;
    size_t remainder = clauses.size() % workers;

    for (size_t worker = 0; worker < workers; ++worker) {
        size_t end = begin + baseChunk + (worker < remainder ? 1 : 0);
        threads.emplace_back([&clauses, &patterns, begin, end, worker, &bestResults]() {
            int localBestScore = -1;
            size_t localBestIndex = begin;
            for (size_t i = begin; i < end; ++i) {
                int score = countClauseConflicts(clauses[i], patterns);
                if (score > localBestScore) {
                    localBestScore = score;
                    localBestIndex = i;
                }
            }
            bestResults[worker] = {localBestScore, localBestIndex};
        });
        begin = end;
    }

    for (auto& thread : threads)
        thread.join();

    int globalBestScore = -1;
    size_t globalBestIndex = 0;
    for (auto const& result : bestResults) {
        if (result.first > globalBestScore) {
            globalBestScore = result.first;
            globalBestIndex = result.second;
        }
    }
    return globalBestIndex;
}

inline size_t selectHighestAverageConflictClauseIndexParallel(const std::vector<Clause>& clauses,
                                                              const std::vector<Clause>& patterns,
                                                              size_t threadCount = 8) {
    if (clauses.empty())
        return 0;

    size_t workers = std::min(threadCount, clauses.size());
    std::vector<std::thread> threads;
    threads.reserve(workers);
    std::vector<std::pair<double, size_t>> bestResults(workers, {-1.0, 0});

    size_t begin = 0;
    size_t baseChunk = clauses.size() / workers;
    size_t remainder = clauses.size() % workers;

    for (size_t worker = 0; worker < workers; ++worker) {
        size_t end = begin + baseChunk + (worker < remainder ? 1 : 0);
        threads.emplace_back([&clauses, &patterns, begin, end, worker, &bestResults]() {
            double localBestFraction = -1.0;
            size_t localBestIndex = begin;
            for (size_t i = begin; i < end; ++i) {
                double fraction = countClauseAverageConflictFraction(clauses[i], patterns);
                if (fraction > localBestFraction) {
                    localBestFraction = fraction;
                    localBestIndex = i;
                }
            }
            bestResults[worker] = {localBestFraction, localBestIndex};
        });
        begin = end;
    }

    for (auto& thread : threads)
        thread.join();

    double globalBestFraction = -1.0;
    size_t globalBestIndex = 0;
    for (auto const& result : bestResults) {
        if (result.first > globalBestFraction) {
            globalBestFraction = result.first;
            globalBestIndex = result.second;
        }
    }
    return globalBestIndex;
}

inline std::vector<size_t> selectTopKHighestAverageConflictClauseIndicesParallel(
        const std::vector<Clause>& clauses,
        const std::vector<Clause>& patterns,
        size_t k,
        size_t threadCount = 8) {
    if (clauses.empty() || k == 0)
        return {};

    std::vector<double> scores(clauses.size());
    size_t workers = std::min(threadCount, clauses.size());
    std::vector<std::thread> threads;
    threads.reserve(workers);

    size_t begin = 0;
    size_t baseChunk = clauses.size() / workers;
    size_t remainder = clauses.size() % workers;

    for (size_t worker = 0; worker < workers; ++worker) {
        size_t end = begin + baseChunk + (worker < remainder ? 1 : 0);
        threads.emplace_back([&clauses, &patterns, begin, end, &scores]() {
            for (size_t i = begin; i < end; ++i) {
                scores[i] = countClauseAverageConflictFraction(clauses[i], patterns);
            }
        });
        begin = end;
    }

    for (auto& thread : threads)
        thread.join();

    std::vector<size_t> indices(clauses.size());
    for (size_t i = 0; i < clauses.size(); ++i)
        indices[i] = i;

    if (k >= indices.size()) {
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return scores[a] > scores[b];
        });
        return indices;
    }

    std::partial_sort(indices.begin(), indices.begin() + k, indices.end(), [&](size_t a, size_t b) {
        return scores[a] > scores[b];
    });
    indices.resize(k);
    return indices;
}

#endif // ITERATIVE_SORTING_H
