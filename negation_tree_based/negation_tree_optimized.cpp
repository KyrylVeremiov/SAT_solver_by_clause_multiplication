#include <bits/stdc++.h>
using namespace std;

// ============================================================
// GLOBAL DATA
// ============================================================

int n_vars;
vector<vector<vector<int>>> value_true;
vector<vector<vector<int>>> value_false;

int max_clause_index = 0;

// Предвычисленные множества переменных для каждой клаузы
// vars_true[x][i] — битсет переменных в value_true[x][i]
vector<vector<vector<int>>> clause_vars_true;
vector<vector<vector<int>>> clause_vars_false;

// ============================================================
// PRINT ASSIGNMENT
// ============================================================

inline void print_assignment(const vector<int>& vals) {
    for (int v : vals) cout << v << " ";
    cout << 0 << "\n";
}

// ============================================================
// CHECK_VARIABLE — ускоренная версия
// ============================================================

inline bool check_variable(int val, const vector<pair<int,int>>& test_set) {
    if (test_set.empty()) return true;

    for (auto& p : test_set) {
        int v = p.first;
        int idx = p.second;

        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        if (!clause.empty() && clause.back() == val)
            return false;
    }
    return true;
}

// ============================================================
// FIND_NEXT — ускоренная версия
// ============================================================

int find_next(int current_var, const vector<pair<int,int>>& test_set) {

    // Предварительно соберём все переменные из всех клауз
    // (но без копирования — просто ссылки)
    vector<const vector<int>*> clause_vars;
    clause_vars.reserve(test_set.size());

    for (auto& p : test_set) {
        int v = p.first;
        int idx = p.second;

        if (v > 0)
            clause_vars.push_back(&clause_vars_true[v - 1][idx]);
        else
            clause_vars.push_back(&clause_vars_false[-v - 1][idx]);
    }

    // Ищем следующую переменную
    for (int candidate = current_var + 1; candidate <= n_vars; candidate++) {

        // Условие 1: candidate не должен встречаться в test_set
        bool appears = false;
        for (auto* vars : clause_vars) {
            for (int x : *vars) {
                if (x == candidate) {
                    appears = true;
                    break;
                }
            }
            if (appears) break;
        }
        if (appears) return candidate;

        // Условие 2–3: value_true/false не пусты
        if (!value_true[candidate - 1].empty()) return candidate;
        if (!value_false[candidate - 1].empty()) return candidate;
    }

    return n_vars + 1;
}

// ============================================================
// FIND_SOLUTION — оптимизированная версия
// ============================================================

bool find_solution(int current_var,
                   const vector<pair<int,int>>& var_set,
                   vector<int>& prev_values)
{
    // 0. Base conditions
    if (var_set.empty() && current_var > max_clause_index) {
        print_assignment(prev_values);
        return true;
    }

    if (current_var > n_vars)
        return false;

    // 1. Разбиваем var_set
    vector<pair<int,int>> this_var_true;
    vector<pair<int,int>> this_var_false;
    vector<pair<int,int>> this_var_unknown;

    this_var_true.reserve(var_set.size());
    this_var_false.reserve(var_set.size());
    this_var_unknown.reserve(var_set.size());

    for (auto& p : var_set) {
        int v = p.first;
        int idx = p.second;

        const vector<int>& clause = (v > 0)
            ? value_true[v - 1][idx]
            : value_false[-v - 1][idx];

        bool has_pos = false, has_neg = false;

        for (int lit : clause) {
            if (lit == current_var) has_pos = true;
            else if (lit == -current_var) has_neg = true;
            if (has_pos && has_neg) break;
        }

        if (has_pos) this_var_true.push_back(p);
        else if (has_neg) this_var_false.push_back(p);
        else this_var_unknown.push_back(p);
    }

    // 1.1 Добавляем клаузы, начинающиеся с current_var
    if (current_var >= 1 && current_var <= n_vars) {
        int idx = current_var - 1;

        for (int i = 0; i < (int)value_true[idx].size(); i++)
            this_var_true.emplace_back(current_var, i);

        for (int i = 0; i < (int)value_false[idx].size(); i++)
            this_var_false.emplace_back(-current_var, i);
    }

    // 2. Только unknown
    if (!this_var_unknown.empty() &&
        this_var_true.empty() &&
        this_var_false.empty())
    {
        return find_solution(current_var + 1, this_var_unknown, prev_values);
    }

    // 3. Проверяем возможность true/false
    bool can_be_true  = check_variable(current_var,  this_var_true);
    bool can_be_false = check_variable(-current_var, this_var_false);

    if (!can_be_true && !can_be_false)
        return false;

    // 4. Рекурсия
    bool res_true = false, res_false = false;

    if (can_be_true) {
        vector<pair<int,int>> union_true = this_var_true;
        union_true.insert(union_true.end(),
                          this_var_unknown.begin(),
                          this_var_unknown.end());

        int next_var = find_next(current_var, union_true);

        prev_values.push_back(current_var);
        res_true = find_solution(next_var, union_true, prev_values);
        prev_values.pop_back();
    }

    if (can_be_false) {
        vector<pair<int,int>> union_false = this_var_false;
        union_false.insert(union_false.end(),
                           this_var_unknown.begin(),
                           this_var_unknown.end());

        int next_var = find_next(current_var, union_false);

        prev_values.push_back(-current_var);
        res_false = find_solution(next_var, union_false, prev_values);
        prev_values.pop_back();
    }

    return res_true || res_false;
}

// ============================================================
// MAIN
// ============================================================

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);


    // string filename = "../test_cases/test_test.cnf";
    // string filename = "../test_cases/uf20-01.cnf";
    // string filename = "../test_cases/uf20-05.cnf";
    // string filename = "../test_cases/uuf50-01.cnf";
    string filename = "../test_cases/uf75-098.cnf";
    // string filename = "../test_cases/uuf75-097.cnf";

    ifstream fin(filename);
    if (!fin) {
        cerr << "Cannot open file\n";
        return 1;
    }

    string line;
    int n_clauses = 0;

    while (getline(fin, line)) {
        if (line.empty() || line[0] == 'c') continue;
        if (line[0] == 'p') {
            string tmp1, tmp2;
            stringstream ss(line);
            ss >> tmp1 >> tmp2 >> n_vars >> n_clauses;
            break;
        }
    }

    value_true.assign(n_vars, {});
    value_false.assign(n_vars, {});

    clause_vars_true.assign(n_vars, {});
    clause_vars_false.assign(n_vars, {});

    int read_clauses = 0;
    vector<int> clause;
    clause.reserve(16);

    int lit;
    while (read_clauses < n_clauses && (fin >> lit)) {
        if (lit == 0) {
            if (!clause.empty()) {
                for (int& x : clause) x = -x;

                sort(clause.begin(), clause.end(),
                     [](int a, int b) {
                         int aa = abs(a), bb = abs(b);
                         if (aa != bb) return aa < bb;
                         return a < b;
                     });

                int min_lit = clause[0];
                int var = abs(min_lit);

                if (min_lit > 0) {
                    value_true[var - 1].push_back(clause);
                    clause_vars_true[var - 1].push_back({});
                    for (int x : clause) clause_vars_true[var - 1].back().push_back(abs(x));
                } else {
                    value_false[var - 1].push_back(clause);
                    clause_vars_false[var - 1].push_back({});
                    for (int x : clause) clause_vars_false[var - 1].back().push_back(abs(x));
                }

                read_clauses++;
                clause.clear();
            }
        } else {
            clause.push_back(lit);
        }
    }

    max_clause_index = 0;
    for (int i = 0; i < n_vars; i++) {
        if (!value_true[i].empty() || !value_false[i].empty())
            max_clause_index = i + 1;
    }
    if (max_clause_index == 0) max_clause_index = n_vars;

    vector<pair<int,int>> empty_set;
    vector<int> empty_values;

    cout << "Result:\n";
    bool sat = find_solution(1, empty_set, empty_values);
    cout << sat << " - SAT result for the formula\n";

    return 0;
}
