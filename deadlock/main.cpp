/*
Write a program to implement the deadlock detection algorithm with multiple resources
of each type. Your program should read from a file the following inputs: the
number of processes, the number of resource types, the number of resources of each
type in existence (vector E), the current allocation matrix C (first row, followed by the
second row, and so on), the request matrix R (first row, followed by the second row,
and so on). The output of your program should indicate whether there is a deadlock in
the system. In case there is, the program should print out the identities of all processes
that are deadlocked.
*/

#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

class SystemState {
public:
    int n; // number of processes
    int m; // number of resource types

    vector<int> E;                 // existing resources
    vector<vector<int>> C;         // allocation matrix
    vector<vector<int>> R;         // request matrix
};

void readInput(const string& filename, SystemState& s) {
    ifstream file(filename);

    file >> s.n >> s.m;

    s.E.resize(s.m);
    s.C.resize(s.n, vector<int>(s.m));
    s.R.resize(s.n, vector<int>(s.m));

    for (int i = 0; i < s.m; i++)
        file >> s.E[i];

    for (int i = 0; i < s.n; i++)
        for (int j = 0; j < s.m; j++)
            file >> s.C[i][j];

    for (int i = 0; i < s.n; i++)
        for (int j = 0; j < s.m; j++)
            file >> s.R[i][j];
}

void detectDeadlock(const SystemState& s) {

    vector<int> A = s.E;

    // Compute available resources
    for (int j = 0; j < s.m; j++)
        for (int i = 0; i < s.n; i++)
            A[j] -= s.C[i][j];

    vector<bool> finished(s.n, false);

    bool progress = true;

    while (progress) {
        progress = false;

        for (int i = 0; i < s.n; i++) {

            if (finished[i]) continue;

            bool canRun = true;

            for (int j = 0; j < s.m; j++) {
                if (s.R[i][j] > A[j]) {
                    canRun = false;
                    break;
                }
            }

            if (canRun) {

                for (int j = 0; j < s.m; j++)
                    A[j] += s.C[i][j];

                finished[i] = true;
                progress = true;
            }
        }
    }

    bool deadlock = false;

    for (int i = 0; i < s.n; i++)
        if (!finished[i])
            deadlock = true;

    if (!deadlock) {
        cout << "No deadlock detected\n";
    } else {
        cout << "Deadlocked processes: ";
        for (int i = 0; i < s.n; i++)
            if (!finished[i])
                cout << "P" << i << " ";
        cout << endl;
    }
}

int main() {

    SystemState s;

    readInput("input.txt", s);

    detectDeadlock(s);

    return 0;
}