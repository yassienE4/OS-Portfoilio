#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>

using namespace std;

struct Frame {
    int page = -1;
    uint8_t counter = 0;
    bool referenced = false;
};

vector<int> readReferences(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }

    vector<int> refs;
    int page;

    while (file >> page) {
        refs.push_back(page);
    }

    return refs;
}

int simulateAging(int frameCount, const vector<int>& refs) {
    vector<Frame> frames(frameCount);
    int faults = 0;

    for (int page : refs) {

        // Shift aging counters
        for (auto& frame : frames) {
            frame.counter >>= 1;

            if (frame.referenced) {
                frame.counter |= 0x80;  // Set MSB (8-bit)
                frame.referenced = false;
            }
        }

        // Check hit
        bool hit = false;
        for (auto& frame : frames) {
            if (frame.page == page) {
                frame.referenced = true;
                hit = true;
                break;
            }
        }

        if (hit) continue;

        // Page fault
        faults++;

        // Empty frame?
        for (auto& frame : frames) {
            if (frame.page == -1) {
                frame.page = page;
                frame.counter = 0x80;
                frame.referenced = false;
                hit = true;
                break;
            }
        }

        if (hit) continue;

        // Replace smallest counter
        auto victim = min_element(frames.begin(), frames.end(),
            [](const Frame& a, const Frame& b) {
                return a.counter < b.counter;
            });

        victim->page = page;
        victim->counter = 0x80;
        victim->referenced = false;
    }

    return faults;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <reference_file> [max_frames]\n";
        return 1;
    }

    string inputFile = argv[1];

    int maxFrames = 50; // default
    if (argc >= 3) {
        maxFrames = stoi(argv[2]);
    }

    vector<int> references = readReferences(inputFile);

    if (references.empty()) {
        cerr << "Error: Reference file is empty.\n";
        return 1;
    }

    ofstream output("results.csv");
    output << "frames,fault_rate\n";

    for (int f = 1; f <= maxFrames; ++f) {
        int faults = simulateAging(f, references);
        double rate = (faults * 1000.0) / references.size();

        output << f << "," << rate << "\n";

        cout << "Frames: " << f
             << " | Faults: " << faults
             << " | Rate per 1000: " << rate << "\n";
    }

    output.close();

    cout << "\nResults written to results.csv\n";

    return 0;
}