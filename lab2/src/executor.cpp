#include "executor.hpp"

using namespace std;

void yash_error(const char *msg) {
    fprintf(stderr, "yash: ");
    perror(msg);
}

Shell::Shell(pt_node_t pte) {
    this->pt = reinterpret_cast<tree<pt_val> *>(pte);
}

void Shell::clear() {
    this->wait_list.clear();
    this->wait_list_fake.clear();
    if (this->pt != NULL) {
        delete this->pt;
    }
}

void IODesc::set(int fd, int new_fd) {
    // Notice: if duplicate fd pairs are set, this means redirection conflicts
    // between pipe and redirection directives.
    // In YaSH, I follow the latter directive(redirection), but I also needed to close pipe fd early (after spawning)
    // (Or sth will be jammed!)
    if (desc.find(fd) != desc.end()) {
        // put into early close list
        this->close_fd.push_back(desc.find(fd)->second);
        this->desc.erase(desc.find(fd));
    }
    desc.insert(pair<int, int>(fd, new_fd));
}

int IODesc::get(int fd) {
    if (desc.find(fd) != desc.end())
        return desc.find(fd)->second;
    else
        return fd;
}

// Non-zero on error
// if enable, use fd -> fd_number; else use stdin/stdout -> fd_number
int IODesc::set(int fd, bool enable_fd, std::string op, std::string filename) {
    if (op == "<") {
        int fd_number = open(filename.c_str(), O_RDONLY);
        if (fd_number < 0) {
            yash_error(filename.c_str());
            return 1;
        }
        this->set(enable_fd ? fd : STDIN_FILENO, fd_number);
    } else if (op == ">") {
        int fd_number = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                             0666); // -rw-rw-rw-
        if (fd_number < 0) {
            yash_error(filename.c_str());
            return 1;
        }
        this->set(enable_fd ? fd : STDOUT_FILENO, fd_number);
    } else if (op == ">>") {
        int fd_number = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666); // -rw-rw-rw-
        if (fd_number < 0) {
            yash_error(filename.c_str());
            return 1;
        }
        this->set(enable_fd ? fd : STDOUT_FILENO, fd_number);
    }

    return 0;
}

    // waitpid(pid, &status, 0);

    // /* Close redirected fd */
    // for (auto &i: redirect.desc) {
    //     close(i.second);
    // }
    
	// if (WIFEXITED(status)) {
	// 	printf("Child exited with code %d\n", WEXITSTATUS(status));
    //     return WEXITSTATUS(status);
    // } else if (WIFSIGNALED(status)) {
	// 	printf("Child terminated abnormally, signal %d\n", WTERMSIG(status));
    //     return 137; //for laziness
    // } else return -1;

int Shell::waitAll(int ret_pid) {
    // Check if it's in fake lists
    if (this->wait_list_fake.find(ret_pid) != this->wait_list_fake.end()) {
        int ret_val = this->wait_list_fake.find(ret_pid)->second;
        this->wait_list_fake.erase(this->wait_list_fake.find(ret_pid));
        return ret_val;
    }

    if (this->wait_list.find(ret_pid) == this->wait_list.end()) {
        throw std::out_of_range("ret_pid not in waiting list");
        return -1;
    }

    for (auto &i : this->wait_list) {
        YALOG("wait for: %d\n", i.first);
    }

    int ret = 0, status;
    pid_t pid;
    while (!this->wait_list.empty()) {
        pid = wait(&status);
        YALOG("Child %d returned %d\n", pid, WEXITSTATUS(status));
        if (pid == ret_pid)
            ret = WEXITSTATUS(status);
        this->wait_list.find(pid)->second.closeRedirections(); // Do when child exits
        this->wait_list.erase(this->wait_list.find(pid));
    }
    return ret;
}

int Shell::runPipeSequence(const tree<pt_val>::sibling_iterator& node) {
    // Figure out how many commands in pipe
    int cmd_num = node.number_of_children();
    int last_pid;
    //cout << cmd_num << endl;
    IODesc redirect;
    if (cmd_num == 1) {  // Only got one command to execute
        if ((last_pid = this->runSimpleCommand(pt->begin(node), redirect)) < 0)
            return -1;
        
        return this->waitAll(last_pid);
    } else {
        // Create (cmd_num - 1) pipe
        map<int, int> pipe_seq;  // Okay, since pipe will return unique fds
        int pipe_fd[2]; // [0] - read end; [1] - write end;
        for (int i = 0; i < cmd_num - 1; i++) {
            if (pipe(pipe_fd) < 0) {
                yash_error("error creating pipe");
                return -1;
            }
            pipe_seq.insert(pair<int,int>(pipe_fd[0], pipe_fd[1]));
        }

        // Assign and run
        IODesc redirect;
        auto it = pipe_seq.begin(); int next_read_end; auto cmd = pt->begin(node);
        for (int i = 0; i < cmd_num; i++, ++it, ++cmd) { // All prog except the last and the first
            if (i != cmd_num - 1)
                redirect.set(STDOUT_FILENO, it->second);
            if (i != 0)
                redirect.set(STDIN_FILENO, next_read_end);

            // Tell child process to close unrelevant pipe (ends)
            for (auto &j: pipe_seq) { // *short-circult* or end() accessed here
                if ( (i != cmd_num - 1) ? (j.first != it->second) : true   
                    && (i != 0 ? (j.first != next_read_end) : true))
                    redirect.setClose(j.first);

                if ( (i != cmd_num - 1) ? (j.second != it->second) : true
                    && ( (i != 0) ? (j.second != next_read_end) : true))
                    redirect.setClose(j.second);
            }

            if (i != cmd_num - 1)
                next_read_end = it->first;
            
            if (i == cmd_num - 1)
                last_pid = this->runSimpleCommand(cmd, redirect);
            else
                this->runSimpleCommand(cmd, redirect);

            redirect.clearAll();
        }
        
        return this->waitAll(last_pid);
    }
}

void IODesc::clearAll() {
    this->desc.clear();
    this->close_fd.clear();
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
        YALOG("Arg %d: %s\n", i, args[i]);
    }
}

void IODesc::runRedirections() {
    for (auto &i: this->desc) {
        YALOG("dup2(%d, %d);\n", i.second, i.first);
        dup2(i.second, i.first);
    }
}

// fd: fd number to be closed
void IODesc::setClose(int fd) {
    this->close_fd.push_back(fd);
}

// close unrelevant pipes (do when child process spawns)
void IODesc::runClose() {
    for (auto &i: this->close_fd) {
        close(i);
    }
}

// close redirected fds (parent process, do when child exits)
void IODesc::closeRedirections() {
    /* Close redirected fd */
    for (auto &i: this->desc) {
        close(i.second);
    }
}

int Shell::returnBuiltins(int ret, IODesc redirect) {
    int fake = this->genFakePid();
    redirect.closeRedirections();
    this->wait_list_fake.insert(pair<int,int>(fake, ret));
    return fake;
}

// return != 0 if not builtins, == 0 and ret_val(pid.fake) if is builtin
int Shell::runBuiltins(char **args, IODesc redirect, int &ret_val) {
    if (!strcmp(args[0], "cd")) {
        if (args[1] == NULL) {
            ret_val = this->returnBuiltins(-1, redirect);
            return 0;
        } else {
            chdir(args[1]);
            ret_val = this->returnBuiltins(0, redirect);
            return 0;
        }
    } else if (!strcmp(args[0], "export")) {
        if (args[1] == NULL) {
            for (int i = 0; environ[i] != NULL; i++) {
                write(redirect.get(STDOUT_FILENO), environ[i], strlen(environ[i]) );
                write(redirect.get(STDOUT_FILENO), "\n", 1);
            }

            ret_val = this->returnBuiltins(0, redirect);
            return 0;
        } else { //key=val
            int equal_pos = 0;
            for (; args[1][equal_pos] != '=' && args[1][equal_pos] != '\0'; equal_pos++);
            if (args[1][equal_pos] == '\0' || equal_pos == 0) {
                write(redirect.get(STDERR_FILENO), "not valid export stmt\n", strlen("not valid export stmt\n"));
                ret_val = this->returnBuiltins(-1, redirect);
                return 0;
            }
            args[1][equal_pos] = '\0';
            setenv(args[1], (args[1]) + equal_pos + 1, 1);
            ret_val = this->returnBuiltins(0, redirect);
            return 0;
        }
    } else if (!strcmp(args[0], "pwd")) {
        char cwd[4096];
        getcwd(cwd, sizeof(cwd));
        write(redirect.get(STDOUT_FILENO), cwd, strlen(cwd));
        write(redirect.get(STDOUT_FILENO), "\n", 1);
        ret_val = this->returnBuiltins(0, redirect);
        return 0;
    } else {
        return -1;
    }
}

int Shell::genFakePid() {
    int i;
    for (i = 10; this->wait_list.find(i) != this->wait_list.end(); i++);
    return i;
}

int Shell::forkExec(char **args, IODesc redirect) {
    int status;
    /* Check & run builtin */
    if (this->runBuiltins(args, redirect, status) == 0) { // is builtin
        return status; // fake pid
    }

    /* Run program */
    pid_t pid = fork();
    if (pid == 0) {
        /* setting up redirections */
        redirect.runRedirections();

        /* Closing unnessary fds */
        redirect.runClose();

        /* do exec */
        execvp(args[0], args);
        /* execvp failed */
        yash_error(args[0]);

        _exit(127);
    }
    /* parent process */

    // Add to wait_list
    this->wait_list[pid] = redirect;
    // waitpid(pid, &status, 0);
    return pid;
    // /* Close redirected fd */
    // for (auto &i: redirect.desc) {
    //     close(i.second);
    // }
    
	// if (WIFEXITED(status)) {
	// 	printf("Child exited with code %d\n", WEXITSTATUS(status));
    //     return WEXITSTATUS(status);
    // } else if (WIFSIGNALED(status)) {
	// 	printf("Child terminated abnormally, signal %d\n", WTERMSIG(status));
    //     return 137; //for laziness
    // } else return -1;
}

std::ostream& operator<< (std::ostream& stream, const pt_val & val) {
    stream << "{";
    for (auto &i : val) {
        stream << "\"" << i.first << "\":\"" << i.second << "\"";
    }
    stream << "}";
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
    //cout << type << endl;

    tree<pt_val>::sibling_iterator child_sib = pt->begin(node);
    std::vector<std::string> args;
    if (type == "prefix_word_suffix") {
        // cmd_word
        args.push_back((++(pt->begin(node)))->find("value")->second);
        // cmd_prefix
        for (auto prefix_sib = pt->begin(child_sib); prefix_sib != pt->end(child_sib); prefix_sib++) {
            if (prefix_sib->find("name")->second == "io_redirect") {
                //Judge if it's io_file
                auto io_redirect_sib = pt->begin(prefix_sib);
                if (io_redirect_sib->find("name")->second == "io_file") {
                    // See if we have IO_NUMBER
                    int io_number;
                    bool io_number_field;
                    if (prefix_sib->find("value") != prefix_sib->end()) {
                        // Got one
                        io_number = std::stoi(prefix_sib->find("value")->second);
                        io_number_field = true;
                    } else {
                        io_number_field = false;
                    }
                    
                    // See the value of io_file_op; notice this assumes "io_file_op filename" sequence
                    auto io_file_sib = pt->begin(io_redirect_sib);
                    std::string io_op = io_file_sib->find("value")->second;

                    // Grab filename
                    io_file_sib++;
                    std::string filename = io_file_sib->find("value")->second;

                    // Set up redirection
                    if (redirect.set(io_number, io_number_field, io_op, filename) != 0)
                        return -1;
                }
            }
        }
        // cmd_suffix
        child_sib++; child_sib++;
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
                    bool io_number_field;
                    if (suffix_sib->find("value") != suffix_sib->end()) {
                        // Got one
                        io_number = std::stoi(suffix_sib->find("value")->second);
                        io_number_field = true;
                    } else {
                        io_number_field = false;
                    }
                    
                    // See the value of io_file_op; notice this assumes "io_file_op filename" sequence
                    auto io_file_sib = pt->begin(io_redirect_sib);
                    std::string io_op = io_file_sib->find("value")->second;

                    // Grab filename
                    io_file_sib++;
                    std::string filename = io_file_sib->find("value")->second;

                    // Set up redirection
                    if (redirect.set(io_number, io_number_field, io_op, filename) != 0)
                        return -1;
                }
            }
        }

        argv = this->makeArgs(args);
        printArgs(argv);
        return this->forkExec(argv, redirect);

    } else if (type == "prefix_word") {
        // cmd_word
        args.push_back((++(pt->begin(node)))->find("value")->second);
        // cmd_prefix
        for (auto prefix_sib = pt->begin(child_sib); prefix_sib != pt->end(child_sib); prefix_sib++) {
            if (prefix_sib->find("name")->second == "io_redirect") {
                //Judge if it's io_file
                auto io_redirect_sib = pt->begin(prefix_sib);
                if (io_redirect_sib->find("name")->second == "io_file") {
                    // See if we have IO_NUMBER
                    int io_number;
                    bool io_number_field;
                    if (prefix_sib->find("value") != prefix_sib->end()) {
                        // Got one
                        io_number = std::stoi(prefix_sib->find("value")->second);
                        io_number_field = true;
                    } else {
                        io_number_field = false;
                    }
                    
                    // See the value of io_file_op; notice this assumes "io_file_op filename" sequence
                    auto io_file_sib = pt->begin(io_redirect_sib);
                    std::string io_op = io_file_sib->find("value")->second;

                    // Grab filename
                    io_file_sib++;
                    std::string filename = io_file_sib->find("value")->second;

                    // Set up redirection
                    if (redirect.set(io_number, io_number_field, io_op, filename) != 0)
                        return -1;
                }
            }
        }

        argv = this->makeArgs(args);
        printArgs(argv);
        return this->forkExec(argv, redirect);
    } else if (type == "prefix") { // (used to create a file?) - ignore this
        
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
                    bool io_number_field;
                    if (suffix_sib->find("value") != suffix_sib->end()) {
                        // Got one
                        io_number = std::stoi(suffix_sib->find("value")->second);
                        io_number_field = true;
                    } else {
                        io_number_field = false;
                    }
                    
                    // See the value of io_file_op; notice this assumes "io_file_op filename" sequence
                    auto io_file_sib = pt->begin(io_redirect_sib);
                    std::string io_op = io_file_sib->find("value")->second;

                    // Grab filename
                    io_file_sib++;
                    std::string filename = io_file_sib->find("value")->second;

                    // Set up redirection
                    if (redirect.set(io_number, io_number_field, io_op, filename) != 0)
                        return -1;
                }
            }
        }

        argv = this->makeArgs(args);
        printArgs(argv);
        return this->forkExec(argv, redirect);
    } else if (type == "name") {
        //cout << child_sib->find("value")->second << endl;
        args.push_back(child_sib->find("value")->second);
        argv = this->makeArgs(args);
        //printArgs(argv);

        return this->forkExec(argv, redirect);
    }
    
    throw std::out_of_range("unexpected simple_command type");
}

void Shell::run() {
    tree<pt_val>::sibling_iterator entry = pt->begin();  // find 'entry' iterator
    tree<pt_val>::sibling_iterator pipe_seq = pt->begin(entry);
    //cout << *pipe_seq << endl;

    /* Give node to runSimpleCommand */
    this->runPipeSequence(pipe_seq);

}