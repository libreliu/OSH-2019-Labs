%{
#define YYSTYPE pt_node_t
#include "pt.hpp"
#include "y.tab.h"

#include <stdio.h>
#include <string.h>

#define PUTBACK(t) do{yylval = strdup(t);}while(0)
%}

WORD        [A-Za-z_]+
IO_NUMBER      [0-9]+
DLESS       <<
DGREAT      >>
LESSAND     <&
GREATAND    >&
LESSGREAT   <>
DLESSDASH   <<-
  /* Here requires ' ' to escape */
CLOBBER     >'|'
SINGLE_CHAR .

BLANK_CHARS [ \n]

%%

{BLANK_CHARS} { printf("ignored: '%s'\n", yytext);}

{WORD} { PUTBACK(yytext);
         return WORD; }

{IO_NUMBER} { PUTBACK(yytext);
         return IO_NUMBER; }

{DLESS} { return DLESS; }

{DGREAT} { return DGREAT; }

{LESSAND} { return LESSAND; }

{GREATAND} { return GREATAND; }

{LESSGREAT} { return LESSGREAT; }

{DLESSDASH} { return DLESSDASH; }

{CLOBBER} { return CLOBBER; }

{SINGLE_CHAR} { printf("Flex: %d\n", *yytext); return *yytext; }

%%

int yywrap() {
    return 1;
}
