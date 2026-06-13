#ifndef SORT_CLAUSES_MIN_RANGE_H
#define SORT_CLAUSES_MIN_RANGE_H

#include <bits/stdc++.h>
using namespace std;

/*
    Sort clauses by minimal (max_index - min_index).

    For each clause:
        min_index = minimum absolute literal
        max_index = maximum absolute literal
        diff = max_index - min_index

    Clauses are sorted by ascending diff:
        smaller diff → earlier in the file

    Example:
        Clause A: [-1, 3, 2, 5]
            min = 1, max = 5 → diff = 4

        Clause B: [5, -6, 3, 4]
            min = 3, max = 6 → diff = 3

        Clause C: [5, -6, 5, 4, -8]
            min = 4, max = 8 → diff = 4

    Sorted order:
        B (diff = 3)
        A (diff = 4)
        C (diff = 4)

    Inside each clause, literals are sorted by abs(x) ascending.
*/

int sort_clauses_min_range(const string& folder_name, const string& filename) {
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

    // Step 2: sort clauses by ascending (max_index - min_index)
    sort(clauses.begin(), clauses.end(),
         [](const vector<int>& a, const vector<int>& b) {
             int mina = INT_MAX, minb = INT_MAX;
             int maxa = 0,       maxb = 0;

             for (int x : a) {
                 int ax = abs(x);
                 mina = min(mina, ax);
                 maxa = max(maxa, ax);
             }
             for (int x : b) {
                 int bx = abs(x);
                 minb = min(minb, bx);
                 maxb = max(maxb, bx);
             }

             int diffa = maxa - mina;
             int diffb = maxb - minb;

             if (diffa != diffb) return diffa < diffb;

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
