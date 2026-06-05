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

    vector<string> header;
    vector<vector<int>> clauses;

    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;

        if (line[0] == 'c' || line[0] == 'p') {
            header.push_back(line);
            continue;
        }

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

    // ЛЕКСИКОГРАФИЧЕСКАЯ сортировка клауз
    sort(clauses.begin(), clauses.end(),
         [](const vector<int>& a, const vector<int>& b) {
             size_t n = min(a.size(), b.size());
             for (size_t i = 0; i < n; i++) {
                 int aa = abs(a[i]);
                 int bb = abs(b[i]);
                 if (aa != bb) return aa < bb;
             }
             return a.size() < b.size();
         });

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
