%{
#include <ctype.h>
#include <stdio.h>

extern FILE *yyin;
void yyerror(char *s);
int yylex();
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
    |               statement

statement: IDENTIFIER '=' expression  {printf ("[parser]=%.2f\n",$3);}
    |      expression                 {printf ("[parser]=%.2f\n",$1);}
    ;
expression: expression '+' NUMBER { $$ = $1 + $3; printf("[parser]%.2f\n",$$);  }
    |       expression '-' NUMBER { $$ = $1 - $3; printf("[parser]%.2f\n",$$);  }
    |       NUMBER                { printf("[parser]:%.2f\n",$1); $$ = $1;     }
    ;
%%

int main(int argc, char* argv[])
{
  while(!feof(stdin)) {
    yyparse();
  }
  return 0;
}

void yyerror(char *s)
//char *s;
{
    fprintf(stderr, "%s\n", s);
}
