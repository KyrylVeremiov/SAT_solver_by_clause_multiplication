#include <bits/stdc++.h>
using namespace std;

// Global structures
int n_vars;                                      // number of variables
vector<vector<vector<int>>> value_true;          // value_true[x-1] -> list of clauses
vector<vector<vector<int>>> value_false;         // value_false[x-1] -> list of clauses
int max_clause_index = 0;                        // as defined in the description

// Print assignment like "1 -2 3 0"
void print_assignment(const vector<int>& vals) {
    for (int v : vals) {
        cout << v << " ";
    }
    cout << 0 << "\n";
}

// test_set: set of (signed_var_index, clause_index)
// val: literal to check as last in clause (e.g. 7 or -7)
bool check_variable(int val, const set<pair<int,int>>& test_set) {
    for (auto [v, idx] : test_set) {
        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        if (!clause.empty() && clause.back() == val) {
            return false;
        }
    }
    return true;
}

// Recursive search
bool find_solution(int current_var,
                   const set<pair<int,int>>& var_set,
                   const vector<int>& prev_values) {
    // 0. Base conditions

    // If no clauses to track and we passed all possible starting indices
    if (var_set.empty() && current_var > max_clause_index) {
        print_assignment(prev_values);
        return true;
    }

    // If variable index is out of range
    if (abs(current_var) > n_vars) {
        return false;
    }

    // 1. Split var_set into three groups
    set<pair<int,int>> this_var_true;
    set<pair<int,int>> this_var_false;
    set<pair<int,int>> this_var_unknown;

    for (auto [v, idx] : var_set) {
        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        bool has_pos = false;
        bool has_neg = false;
        for (int lit : clause) {
            if (lit == current_var)  has_pos = true;
            if (lit == -current_var) has_neg = true;
        }

        if (has_pos) {
            this_var_true.insert({v, idx});
        } else if (has_neg) {
            this_var_false.insert({v, idx});
        } else {
            this_var_unknown.insert({v, idx});
        }
    }

    // 1.1. Add all clauses that start with current_var (after inversion/sorting)
    if (current_var >= 1 && current_var <= n_vars) {
        int idx = current_var - 1;

        for (int i = 0; i < (int)value_true[idx].size(); ++i) {
            this_var_true.insert({current_var, i});
        }
        for (int i = 0; i < (int)value_false[idx].size(); ++i) {
            this_var_false.insert({-current_var, i});
        }
    }

    // 2. If only unknown clauses exist for this variable
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

    // 4. Recurse with unions: (true ∪ unknown) and (false ∪ unknown)
    bool value_true_res = false;
    bool value_false_res = false;

    if (can_be_true) {
        set<pair<int,int>> next_set_true = this_var_true;
        next_set_true.insert(this_var_unknown.begin(), this_var_unknown.end());

        vector<int> next_values = prev_values;
        next_values.push_back(current_var);

        value_true_res = find_solution(current_var + 1, next_set_true, next_values);
    }

    if (can_be_false) {
        set<pair<int,int>> next_set_false = this_var_false;
        next_set_false.insert(this_var_unknown.begin(), this_var_unknown.end());

        vector<int> next_values = prev_values;
        next_values.push_back(-current_var);

        value_false_res = find_solution(current_var + 1, next_set_false, next_values);
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
    ifstream fin(filename);
    if (!fin) {
        cerr << "Cannot open file: " << filename << "\n";
        return 1;
    }

    string line;
    int n_clauses = 0;

    // Read header "p cnf n m"
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

    // Read clauses
    int read_clauses = 0;
    while (read_clauses < n_clauses && fin) {
        vector<int> clause;
        int lit;
        bool got_any = false;

        while (fin >> lit) {
            if (lit == 0) {
                if (got_any) {
                    // Invert literals
                    for (int &x : clause) x = -x;

                    // Sort by absolute index (then by sign)
                    sort(clause.begin(), clause.end(), [](int a, int b) {
                        if (abs(a) != abs(b)) return abs(a) < abs(b);
                        return a < b;
                    });

                    // Determine smallest index and sign
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
                    got_any = false;
                }
                break;
            } else {
                clause.push_back(lit);
                got_any = true;
            }
        }
    }

    // Compute max_clause_index: maximum index that can be first in a clause
    max_clause_index = 0;
    for (int i = 0; i < n_vars; ++i) {
        if (!value_true[i].empty() || !value_false[i].empty()) {
            max_clause_index = i + 1;
        }
    }
    if (max_clause_index == 0) {
        max_clause_index = n_vars;
    }

    set<pair<int,int>> empty_set;
    vector<int> empty_values;

    cout << "Result:\n";
    bool sat = find_solution(1, empty_set, empty_values);
    cout << sat << " - SAT result for the formula\n";

    return 0;
}
