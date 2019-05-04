#include <stdio.h>
#include "tree.hh"
#include "tree_util.hh"
#include "pt.hpp"

#include "executor.hpp"
#include "y.tab.h"
#include "lex.h"
#include <iostream>

pt_node_t pt_entry;



int main(int argc, char *argv[]) {
    if (argc == 2) {
        YY_BUFFER_STATE buf;
        int ret;
        buf = yy_scan_string(argv[1]);
        ret = yyparse();
        yy_delete_buffer(buf);
        #ifdef YASH_DEBUG
            kptree::print_tree_bracketed<pt_val>(*(reinterpret_cast<tree<pt_val> *>(pt_entry)));
            std::cout << "Run result:" << std::endl;
        #endif
        if (ret == 1) {
            exit(255);
            //TODO: Release all the new'ed trees (by overloading new[] op, maybe)
        }
        auto a = Shell(pt_entry);
        a.run();
        return 0;
    }
    //Interactive mode
    char cmd_buf[1024];
    int ret;
    auto sh = Shell(NULL);
    while (1) {
        printf("# ");
        fflush(NULL);
        if (fgets(cmd_buf, 1024, stdin) == NULL) {
            printf("exit\n");
            break;
        }

        YY_BUFFER_STATE buf;
        buf = yy_scan_string(cmd_buf);
        ret = yyparse();
        yy_delete_buffer(buf);

        if (ret != 1) {  // Valid command
            sh.pt = reinterpret_cast<tree<pt_val> *>(pt_entry);
            sh.run();
            sh.clear();
        }
    }




    
    return 0;
}

// int main() {
//     /* 输入的命令行 */
//     char cmd[256];
//     /* 命令行拆解成的各部分，以空指针结尾 */
//     char *args[128];
//     while (1) {
//         /* 提示符 */
//         printf("# ");
//         fflush(stdin);
//         fgets(cmd, 256, stdin);
//         /* 清理结尾的换行符 */
//         int i;
//         for (i = 0; cmd[i] != '\n'; i++)
//             ;
//         cmd[i] = '\0';
//         /* 拆解命令行 */
//         args[0] = cmd;
//         for (i = 0; *args[i]; i++)
//             for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
//                 if (*args[i+1] == ' ') {
//                     *args[i+1] = '\0';
//                     args[i+1]++;
//                     break;
//                 }
//         args[i] = NULL;

//         /* 没有输入命令 */
//         if (!args[0])
//             continue;

//         /* 内建命令 */
//         if (strcmp(args[0], "cd") == 0) {
//             if (args[1])
//                 chdir(args[1]);
//             continue;
//         }
//         if (strcmp(args[0], "pwd") == 0) {
//             char wd[4096];
//             puts(getcwd(wd, 4096));
//             continue;
//         }
//         if (strcmp(args[0], "exit") == 0)
//             return 0;

//         /* 外部命令 */
//         pid_t pid = fork();
//         if (pid == 0) {
//             /* 子进程 */
//             execvp(args[0], args);
//             /* execvp失败 */
//             return 255;
//         }
//         /* 父进程 */
//         wait(NULL);
//     }
// }