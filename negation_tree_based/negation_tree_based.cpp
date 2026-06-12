#include <bits/stdc++.h>
using namespace std;

int n_vars, n_clauses;
vector<vector<int>> clauses;              // все клаузы (после инверсии и сортировки)
vector<vector<int>> value_true;          // для каждой переменной: индексы клауз, где минимальный литерал положительный
vector<vector<int>> value_false;         // для каждой переменной: индексы клауз, где минимальный литерал отрицательный

// Получить ссылку на клаузу по паре (var_with_sign, idx_in_vector)
const vector<int>& get_clause(const pair<int,int>& ref) {
    int v = ref.first;
    int pos = ref.second;
    int var = abs(v);
    int clause_index;
    if (v > 0) {
        clause_index = value_true[var][pos];
    } else {
        clause_index = value_false[var][pos];
    }
    return clauses[clause_index];
}

// Проверка: содержит ли клауза литерал val и он последний
bool contains_literal_last(const vector<int>& clause, int val) {
    for (int i = 0; i < (int)clause.size(); ++i) {
        if (clause[i] == val) {
            return i == (int)clause.size() - 1;
        }
    }
    return false;
}

// Функция check_variable(val, test_set)
bool check_variable(int val, const vector<pair<int,int>>& test_set) {
    for (const auto& ref : test_set) {
        const vector<int>& cl = get_clause(ref);
        if (contains_literal_last(cl, val)) {
            return false;
        }
    }
    return true;
}

// Рекурсивная функция find_solution
bool find_solution(int current_var,
                   const vector<pair<int,int>>& var_set,
                   const vector<int>& prev_values) {
    if (current_var > n_vars) {
        return false; // пункт 0
    }

    vector<pair<int,int>> this_var_true;
    vector<pair<int,int>> this_var_false;
    vector<pair<int,int>> this_var_unknown;

    // 1. Разбор var_set
    for (const auto& ref : var_set) {
        const vector<int>& cl = get_clause(ref);
        bool has_true = false, has_false = false;
        for (int lit : cl) {
            if (lit == current_var) has_true = true;
            if (lit == -current_var) has_false = true;
        }
        if (has_true) {
            this_var_true.push_back(ref);
        } else if (has_false) {
            this_var_false.push_back(ref);
        } else {
            this_var_unknown.push_back(ref);
        }
    }

    // 1.1. Добавить все клаузы из value_true[current_var] и value_false[current_var]
    for (int i = 0; i < (int)value_true[current_var].size(); ++i) {
        this_var_true.emplace_back(current_var, i);
    }
    for (int i = 0; i < (int)value_false[current_var].size(); ++i) {
        this_var_false.emplace_back(-current_var, i);
    }

    // 2. Проверка на пустое unknown
    bool printed = false;
    if (this_var_unknown.empty()) {
        if (this_var_true.empty()) {
            // вывод prev_values + current_var
            for (int v : prev_values) cout << v << " ";
            cout << current_var << " 0\n";
            printed = true;
        }
        if (this_var_false.empty()) {
            for (int v : prev_values) cout << v << " ";
            cout << -current_var << " 0\n";
            printed = true;
        }
        if (printed) return true;
    }

    // Если unknown непустое, а true и false пустые
    if (!this_var_unknown.empty() && this_var_true.empty() && this_var_false.empty()) {
        return find_solution(current_var + 1, this_var_unknown, prev_values);
    }

    // 3. Работа с check_variable
    bool can_be_true = check_variable(current_var, this_var_true);
    bool can_be_false = check_variable(-current_var, this_var_false);

    if (!can_be_true && !can_be_false) {
        return false;
    }

    // 4. Рекурсивные вызовы
    bool value_true_res = false, value_false_res = false;

    if (can_be_true) {
        vector<int> new_prev = prev_values;
        new_prev.push_back(current_var);
        value_true_res = find_solution(current_var + 1, this_var_true, new_prev);
    }

    if (can_be_false) {
        vector<int> new_prev = prev_values;
        new_prev.push_back(-current_var);
        value_false_res = find_solution(current_var + 1, this_var_false, new_prev);
    }

    return value_true_res || value_false_res;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Чтение DIMACS из файла ../test_cases/test_test.cnf
    ifstream fin("../test_cases/test_test.cnf");
    if (!fin) {
        cerr << "Cannot open file ../test_cases/test_test.cnf\n";
        return 1;
    }

    string line;
    n_vars = 0;
    n_clauses = 0;

    // Сначала ищем строку p cnf
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

    clauses.clear();
    value_true.assign(n_vars + 1, {});
    value_false.assign(n_vars + 1, {});

    // Чтение клауз
    int lit;
    vector<int> clause;
    while (fin >> lit) {
        if (lit == 0) {
            if (!clause.empty()) {
                // Инверсия литералов
                for (int& x : clause) x = -x;

                // Сортировка по возрастанию индекса (по |x|)
                sort(clause.begin(), clause.end(),
                     [](int a, int b) {
                         if (abs(a) != abs(b)) return abs(a) < abs(b);
                         return a < b;
                     });

                int clause_index = (int)clauses.size();
                clauses.push_back(clause);

                // Определяем минимальный индекс и знак
                int minAbs = abs(clause[0]);
                int sign = (clause[0] > 0) ? 1 : -1;
                if (sign > 0) {
                    value_true[minAbs].push_back(clause_index);
                } else {
                    value_false[minAbs].push_back(clause_index);
                }
            }
            clause.clear();
        } else {
            clause.push_back(lit);
        }
    }

    cout << "Result:\n";
    bool sat = find_solution(1, {}, {});
    cout << sat << " - SAT result for the formula\n";

    return 0;
}
