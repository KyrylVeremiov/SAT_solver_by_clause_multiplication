#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include "cnf_sorter.h"
// #include <unordered_set>
using namespace std;

// string filename = "small_sat.cnf";
// string filename = "small.cnf";


string filename = "big_unsat.cnf";
// string filename = "big_sat.cnf";


int counter=0;
int treshold=10000;
long long I=0;


bool SAT=false;
set<string> res;

bool multiply(set<string>* C,set<string> prev, int i, int clausesCount);

int main() {
    // Fixed filename instead of user input

    ifstream fin(filename);
    if (!fin.is_open()) {
        cerr << "Error: cannot open file\n";
        return 1;
    }

    int vars = 0, clauses = 0;
    string line;

    // Search for the "p cnf" line to read number of variables and clauses
    while (getline(fin, line)) {
        if (!line.empty() && line[0] == 'p') {
            string tmp;
            stringstream ss(line);
            ss >> tmp >> tmp >> vars >> clauses;
            break;
        }
    }

    // Create an array of sets (one set per clause)
    set<string>* C = new set<string>[clauses];

    int clauseIndex = 0;

    // Read the rest of the file
    while (getline(fin, line) && clauseIndex < clauses) {
        if (line.empty()) continue;
        if (line[0] == 'c' || line[0] == 'p') continue; // skip comments and header

        stringstream ss(line);
        string token;

        // Read literals as strings until "0"
        while (ss >> token) {
            if (token == "0") break;
            C[clauseIndex].insert(token);
        }

        if (!C[clauseIndex].empty())
            clauseIndex++;
    }

    fin.close();

    // Output for verification
    // cout << "\nLoaded clauses:\n";
    // for (int i = 0; i < clauses; i++) {
    //     cout << "Clause " << i + 1 << ": ";
    //     for (const string& s : C[i]) {
    //         cout << s << " ";
    //     }
    //     cout << "\n";
    // }


    
    // unordered_set<string>* newC = std::unordered_set<int> C(s.begin(), s.end());;
    // unordered_set<string>* newC = sortClausesByConflicts(C);
    

    // cout << multiply(newC, {}, 0, clauses) << "\n";
    cout << multiply(C, {}, 0, clauses) << "\n";

    for (const string& s : res) {
        cout << s << " ";
    }
    cout << "\n";

    delete[] C;
    return 0;
}





bool multiply(set<string>* C,set<string> prev, int i, int clausesCount) {

    if (!SAT) {
        
        if (i >= clausesCount){
            SAT = true;
            res=prev;
            return true;
        }

            
        for (const string& value : C[i]) {
            if(i==0){
                I++;
            }
            string match_value;
            
            if(value[0] == '-'){
            match_value = value.substr(1);
            }
            else {
                match_value = "-" + value;
            }

            
            if (counter > treshold) {
                cout << "Counter: " << I <<" / " << clausesCount << "\n";
                counter = 0;
            }
            counter++;
            
            if (prev.find(match_value) == prev.end()) {
                set<string> new_prev = prev;
                new_prev.insert(value);
                bool next = multiply(C, new_prev, i + 1, clausesCount);
                if (next) {
                    return true;
                }
            }
        }
        return false;
    }
    else {
        return true;
    }
}