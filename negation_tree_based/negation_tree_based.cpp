#include <bits/stdc++.h>
using namespace std;

// Global structures
int n_vars;                                      // number of variables
vector<vector<vector<int>>> value_true;          // value_true[x-1] -> list of clauses
vector<vector<vector<int>>> value_false;         // value_false[x-1] -> list of clauses
int max_clause_index = 0;                        // as defined in the description

// Print assignment like "1 -2 3 0"
inline void print_assignment(const vector<int>& vals) {
    for (int v : vals) {
        cout << v << " ";
    }
    cout << 0 << "\n";
}

// test_set: set of (signed_var_index, clause_index)
// val: literal to check as last in clause (e.g. 7 or -7)
inline bool check_variable(int val, const set<pair<int,int>>& test_set) {
    if (test_set.empty()) return true;

    for (const auto& p : test_set) {
        int v   = p.first;
        int idx = p.second;

        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        if (!clause.empty() && clause.back() == val) {
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------
// NEW CORRECT VERSION OF find_next
// ------------------------------------------------------------
int find_next(int current_var, const set<pair<int,int>>& test_set) {

    // Precompute: for each clause, store all variables in it
    vector<vector<int>> clause_vars;
    clause_vars.reserve(test_set.size());

    for (const auto& p : test_set) {
        int v   = p.first;
        int idx = p.second;

        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        vector<int> vars;
        vars.reserve(clause.size());
        for (int lit : clause) vars.push_back(abs(lit));

        clause_vars.push_back(move(vars));
    }

    // Scan candidate_var from current_var+1 to n_vars
    for (int candidate = current_var + 1; candidate <= n_vars; candidate++) {

        // Condition 1: candidate must NOT appear in ANY clause of test_set
        bool appears = false;
        for (const auto& vars : clause_vars) {
            for (int v : vars) {
                if (v == candidate) {
                    appears = true;
                    break;
                }
            }
            if (appears) break;
        }
        if (appears) return candidate;

        // Condition 2: value_true[candidate-1] must be empty
        if (!value_true[candidate - 1].empty()) return candidate;

        // Condition 3: value_false[candidate-1] must be empty
        if (!value_false[candidate - 1].empty()) return candidate;
    }

    return n_vars + 1;
}
// ------------------------------------------------------------


// Recursive search
bool find_solution(int current_var,
                   const set<pair<int,int>>& var_set,
                   const vector<int>& prev_values) {

    // 0. Base conditions
    if (var_set.empty() && current_var > max_clause_index) {
        print_assignment(prev_values);
        return true;
    }

    if (current_var > n_vars) {
        return false;
    }

    // 1. Split var_set into three groups
    set<pair<int,int>> this_var_true;
    set<pair<int,int>> this_var_false;
    set<pair<int,int>> this_var_unknown;

    for (const auto& p : var_set) {
        int v   = p.first;
        int idx = p.second;

        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        bool has_pos = false;
        bool has_neg = false;

        for (int lit : clause) {
            if (lit == current_var)  has_pos = true;
            else if (lit == -current_var) has_neg = true;
            if (has_pos && has_neg) break;
        }

        if (has_pos) {
            this_var_true.insert(p);
        } else if (has_neg) {
            this_var_false.insert(p);
        } else {
            this_var_unknown.insert(p);
        }
    }

    // 1.1. Add all clauses that start with current_var
    if (current_var >= 1 && current_var <= n_vars) {
        int idx = current_var - 1;

        const auto& vt = value_true[idx];
        const auto& vf = value_false[idx];

        for (int i = 0; i < (int)vt.size(); ++i) {
            this_var_true.insert({current_var, i});
        }
        for (int i = 0; i < (int)vf.size(); ++i) {
            this_var_false.insert({-current_var, i});
        }
    }

    // 2. If only unknown clauses exist
    if (!this_var_unknown.empty() &&
        this_var_true.empty() &&
        this_var_false.empty()) {
        return find_solution(current_var + 1, this_var_unknown, prev_values);
    }

    // 3. Check if current variable can be true/false
    bool can_be_true  = check_variable(current_var,  this_var_true);
    bool can_be_false = check_variable(-current_var, this_var_false);

    if (!can_be_true && !can_be_false) {
        return false;
    }

    // 4. Recurse with unions and find_next
    bool value_true_res  = false;
    bool value_false_res = false;

    if (can_be_true) {
        set<pair<int,int>> union_true = this_var_true;
        union_true.insert(this_var_unknown.begin(), this_var_unknown.end());

        int next_var = find_next(current_var, union_true);
        vector<int> next_values = prev_values;
        next_values.push_back(current_var);

        value_true_res = find_solution(next_var, union_true, next_values);
    }

    if (can_be_false) {
        set<pair<int,int>> union_false = this_var_false;
        union_false.insert(this_var_unknown.begin(), this_var_unknown.end());

        int next_var = find_next(current_var, union_false);
        vector<int> next_values = prev_values;
        next_values.push_back(-current_var);

        value_false_res = find_solution(next_var, union_false, next_values);
    }

    return value_true_res || value_false_res;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string filename = "../test_cases/test_test.cnf";
    // string filename = "../test_cases/uf20-01.cnf";
    // string filename = "../test_cases/uf20-05.cnf";
    // string filename = "../test_cases/uuf50-01.cnf";
    // string filename = "../test_cases/uf75-098.cnf";
    // string filename = "../test_cases/uuf75-097.cnf";
    ifstream fin(filename);
    if (!fin) {
        cerr << "Cannot open file: " << filename << "\n";
        return 1;
    }

    string line;
    int n_clauses = 0;

    while (getline(fin, line)) {
        if (line.empty()) continue;
        if (line[0] == 'c') continue;
        if (line[0] == 'p') {
            string tmp1, tmp2;
            stringstream ss(line);
            ss >> tmp1 >> tmp2 >> n_vars >> n_clauses;
            break;
        }
    }

    value_true.assign(n_vars, {});
    value_false.assign(n_vars, {});

    int read_clauses = 0;
    vector<int> clause;
    clause.reserve(16);

    int lit;
    while (read_clauses < n_clauses && (fin >> lit)) {
        if (lit == 0) {
            if (!clause.empty()) {
                for (int &x : clause) x = -x;

                sort(clause.begin(), clause.end(), [](int a, int b) {
                    int aa = abs(a), bb = abs(b);
                    if (aa != bb) return aa < bb;
                    return a < b;
                });

                int min_lit = clause[0];
                int var = abs(min_lit);

                if (var >= 1 && var <= n_vars) {
                    if (min_lit > 0) {
                        value_true[var - 1].push_back(clause);
                    } else {
                        value_false[var - 1].push_back(clause);
                    }
                }

                ++read_clauses;
                clause.clear();
            }
        } else {
            clause.push_back(lit);
        }
    }

    max_clause_index = 0;
    for (int i = 0; i < n_vars; ++i) {
        if (!value_true[i].empty() || !value_false[i].empty()) {
            max_clause_index = i + 1;
        }
    }
    if (max_clause_index == 0) max_clause_index = n_vars;

    set<pair<int,int>> empty_set;
    vector<int> empty_values;

    cout << "Result:\n";
    bool sat = find_solution(1, empty_set, empty_values);
    cout << sat << " - SAT result for the formula\n";

    return 0;
}
