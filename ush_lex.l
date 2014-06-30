%top{
#include <unistd.h>
#include <stdio.h>
#include "ush_tokens.h"
}


WORD_PATTERN  [a-zA-Z_$-.*/][a-zA-Z0-9_$.*/]*
DIGIT_PATTERN  [0-9]+
MINUS_PATTERN   -
COMMENT #.*
%%

{DIGIT_PATTERN} { 
                   yylval.dval = atoi(yytext); 
                   return NUM_TOK;
                }

{WORD_PATTERN}  { 
                  yylval.sval = strdup(yytext);
                  return WORD_TOK;
                 }

\n     return 0;
.      {
          if (!isblank (yytext [0])) {
            return yytext[0];
          }
       }

%%
