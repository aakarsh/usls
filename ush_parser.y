%{
#include <ctype.h>
#include <stdio.h>

extern FILE *yyin;

%}

%union {
  char* str;
  double dbl;
}

%token <value> NAME 
%token <dbl>   NUMBER
%token <str>   IDENTIFIER 
%type  <dbl>   expression
%%

statement-list:     statement ';'
    |               statement-list statement 
statement: IDENTIFIER '=' expression  {printf ("[parser]=%.2f\n",$3);}
    |      expression                 {printf ("[parser]=%.2f\n",$1);}
    ;

expression: expression '+' NUMBER { $$ = $1 + $3; printf("[parser]%d\n",$$);  }
    |       expression '-' NUMBER { $$ = $1 - $3; printf("[parser]%d\n",$$);  }
    |       NUMBER                {  printf("[parser]:%.2f\n",$1); $$ = $1;       }
    ;
%%
main()
{
  while(!feof(stdin)) {
    yyparse();
  }
  return 0;
}

yyerror(s)
char *s;
{
    fprintf(stderr, "%s\n", s);
}
