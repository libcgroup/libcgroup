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
"mount"		{yylval.name = NULL; return MOUNT;}
"task"		{yylval.name = NULL; return TASK;}
"admin"		{yylval.name = NULL; return ADMIN;}
"perm"		{yylval.name = NULL; return PERM;}
"group"		{yylval.name = NULL; return GROUP;}
"namespace"	{yylval.name = NULL; return NAMESPACE;}
"template"	{yylval.name = NULL; return TEMPLATE;}
"systemd"     	{yylval.name = NULL; return SYSTEMD;}
"default"	{yylval.name = strdup(yytext); return DEFAULT;}
[a-zA-Z0-9_\-\/\.\,\%\@\\]+ {yylval.name = strdup(yytext); return ID;}
\"[^"]*\"	{
	if (yytext[0] != '\0' && yytext[1] != '\0') {
		yylval.name = strdup(yytext+1); 
		yylval.name[strlen(yylval.name)-1] = '\0';
	} else {
		yylval.name = strdup("");
	} 
	return ID;
	} 
.	{return yytext[0];}
%%

