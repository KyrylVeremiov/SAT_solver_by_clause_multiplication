#ifndef CNF_ORDER_B_H
#define CNF_ORDER_B_H

#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <algorithm>

using std::vector;
using std::string;
using std::unordered_map;

/*
    ============================================================
    HYBRID CNF ORDERING (Variant B, conflict-free version)
    ============================================================

    This version guarantees:
        ✔ Clauses with the highest total conflict score appear first.
        ✔ Local conflict density is preserved via MAO inside groups.
        ✔ No name collisions with other headers (all functions renamed).

    Method:
        1. Build conflict graph G[i][j].
        2. Compute global conflict score for each clause.
        3. Sort clauses by descending score.
        4. Inside equal-score groups, apply greedy MAO.
*/


// ------------------------------------------------------------
// Split clause string into literals (unique name: splitClauseB)
// ------------------------------------------------------------
inline vector<string> splitClauseB(const string& clause) {
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
// Build conflict graph G[i][j] (unique name: buildConflictGraphB)
// ------------------------------------------------------------
inline vector<vector<int>> buildConflictGraphB(const vector<string>& C) {
    int n = C.size();
    vector<vector<int>> G(n, vector<int>(n, 0));

    unordered_map<string, vector<int>> pos, neg;

    for (int i = 0; i < n; i++) {
        for (const string& lit : splitClauseB(C[i])) {
            if (lit[0] == '-') neg[lit.substr(1)].push_back(i);
            else pos[lit].push_back(i);
        }
    }

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


// ------------------------------------------------------------
// Compute global conflict score (unique name: computeScoresB)
// ------------------------------------------------------------
inline vector<int> computeScoresB(const vector<vector<int>>& G) {
    int n = G.size();
    vector<int> score(n, 0);

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            score[i] += G[i][j];

    return score;
}


// ------------------------------------------------------------
// Greedy MAO inside equal-score group (unique name: greedyMAOB)
// ------------------------------------------------------------
inline vector<int> greedyMAOB(const vector<int>& group,
                              const vector<vector<int>>& G)
{
    int m = group.size();
    vector<bool> used(m, false);
    vector<int> order;
    order.reserve(m);

    auto gi = [&](int local) { return group[local]; };

    // Start from clause with max internal conflict
    int start = 0;
    int bestScore = -1;
    for (int a = 0; a < m; a++) {
        int s = 0;
        for (int b = 0; b < m; b++)
            s += G[gi(a)][gi(b)];
        if (s > bestScore) { bestScore = s; start = a; }
    }

    order.push_back(gi(start));
    used[start] = true;

    // Greedy expansion
    for (int step = 1; step < m; step++) {
        int last = order.back();
        int best = -1, bestW = -1;

        for (int a = 0; a < m; a++) {
            if (used[a]) continue;
            int w = G[last][gi(a)];
            if (w > bestW) {
                bestW = w;
                best = a;
            }
        }

        used[best] = true;
        order.push_back(gi(best));
    }

    return order;
}


// ------------------------------------------------------------
// Main function (unique name: orderByConflictClusteringB)
// ------------------------------------------------------------
inline vector<vector<string>> orderByConflictClusteringB(const vector<vector<string>>& input) {
    // Convert clauses to strings
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
    auto G = buildConflictGraphB(C);
    auto score = computeScoresB(G);

    // Create index list
    vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;

    // Sort by descending score
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b) {
                  return score[a] > score[b];
              });

    // Group by equal score and apply MAO
    vector<int> finalOrder;
    for (int i = 0; i < n; ) {
        int s = score[idx[i]];
        vector<int> group;

        while (i < n && score[idx[i]] == s) {
            group.push_back(idx[i]);
            i++;
        }

        auto localOrder = greedyMAOB(group, G);
        finalOrder.insert(finalOrder.end(), localOrder.begin(), localOrder.end());
    }

    // Convert back to clause vectors
    vector<vector<string>> out;
    out.reserve(n);
    for (int id : finalOrder)
        out.push_back(input[id]);

    return out;
}

#endif
