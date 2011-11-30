/*
 * Copyright IBM Corporation. 2007
 *
 * Authors:	Balbir Singh <balbir@linux.vnet.ibm.com>
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

%{
#include <string.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include "parse.h"
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
^#.*[ \t]*  {/* Comments */}
^\*.*[ \t]* {/* Comments */}
"mount"		{return MOUNT;}
"task"		{return TASK;}
"admin"		{return ADMIN;}
"perm"		{return PERM;}
"group"		{return GROUP;}
"namespace"	{return NAMESPACE;}
"default"	{return DEFAULT;}
[a-zA-Z0-9_\-\/\.\,]+ {yylval.name = strdup(yytext); return ID;}
\"[^"]*\" {yylval.name = strdup(yytext+1); yylval.name[strlen(yylval.name)-1] = '\0'; return ID; }
.	{return yytext[0];}
%%

