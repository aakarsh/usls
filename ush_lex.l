%top{
#include <unistd.h>
#include <stdio.h>
#include "ush_tokens.h"
}
MATH_SYMBOL  [+*-/]
IDENTIFIER  [a-zA-Z_$][a-zA-Z0-9_$]*
PIPE     \|
SEMI     \;
BACKGROUND      &
COMMENT #.*

%%
[0-9]+      { 
                   printf("lex:number [%s]\n",yytext);
                   yylval.dbl = atoi(yytext); 
                   return NUMBER;
              }
{IDENTIFIER}  { 
                  printf("lex:identifier [%s]\n",yytext);
                  yylval.str = strdup(yytext);
                  return IDENTIFIER;
              }

\n     return 0;
.      return yytext[0];

%%
