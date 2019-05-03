#include "executor.hpp"

using namespace std;

Shell::Shell(pt_node_t pte) {
    this->pt = reinterpret_cast<tree<pt_val> *>(pte);
}

void IODesc::set(int fd, int new_fd) {
    desc.insert(pair<int, int>(fd, new_fd));
}

int IODesc::get(int fd) {
    return desc.find(fd)->second;
}

char** Shell::makeArgs(std::vector<std::string> args) {
    /* Alloc for pointer array first */
    char **arr = (char **)malloc(sizeof(char *) * (args.size() + 1) ); // NULL sentineal
    auto i = args.begin();
    for (int count = 0; i != args.end(); i++, count++) {
        arr[count] = strcpy( (char *)malloc(sizeof(char) * (i->length() + 1) ), i->c_str() );
    }
    arr[args.size()] = 0;
    return arr;
}

void printArgs(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        printf("Arg %d: %s\n", i, args[i]);
    }
}

std::ostream& operator<< (std::ostream& stream, const pt_val & val) {
    for (auto &i : val) {
        stream << "{\"" << i.first << "\",\"" << i.second << "\"}";
    }
    return stream;
}

/* IO Redirect Defaults */
// return return_value
int Shell::runSimpleCommand(const tree<pt_val>::sibling_iterator& node, IODesc redirect) {
    /* Grab command */
    std::string command_name;
    
    /* Args table */
    char **argv;

    /* Check for cmd type */
    std::string type = (  *(node)  ).find("type")->second;
    cout << type << endl;

    tree<pt_val>::sibling_iterator child_sib = pt->begin(node);
    std::vector<std::string> args;
    if (type == "prefix_word_suffix") {
        
    } else if (type == "prefix_word") {
        
    } else if (type == "prefix") {

    } else if (type == "name_suffix") {
        // cmd_name
        args.push_back(child_sib->find("value")->second);
        // cmd_suffix
        child_sib++;
        for (auto suffix_sib = pt->begin(child_sib); suffix_sib != pt->end(child_sib); suffix_sib++) {
            // Judge if it's cmd_word
            if (suffix_sib->find("name")->second == "cmd_word") {
                args.push_back(suffix_sib->find("value")->second);
            } else if (suffix_sib->find("name")->second == "io_redirect") {
                //Judge if it's io_file
                auto io_redirect_sib = pt->begin(suffix_sib);
                if (io_redirect_sib->find("name")->second == "io_file") {
                    // See if we have IO_NUMBER
                    int io_number;
                    if (suffix_sib->find("value") != suffix_sib->end()) {
                        // Got one
                        io_number = std::stoi(suffix_sib->find("value")->second);
                    }
                    // See the value of io_file_op; notice this assumes "io_file_op filename" sequence
                    auto io_file_sib = pt->begin(io_redirect_sib);
                    std::string io_op = io_file_sib->find("value")->second;

                    // Grab filename
                    io_file_sib++;
                    std::string filename = io_file_sib->find("value")->second;
                    //cout << filename << endl;
                    if (io_op == "<") { // cmd < filename
                        
                    } else if (io_op == ">")  { // cmd > filename
                        int fd_number = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
                        if (fd_number < 0) {
                            perror("Error opening stdout redirection");
                            return;
                        }
                    }
                }
            }
        }
        argv = this->makeArgs(args);
        printArgs(argv);
    } else if (type == "name") {
        //cout << child_sib->find("value")->second << endl;
        args.push_back(child_sib->find("value")->second);
        argv = this->makeArgs(args);
        //printArgs(argv);

        /* Run program */
        pid_t pid = fork();
        if (pid == 0) {
            /* 子进程 */
            execvp(argv[0], argv);
            /* execvp失败 */
            exit(255);
        }
        /* 父进程 */
        wait(NULL);
    }
    
}

void Shell::run() {
    tree<pt_val>::sibling_iterator entry = pt->begin();  // find 'entry' iterator
    tree<pt_val>::sibling_iterator simple_cmd = pt->begin(entry);
    cout << *simple_cmd << endl;

    IODesc test_desc;
    /* Give node to runSimpleCommand */
    this->runSimpleCommand(simple_cmd, test_desc);

}