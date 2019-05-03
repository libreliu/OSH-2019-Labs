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
    void set(int fd, int new_fd);
    int get(int fd);
    std::map<int,int> desc;
};

class Shell {
public:
    int runSimpleCommand(const tree<pt_val>::sibling_iterator& node, IODesc redirect);
    void run();
    
    Shell(pt_node_t pte);
    tree<pt_val>* pt;
protected:
    char** makeArgs(std::vector<std::string> args);
};

#endif