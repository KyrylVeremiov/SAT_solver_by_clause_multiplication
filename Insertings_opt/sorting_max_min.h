#ifndef NEW_SORT_CLAUSES_MIN_INDEX_H
#define NEW_SORT_CLAUSES_MIN_INDEX_H

#include <bits/stdc++.h>
using namespace std;

/*
    Sort clauses by descending min-index.
    min_index(clause) = minimum absolute variable index in the clause.
    Clauses with larger min_index should appear earlier in the file.

    Example:
        Clause A: [-1, 3, 2, 5]   -> min_index = 1
        Clause B: [5, -6, 3, 4]   -> min_index = 3
        Clause C: [5, -6, 5, 4, -8] -> min_index = 5

    Sorted order:
        C
        B
        A

    Inside each clause, literals are sorted by abs(x) ascending.
*/

int sort_clauses_max_min_index(const string& folder_name, const string& filename) {
    string input_path  = folder_name + filename;
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

    // Step 1: sort literals inside each clause by abs(x)
    for (auto& cl : clauses) {
        sort(cl.begin(), cl.end(),
             [](int a, int b) { return abs(a) < abs(b); });
    }

    // Step 2: sort clauses by descending min-index
    sort(clauses.begin(), clauses.end(),
         [](const vector<int>& a, const vector<int>& b) {
             int mina = INT_MAX, minb = INT_MAX;

             for (int x : a) mina = min(mina, abs(x));
             for (int x : b) minb = min(minb, abs(x));

             // Descending order: larger min-index first
             if (mina != minb) return mina > minb;

             // Tie-breaker: lexicographic by abs(x)
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

    for (auto& h : header) fout << h << "\n";

    for (auto& cl : clauses) {
        for (int x : cl) fout << x << " ";
        fout << "0\n";
    }

    fout.close();

    cout << "Saved sorted CNF to: " << output_path << endl;
    return 0;
}

#endif
