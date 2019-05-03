%{
#define YYSTYPE pt_node_t

#include <stdio.h>
extern int yylex();
int yyerror(char *msg);

#include "pt.hpp"

inline pt_val PTNAME(std::string x) {
    return pt_mapbuild("name", x);
}

inline pt_val PTNAMEVAL (std::string x, std::string y) {
    auto m = pt_mapbuild("name", x);
    m.insert( (std::pair<std::string, std::string>("value", y)) );
    return m;
}

inline pt_val PTNAMETYPE (std::string name, std::string type) {
    auto m = pt_mapbuild("name", name);
    m.insert( (std::pair<std::string, std::string>("type", type)) );
    return m;
}

/* extern decl on pt_entry; exist in shell.cpp */
extern pt_node_t pt_entry;
%}

%token  WORD
%token  IO_NUMBER

%token  DLESS  DGREAT  LESSAND  GREATAND  LESSGREAT  DLESSDASH
/*      '<<'   '>>'    '<&'     '>&'      '<>'       '<<-'   */

%token  CLOBBER
/*      '>|'   */

/* no support for &> file : redirecting stdout & stderr to file */

%start entry

%%

entry            : simple_command  { printf("Entrypoint Reached.\n"); 
                                     pt_entry = pt_mkchild($1, PTNAME("entry")); }
                 ;

cmd_prefix       :            io_redirect  { $$ = pt_mkchild($1, PTNAME("cmd_prefix")); }
                 | cmd_prefix io_redirect  { $$ = pt_merge($2, $1, -1); }
                 //|            ASSIGNMENT_WORD
                 //| cmd_prefix ASSIGNMENT_WORD
                 ;

simple_command   : cmd_prefix cmd_word cmd_suffix  { 
                     $$ = pt_merge( pt_merge(pt_mkchild($1, PTNAMETYPE("simple_command","prefix_word_suffix")), $2, -1), $3, -1); 
                    } 
                 | cmd_prefix cmd_word      { 
                     $$ = pt_merge($2, pt_mkchild($1, PTNAMETYPE("simple_command", "prefix_word")), -1); }
                 | cmd_prefix               { 
                     $$ = pt_mkchild ($1, PTNAMETYPE("simple_command", "prefix"));  }
                 | cmd_name cmd_suffix      { 
                     $$ = pt_merge($2, pt_mkchild($1, PTNAMETYPE("simple_command", "name_suffix")), -1); }
                 | cmd_name                 { 
                     $$ = pt_mkchild ($1, PTNAMETYPE("simple_command", "name")); }
                 ;

    /* Apply rule 7a */
cmd_name         : WORD { $$ = pt_mkleaf(PTNAMEVAL("cmd_name", $1)); }
                 ;

    /* Apply rule 7a */
cmd_word         : WORD { $$ = pt_mkleaf(PTNAMEVAL("cmd_word", $1)); }
                 ;

cmd_suffix       :            io_redirect  { $$ = pt_mkchild($1, PTNAME("cmd_suffix")); }
                 | cmd_suffix io_redirect  { $$ = pt_merge($2, $1, -1); }
                 |            cmd_word         { $$ = pt_mkchild($1, PTNAME("cmd_suffix")); }
                 | cmd_suffix cmd_word         { $$ = pt_merge($2, $1, -1);}
                 ;

io_redirect      :           io_file   { $$ = pt_mkchild($1, PTNAME("io_redirect")); }
                 | IO_NUMBER io_file   { $$ = pt_mkchild($2, PTNAMEVAL("io_redirect", $1)); }
                 |           io_here   { /* Here documents */ 
                                         $$ = pt_mkchild($1, PTNAME("io_redirect"));
                                        }
                 | IO_NUMBER io_here   { $$ = pt_mkchild($2, PTNAMEVAL("io_redirect", $1)); }
                 ;

io_file          : '<'       filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", "<" ) ), PTNAME("io_file")); }
                 | LESSAND   filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", "<&") ), PTNAME("io_file")); }
                 | '>'       filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", ">" ) ), PTNAME("io_file")); }
                 | GREATAND  filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", ">&") ), PTNAME("io_file")); }
                 | DGREAT    filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", ">>") ), PTNAME("io_file")); }
                 | LESSGREAT filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", "<>") ), PTNAME("io_file")); }
                 | CLOBBER   filename  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_file_op", ">|") ), PTNAME("io_file")); }
                 ;

io_here          : DLESS     here_end  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_here_op", "<<" )), PTNAME("io_here")); }
                 | DLESSDASH here_end  { $$ = pt_mkchild (pt_mksib($2, PTNAMEVAL("io_here_op", "<<-")), PTNAME("io_here")); }
                 ;

here_end         : WORD                { $$ = pt_mkleaf(PTNAMEVAL("here_end", $1)); }      /* Apply rule 3 */
                 ;

filename         : WORD                { $$ = pt_mkleaf(PTNAMEVAL("filename", $1)); }      /* Apply rule 2 */
                 ;

%%

/* yylex() provided by flex */

int yyerror(char *msg) {
    printf("%s (lookahead token = %d)\n", msg, yychar);
    return -1;
}