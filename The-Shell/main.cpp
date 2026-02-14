#include <iostream>
#include <unistd.h>   // getcwd
#include <limits.h>   // PATH_MAX
#include <cctype>     // std::isspace
#include <string>
#include <vector>
#include <cstring>   // strncpy
#include "Command.h"
#include <sstream>
#include <unordered_set>
#include <fcntl.h>  
#include <sys/wait.h>
#include <sys/types.h>
#include <fstream>
#include <dirent.h>




using namespace std;

static bool isWhitespaceOnly(const string& s) {
    for (unsigned char c : s) {
        if (!isspace(c)) return false;
    }
    return true;
}

static Command parseLine(string& line)
{
    Command cmd;

    //default values
    cmd.append = false;
    cmd.background = false;
    cmd.isBuildIn = false;

    istringstream iss(line);
    string token;

    while (iss >> token) {
        if (token == "<") {
            iss >> cmd.inFile;
        }
        else if (token == ">") {
            cmd.append = false;
            iss >> cmd.outFile;
        }
        else if (token == ">>") {
            cmd.append = true;
            iss >> cmd.outFile;
        }
        else if (token == "&") {
            cmd.background = true;
        }
        else {
            // normal command or argument
            if (cmd.name.empty()) {
                cmd.name = token;
            }
            else
                cmd.args.push_back(token);
        }
    }

    static const unordered_set<string> builtins = {
         "cd", "echo", "quit", "help", "environ", "set", "dir", "pause"
    };

    if (!cmd.name.empty() && builtins.count(cmd.name)) {
        cmd.isBuildIn = true;
    }

    return cmd;

}

static int redirectStdout(const Command& cmd)
{
    if (cmd.outFile.empty()) return -1; // no redirection

    int saved = dup(STDOUT_FILENO);
    if (saved < 0) { perror("dup"); return -1; }

    int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
    int fd = open(cmd.outFile.c_str(), flags, 0644);
    if (fd < 0) { perror("open >"); close(saved); return -1; }

    if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 >"); close(fd); close(saved); return -1; }
    close(fd);

    return saved; // caller must restore
}

static void restoreStdout(int savedFd)
{
    if (savedFd < 0) return;
    if (dup2(savedFd, STDOUT_FILENO) < 0) perror("restore stdout");
    close(savedFd);
}


static void executeBuiltIn(const Command &cmd)
{
    int savedStdout = -1;
    bool redirectable = 
        (cmd.name == "dir" || cmd.name == "environ" || cmd.name == "echo" || cmd.name == "help");

    if (redirectable && !cmd.outFile.empty())
        savedStdout = redirectStdout(cmd);

    if(cmd.name == "quit")
    {
        exit(0);
    }
    if(cmd.name == "pause")
    {
        cout << "Press Enter to continue..." << flush;

        // Wait for Enter
        string dummy;
        getline(cin, dummy);
    }
    if(cmd.name == "cd")
    {
        if(cmd.args.empty())
        {
            char cwd[PATH_MAX]; // set array with max path size
            if (!getcwd(cwd, sizeof(cwd))) { // puts cd in cwd, with max size of path size
                strncpy(cwd, "?", sizeof(cwd)); 
                cwd[sizeof(cwd) - 1] = '\0'; // makes sure theres a null at the end instead of being empty
            }
            cout << cwd << "\n"; // flush forces output
        }
        else
        {
            const char* path = cmd.args[0].c_str();
            if (chdir(path) != 0)
            {
                perror("cd");
            }
            else
            {   
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != nullptr)
                {
                    setenv("PWD", cwd, 1); // overwrite existing
                }
            }
        }
    }
    if(cmd.name == "dir")
    {
        string dir;
        if(cmd.args.empty())
        {
            dir = "." ;//cd
        }
        else
            dir = cmd.args[0];
        
        DIR* d = opendir(dir.c_str());

        dirent* entry;
        while ((entry = readdir(d)) != nullptr)
        {
            string name = entry->d_name;
            if (name == "." || name == "..")
                continue;

            cout << name << "\n";
        }
        closedir(d);
    }
    if(cmd.name == "environ")
    {
        for (char **env = environ; *env != nullptr; ++env)
        {
            cout << *env << "\n";
        }
    }
    if(cmd.name == "set")
    {
        if(cmd.args.size() != 2)
        {
            cerr << "set: usage: set VARIABLE VALUE\n";
            return;
        }
        const string& variable = cmd.args[0];
        const string& value    = cmd.args[1];

        if (setenv(variable.c_str(), value.c_str(), 1) != 0)
        {
            perror("set");
        }
    }
    if(cmd.name == "echo")
    {
        for(auto n : cmd.args)
        {
            cout << n << " ";
        }
        cout << "\n";
    }
    if (cmd.name == "help")
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            execlp("more", "more", "help.txt", (char*)nullptr);
            perror("execlp");
            _exit(1);
        }
        else
        {
            waitpid(pid, nullptr, 0);
        }
    }

    restoreStdout(savedStdout);
}

static void executeCommand(const Command &cmd)
{
    if(cmd.isBuildIn)
    {
        executeBuiltIn(cmd);
    }
    else
    {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }
        
        if (pid == 0) // CHILD
        {
            if (!cmd.inFile.empty()) 
            {
                int fileDescriptor = open(cmd.inFile.c_str(), O_RDONLY); // opening file in readonly
                // making sure it was opened correctly, less then 0 = not valid file descriptor
                if (fileDescriptor < 0) {
                    perror("open <"); 
                    _exit(1); }
                if (dup2(fileDescriptor, STDIN_FILENO) < 0) {
                    perror("dup2 <");
                    _exit(1); }
                close(fileDescriptor);
            }

            if (!cmd.outFile.empty()) 
            {
                int flags = O_WRONLY | O_CREAT;
                flags |= cmd.append ? O_APPEND : O_TRUNC;

                int fileDescriptor = open(cmd.outFile.c_str(), flags, 0644); //if file exists, open in write only, otherwise create file
                // error checking
                if (fileDescriptor < 0) {
                    perror("open >"); 
                    _exit(1); }
                if (dup2(fileDescriptor, STDOUT_FILENO) < 0) {
                    perror("dup2 >"); 
                    _exit(1); }
                close(fileDescriptor);
            }
            
            // exec takes a char array as params so we must construct one,
            vector<char*> argv;
            argv.reserve(cmd.args.size() + 1);
            argv.push_back(const_cast<char*>(cmd.name.c_str()));
            for (const string& s : cmd.args) 
                argv.push_back(const_cast<char*>(s.c_str()));
            argv.push_back(nullptr);
            
            execvp(argv[0], argv.data());

            perror("execvp");
            _exit(127);
        }
        if(!cmd.background) // parent
        {
            int status = 0;
            if (waitpid(pid, &status, 0) < 0) 
                perror("waitpid");
        }
    }
}

static void runShell(istream& in, bool interactive)
{
    bool quit = false;
    while(!quit)
    {
        char cwd[PATH_MAX]; // set array with max path size
        if (!getcwd(cwd, sizeof(cwd))) { // puts cd in cwd, with max size of path size
            strncpy(cwd, "?", sizeof(cwd)); 
            cwd[sizeof(cwd) - 1] = '\0'; // makes sure theres a null at the end instead of being empty
        }


        if(interactive)
        {
            cout << cwd << " # " << flush; // flush forces output
        }

        string line;
        if (!getline(in, line)) {
            // EOF (Ctrl+D) or input error => quit cleanly
            if(interactive)
            {
                cout << "\n";
            }
            break;
        }
        
        // now we have a "command" in line
        if(line.empty() || isWhitespaceOnly(line))
        {
            continue;
        }
        // now we know we actually have an input
        Command cmd = parseLine(line);

        executeCommand(cmd);
        //cout << endl;
    }
}

int main(int argc, char* argv[])
{
    if(argc == 2)
    {
        //batch mode
        ifstream file(argv[1]);
        if(!file)
        {
            cerr << "Error: cannot open batch file\n";
            return 1;
        }
        runShell(file, false);
    }
    else 
    {
        runShell(cin, true);
    }
    return 0;
    
}


// int main()
// {
//     
//     bool quit = false;
//     while(!quit)
//     {
//         char cwd[PATH_MAX]; // set array with max path size
//         if (!getcwd(cwd, sizeof(cwd))) { // puts cd in cwd, with max size of path size
//             strncpy(cwd, "?", sizeof(cwd)); 
//             cwd[sizeof(cwd) - 1] = '\0'; // makes sure theres a null at the end instead of being empty
//         }


//         cout << cwd << " # " << flush; // flush forces output

//         string line;
//         if (!getline(cin, line)) {
//             // EOF (Ctrl+D) or input error => quit cleanly
//             cout << "\n";
//             break;
//         }
        
//         // now we have a "command" in line
//         if(line.empty() || isWhitespaceOnly(line))
//         {
//             continue;
//         }
//         // now we know we actually have an input
//         Command cmd = parseLine(line);

//         executeCommand(cmd);
//     }
//     return 0;
    
// }
