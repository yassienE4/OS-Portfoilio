#include <string>
#include <vector>

using namespace std;

class Command
{
    public:
        string name;
        vector<string> args;
        string inFile;
        string outFile;
        bool append;
        bool background;
        bool isBuildIn;

};