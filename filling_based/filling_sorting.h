#ifndef FILLING_SORTING_H
#define FILLING_SORTING_H

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

inline void normalize_clause(std::vector<int>& clause) {
    std::sort(clause.begin(), clause.end());
    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());
}

inline void sort_clauses(std::vector<std::vector<int>>& clauses, const std::string& criterion) {
    for (auto& clause : clauses) {
        normalize_clause(clause);
    }

    std::string key = criterion;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (key == "max") {
        std::stable_sort(clauses.begin(), clauses.end(), [](auto const& a, auto const& b) {
            int maxA = a.empty() ? std::numeric_limits<int>::min() : a.back();
            int maxB = b.empty() ? std::numeric_limits<int>::min() : b.back();
            if (maxA != maxB) {
                return maxA > maxB;
            }
            if (a.size() != b.size()) {
                return a.size() > b.size();
            }
            return a > b;
        });
    } else {
        std::stable_sort(clauses.begin(), clauses.end(), [](auto const& a, auto const& b) {
            int minA = a.empty() ? std::numeric_limits<int>::max() : a.front();
            int minB = b.empty() ? std::numeric_limits<int>::max() : b.front();
            if (minA != minB) {
                return minA < minB;
            }
            if (a.size() != b.size()) {
                return a.size() > b.size();
            }
            return a < b;
        });
    }
}

#endif // FILLING_SORTING_H
