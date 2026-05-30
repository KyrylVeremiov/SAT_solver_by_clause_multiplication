#ifndef SORTING_CLAUSES_GREEDY_V2_H
#define SORTING_CLAUSES_GREEDY_V2_H

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>

using std::vector;
using std::string;
using std::unordered_map;

// Sort literals inside a clause by ascending absolute value
inline void sortLiteralsByAbsInClause(vector<string>& clause) {
    std::sort(clause.begin(), clause.end(),
        [](const string& a, const string& b) {
            int va = std::stoi(a);
            int vb = std::stoi(b);
            int aa = std::abs(va);
            int ab = std::abs(vb);
            if (aa != ab) return aa < ab;
            return va < vb; // tie‑break: negative before positive
        }
    );
}

// Build conflict graph G[i][j] for vector<vector<string>>
inline vector<vector<int>> buildConflictGraphB(const vector<vector<string>>& C) {
    int n = (int)C.size();
    vector<vector<int>> G(n, vector<int>(n, 0));

    unordered_map<string, vector<int>> pos, neg;

    for (int i = 0; i < n; i++) {
        for (const string& lit : C[i]) {
            if (!lit.empty() && lit[0] == '-') {
                neg[lit.substr(1)].push_back(i);
            } else {
                pos[lit].push_back(i);
            }
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

// Compute global conflict score for each clause
inline vector<int> computeScoresB(const vector<vector<int>>& G) {
    int n = (int)G.size();
    vector<int> score(n, 0);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            score[i] += G[i][j];
    return score;
}

// Greedy MAO inside a group of equal‑score clauses
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

// Main: Variant B — score sort + MAO inside groups + literal sort by |x|
inline vector<vector<string>> orderByConflictClusteringB(const vector<vector<string>>& input) {
    int n = (int)input.size();
    if (n == 0) return {};

    auto G = buildConflictGraphB(input);
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

    vector<vector<string>> out;
    out.reserve(n);
    for (int id : finalOrder) {
        out.push_back(input[id]);
        sortLiteralsByAbsInClause(out.back());
    }

    return out;
}

#endif
