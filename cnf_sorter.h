//DEPRECATED. See sorting_clauses.h and sorting_clauses_greedy.h for new versions of this code.

//Sorting by number of conflicts for each clause, using fast variable indexing. If C1 conflicts with C2, C3
// then C1 has score 2, C2 and C3 have score 1. Clauses are sorted by descending score. This is 
// a heuristic to cluster.



#ifndef CNF_SORTER_H
#define CNF_SORTER_H

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

using std::string;
using std::vector;
using std::unordered_set;
using std::unordered_map;

// -------------------------
// Utility: split clause string into literals
// -------------------------
inline vector<string> splitClause(const string& clause) {
    vector<string> out;
    string cur;

    for (char c : clause) {
        if (c == ' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);

    return out;
}

// -------------------------
// Compute conflict scores using fast variable indexing
// -------------------------
inline vector<int> computeConflictScoresFast(const vector<string>& clauses) {
    int n = clauses.size();

    // variable → list of clause indices
    unordered_map<string, vector<int>> pos;
    unordered_map<string, vector<int>> neg;

    // Build index
    for (int i = 0; i < n; i++) {
        vector<string> lits = splitClause(clauses[i]);

        for (const string& lit : lits) {
            if (!lit.empty() && lit[0] == '-')
                neg[lit.substr(1)].push_back(i);
            else
                pos[lit].push_back(i);
        }
    }

    vector<int> score(n, 0);

    // Count conflicts
    for (auto& p : pos) {
        const string& var = p.first;

        if (!neg.count(var))
            continue;

        const vector<int>& P = p.second;
        const vector<int>& N = neg[var];

        for (int i : P)
            score[i] += (int)N.size();

        for (int j : N)
            score[j] += (int)P.size();
    }

    return score;
}

// -------------------------
// Main function:
// input: vector<vector<string>>
// output: vector<vector<string>>
// -------------------------
inline vector<vector<string>> sortClausesByConflicts(const vector<vector<string>>& input) {
    int n = input.size();

    // Convert vector<vector<string>> -> vector<string>
    vector<string> C;
    C.reserve(n);
    for (const auto& cl : input) {
        string s;
        bool first = true;
        for (const auto& lit : cl) {
            if (!first) s.push_back(' ');
            s += lit;
            first = false;
        }
        C.push_back(std::move(s));
    }

    // Compute conflict scores
    vector<int> score = computeConflictScoresFast(C);

    // Order indices
    vector<int> order(n);
    for (int i = 0; i < n; i++)
        order[i] = i;

    // Sort by descending conflict score
    std::sort(order.begin(), order.end(),
              [&](int a, int b) {
                  return score[a] > score[b];
              });

    // Build result preserving original clause vectors
    vector<vector<string>> result;
    result.reserve(n);
    for (int idx : order)
        result.push_back(input[idx]);

    return result;
}

#endif // CNF_SORTER_H
