%{
#include <ctype.h>
#include <stdio.h>
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
start:     statement
statement: IDENTIFIER '=' expression  {printf ("[parser]=%f\n",$3);}
    |      expression                 {printf ("[parser]=%f\n",$1);}
    ;

expression: expression '+' NUMBER {printf("1jlj"); $$ = $1 + $3; }
    |       expression '-' NUMBER { printf("kalj");$$ = $1 - $3; }
    |       NUMBER  { printf("parser:Number:%f",$1);
                      $$ = $1;}
    ;

