#include <bits/stdc++.h>
using namespace std;

int sort_clauses(const string& folder_name, const string& filename) {
    string input_path = folder_name + filename;
    string output_path = folder_name + "sorted_" + filename;

    ifstream fin(input_path);
    if (!fin) {
        cerr << "Cannot open input file: " << input_path << endl;
        return 1;
    }

    vector<string> header;          // строки начинающиеся с 'c' или 'p'
    vector<vector<int>> clauses;    // список клауз

    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;

        if (line[0] == 'c' || line[0] == 'p') {
            header.push_back(line);
            continue;
        }

        // читаем клаузу
        stringstream ss(line);
        vector<int> clause;
        int x;
        while (ss >> x) {
            if (x == 0) break;
            clause.push_back(x);
        }
        if (!clause.empty())
            clauses.push_back(clause);
    }
    fin.close();

    // сортировка литералов внутри клаузы
    for (auto &cl : clauses) {
        sort(cl.begin(), cl.end(), [](int a, int b){
            return abs(a) < abs(b);
        });
    }

    // сортировка клауз
    sort(clauses.begin(), clauses.end(),
         [](const vector<int>& a, const vector<int>& b) {
             int minA = INT_MAX, minB = INT_MAX;
             for (int x : a) minA = min(minA, abs(x));
             for (int x : b) minB = min(minB, abs(x));

             if (minA != minB) return minA < minB;
             return a.size() < b.size();
         });

    // запись результата
    ofstream fout(output_path);
    if (!fout) {
        cerr << "Cannot open output file: " << output_path << endl;
        return 1;
    }

    for (auto &h : header) fout << h << "\n";

    for (auto &cl : clauses) {
        for (int x : cl) fout << x << " ";
        fout << "0\n";
    }

    fout.close();

    cout << "Saved sorted CNF to: " << output_path << endl;
    return 0;
}
