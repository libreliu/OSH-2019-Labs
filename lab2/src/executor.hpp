#ifndef __EXECUTOR_HPP_INCLUDED__
#define __EXECUTOR_HPP_INCLUDED__

#include "pt.hpp"
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
// O_RDONLY and fnctl stuff
#include <fcntl.h>


std::ostream& operator<< (std::ostream& stream, const pt_val & val);

//IO Redirect array
class IODesc {
public:
    void set(int fd, int new_fd); //redirect fd to new_fd
    int set(int fd, bool enable_fd, std::string op, std::string filename);
    int get(int fd);
    void clearAll();
    void runRedirections();
    void closeRedirections();
    void setClose(int fd);
    void runClose();
    std::map<int,int> desc;
    std::vector<int> close_fd;
};

class Shell {
public:
    int runSimpleCommand(const tree<pt_val>::sibling_iterator& node, IODesc redirect);
    int runPipeSequence(const tree<pt_val>::sibling_iterator& node);
    
    void run();
    void clear();
    Shell(pt_node_t pte);
    tree<pt_val>* pt;
protected:
    char** makeArgs(std::vector<std::string> args);
    int forkExec(char **args, IODesc redirect);
    int waitAll(int ret_pid); // waiting for all child process to complete, and return ret_pid's return value
    
    int genFakePid();
    int returnBuiltins(int ret, IODesc redirect);
    int runBuiltins(char **args, IODesc redirect, int &ret_val);
    std::map<int, class IODesc> wait_list; // child pids in wait
    std::map<int, int> wait_list_fake;     // fake wait lists, used to provide return values on builtins
};

#endif