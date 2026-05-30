

/*
    ============================================================
    CNF CLAUSE ORDERING BY CONFLICT CLUSTERING (GREEDY MAO)
    ============================================================

    Goal:
        Reorder clauses so that clauses with the highest number
        of mutual conflicts appear as close to each other as possible.

    Method:
        This implementation uses a greedy variant of
        Maximum Adjacency Ordering (MAO), a known heuristic for
        producing conflict-dense linear arrangements in graphs.

    High-level idea:
        1. Build a conflict graph:
               - Nodes = clauses
               - Edge weight(i,j) = number of conflicting literals
        2. Pick the clause with the highest total conflict weight.
        3. Repeatedly append the unused clause that has the strongest
           conflict with the last chosen clause.
        4. Convert indices back to the original clause sets.

    What this algorithm guarantees:
        - Strong local clustering: clauses that directly conflict
          with each other will appear next to each other.
        - Good conflict density around high-conflict clauses.

    What it does NOT guarantee:
        - Global optimality (the MinLA problem is NP-hard).
        - Perfect clustering of multi-branch conflict structures.

    Nevertheless, this greedy MAO approach is fast, simple,
    and produces high-quality local conflict clusters.
*/

#ifndef CNF_ORDER_H
#define CNF_ORDER_H

#include <vector>
#include <string>
#include <set>
#include <unordered_map>
#include <algorithm>

using std::vector;
using std::string;
using std::set;
using std::unordered_map;

// Split clause into literals for greedy ordering
inline vector<string> splitClauseGreedy(const string& clause) {
    vector<string> out;
    string cur;
    for (char c : clause) {
        if (c == ' ') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}



// ------------------------------------------------------------
// Build a weighted conflict graph G[i][j]:
//     G[i][j] = number of conflicting literals between clause i and j
//
// A conflict occurs when one clause contains x and another contains -x.
// Build conflict graph: adjacency matrix of weights
inline vector<vector<int>> buildConflictGraph(const vector<string>& C) {
    int n = C.size();
    vector<vector<int>> G(n, vector<int>(n, 0));

    // Build index: var -> clauses
    unordered_map<string, vector<int>> pos, neg;

    for (int i = 0; i < n; i++) {
        for (const string& lit : splitClauseGreedy(C[i])) {
            if (lit[0] == '-') neg[lit.substr(1)].push_back(i);
            else pos[lit].push_back(i);
        }
    }

    // Fill conflict graph
    for (auto& p : pos) {
        const string& var = p.first;
        if (!neg.count(var)) continue;

        const vector<int>& P = p.second;
        const vector<int>& N = neg[var];

        for (int i : P)
            for (int j : N)
                G[i][j]++, G[j][i]++;
    }

    return G;
}



// Greedy Maximum Adjacency Ordering (MAO)
//
// Produces a sequence of clauses where each next clause is chosen
// to maximize conflict weight with the previously chosen clause.

// Maximum adjacency ordering
inline vector<set<string>> orderByConflictClustering(const vector<set<string>>& input) {
    // Convert vector<set<string>> -> vector<string> where each clause's literals are space-separated
    vector<string> C;
    C.reserve(input.size());
    for (const auto& s : input) {
        string clause;
        bool first = true;
        for (const string& lit : s) {
            if (!first) clause.push_back(' ');
            clause += lit;
            first = false;
        }
        C.push_back(clause);
    }
    int n = C.size();

    auto G = buildConflictGraph(C);

    vector<int> result;
    vector<bool> used(n, false);

    // Start from clause with max total conflicts
    int start = 0;
    int bestScore = -1;
    for (int i = 0; i < n; i++) {
        int s = 0;
        for (int j = 0; j < n; j++) s += G[i][j];
        if (s > bestScore) { bestScore = s; start = i; }
    }

    result.push_back(start);
    used[start] = true;

    // Greedy expansion
    for (int step = 1; step < n; step++) {
        int last = result.back();
        int best = -1, bestW = -1;

        for (int i = 0; i < n; i++) {
            if (used[i]) continue;
            if (G[last][i] > bestW) {
                bestW = G[last][i];
                best = i;
            }
        }

        if (best == -1) break;
        used[best] = true;
        result.push_back(best);
    }

    // Convert indices to original clause sets
    vector<set<string>> out;
    out.reserve(n);
    for (int idx : result)
        out.push_back(input[idx]);

    return out;
}

// Pointer-taking overload for backward compatibility with code that uses &C
inline vector<set<string>> orderByConflictClustering(const vector<set<string>>* input) {
    return orderByConflictClustering(*input);
}

#endif
