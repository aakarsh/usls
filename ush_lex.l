%{
#include <unistd.h>
#include <stdio.h>
%}
   int nlines = 0;
   int nchars = 0;

DIGIT  [0-9]
IDENTIFIER  [a-zA-Z0-9_$]
%%
{IDENTIFIER}*  { 
                  printf("identifier %s\n",yytext);
              }
{DIGIT}+  {
            printf("An integer:  %s\n",yytext);
          }

\n     nlines++;nchars++;
.      nchars++;

%%
main()
{
   yylex();
   printf("Read %d lines %d chars!\n",nlines,nchars);
   return 0;
}
