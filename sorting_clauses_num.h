//Sorting by number of conflicts for each clause, using fast variable indexing. If C1 conflicts with C2, C3
// then C1 has score 2, C2 and C3 have score 1. Clauses are sorted by descending score. This is 
// a heuristic to cluster.


/*

    ============================================================
    CNF SORTER — OLD CONFLICT SCORING LOGIC (VARIABLE INDEXING)
    ============================================================

    INPUT:  vector<set<string>>
            Each clause is a set of literals, e.g. {"1", "-2", "3"}

    OUTPUT: vector<set<string>>
            Same clauses, but sorted in descending order of
            "conflict score" according to the old logic.

    ------------------------------------------------------------
    OLD CONFLICT LOGIC (VARIABLE-BASED INDEXING)
    ------------------------------------------------------------

    1. Build an index of variables:
           x  → list of clauses containing x
           -x → list of clauses containing -x

    2. For each variable:
           If x appears in P clauses
           and -x appears in N clauses,
           then each clause containing x gets +N,
           and each clause containing -x gets +P.

    This does NOT count the number of conflicting clauses.
    It counts the "weight" of variable conflicts across CNF.

    ------------------------------------------------------------
    PURPOSE
    ------------------------------------------------------------

    Produce an ordering where clauses with higher potential
    conflict weight appear earlier in the list.*/

#ifndef CNF_SORTER_H
#define CNF_SORTER_H

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>

using std::string;
using std::vector;
//using std::set;
using std::unordered_map;

// -------------------------
// Join vector<string> into a single clause string
// -------------------------
inline string joinClause(const vector<string>& clause) {
    string out;
    for (const auto& lit : clause) {
        if (!out.empty()) out += " ";
        out += lit;
    }
    return out;
}

// -------------------------
// Split clause string into literals
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
// Main function: input vector<vector<string>> → output vector<vector<string>>
// -------------------------
inline vector<vector<string>> sortClausesByConflicts(const vector<vector<string>>& input) {
    int n = input.size();

    // Convert vector<vector<string>> → vector<string> (joined)
    vector<string> C;
    C.reserve(n);
    for (const auto& clause : input)
        C.push_back(joinClause(clause));

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

    // Build sorted vector<vector<string>>
    vector<vector<string>> result;
    result.reserve(n);

    for (int idx : order) {
        // preserve clause literal ordering from input
        result.push_back(input[idx]);
    }

    return result;
}

#endif // CNF_SORTER_H
