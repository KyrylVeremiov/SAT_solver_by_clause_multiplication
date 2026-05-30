#ifndef SORTING_CLAUSES_GREEDY_V2_H
#define SORTING_CLAUSES_GREEDY_V2_H

#include <vector>
#include <algorithm>
#include <cstdlib>

using std::vector;

// ------------------------------------------------------------
// Sort literals inside a clause by ascending absolute value
// ------------------------------------------------------------
inline void sortLiteralsByAbsInClause(vector<int>& clause) {
    std::sort(clause.begin(), clause.end(),
        [](int a, int b) {
            int aa = std::abs(a);
            int ab = std::abs(b);
            if (aa != ab) return aa < ab;
            return a < b;
        }
    );
}

// ------------------------------------------------------------
// Build conflict graph G[i][j] for int literals
// ------------------------------------------------------------
inline vector<vector<int>> buildConflictGraphB(const vector<vector<int>>& C, int maxVar) {
    int n = (int)C.size();
    vector<vector<int>> G(n, vector<int>(n, 0));

    // pos[var] = clauses where var appears positive
    // neg[var] = clauses where var appears negative
    vector<vector<int>> pos(maxVar + 1), neg(maxVar + 1);

    for (int i = 0; i < n; i++) {
        for (int lit : C[i]) {
            int v = std::abs(lit);
            if (v == 0 || v > maxVar) continue;
            if (lit > 0) pos[v].push_back(i);
            else         neg[v].push_back(i);
        }
    }

    for (int v = 1; v <= maxVar; ++v) {
        const auto& P = pos[v];
        const auto& N = neg[v];
        if (P.empty() || N.empty()) continue;

        for (int i : P)
            for (int j : N)
                G[i][j]++, G[j][i]++;
    }

    return G;
}

// ------------------------------------------------------------
inline vector<int> computeScoresB(const vector<vector<int>>& G) {
    int n = (int)G.size();
    vector<int> score(n, 0);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            score[i] += G[i][j];
    return score;
}

// ------------------------------------------------------------
inline vector<int> greedyMAOB(const vector<int>& group,
                              const vector<vector<int>>& G)
{
    int m = (int)group.size();
    vector<bool> used(m, false);
    vector<int> order;
    order.reserve(m);

    auto gi = [&](int local) { return group[local]; };

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
// Main: Variant B on int literals
// ------------------------------------------------------------
inline vector<vector<int>> orderByConflictClusteringB(const vector<vector<int>>& input, int maxVar) {
    int n = (int)input.size();
    if (n == 0) return {};

    auto G = buildConflictGraphB(input, maxVar);
    auto score = computeScoresB(G);

    vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;

    std::sort(idx.begin(), idx.end(),
              [&](int a, int b) {
                  return score[a] > score[b];
              });

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

    vector<vector<int>> out;
    out.reserve(n);
    for (int id : finalOrder) {
        out.push_back(input[id]);
        sortLiteralsByAbsInClause(out.back());
    }

    return out;
}

#endif
