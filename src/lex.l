/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright IBM Corporation. 2007
 *
 * Authors:	Balbir Singh <balbir@linux.vnet.ibm.com>
 */

%{
#include <string.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include "parse.h"
#pragma GCC diagnostic ignored "-Wunused-function"

int line_no = 1;
jmp_buf parser_error_env;
static char token_buf[FILENAME_MAX];

#define YY_FATAL_ERROR(msg) \
	do { \
		fprintf(stderr, "%s\n", msg); \
		longjmp(parser_error_env, 1); \
	} while(0);
%}

%option nounput noinput

%%
\n	{line_no++;}
[ \t]	{/* DO NOTHING */}
^[ \t]*#.*[ \t]*  {/* Comments */}
^\*.*[ \t]* {/* Comments */}
"mount"		{return MOUNT;}
"task"		{return TASK;}
"admin"		{return ADMIN;}
"perm"		{return PERM;}
"group"		{return GROUP;}
"namespace"	{return NAMESPACE;}
"template"	{return TEMPLATE;}
"systemd"     	{return SYSTEMD;}
"default"	{   
    strncpy(token_buf, yytext, sizeof(token_buf)-1);  
    token_buf[sizeof(token_buf)-1] = '\0';  
    yylval.name = token_buf;   
    return DEFAULT;  
}  
[a-zA-Z0-9_\-\/\.\,\%\@\\]+ {   
    strncpy(token_buf, yytext, sizeof(token_buf)-1);  
    token_buf[sizeof(token_buf)-1] = '\0';  
    yylval.name = token_buf;   
    return ID;  
}   
\"[^"]*\"	{     
    int len = strlen(yytext) - 2;  
    if (len >= sizeof(token_buf)) len = sizeof(token_buf) - 1;  
    strncpy(token_buf, yytext+1, len);  
    token_buf[len] = '\0';  
    yylval.name = token_buf;   
    return ID;   
}
.	{return yytext[0];}
%%

