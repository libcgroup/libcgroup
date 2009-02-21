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
 *
 * NOTE: The grammar has been modified, not to be the most efficient, but
 * to allow easy updation of internal data structures.
 */
%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>

int yylex(void);
extern int line_no;
extern char *yytext;

void yyerror(char *s)
{
	fprintf(stderr, "error at line number %d at %c:%s", line_no, *yytext,
		s);
}

int yywrap(void)
{
	return 1;
}

%}

%token ID MOUNT GROUP PERM TASK ADMIN

%union {
	char *name;
	char chr;
	int val;
}
%type <name> ID namevalue_conf
%type <val> mountvalue_conf mount task_namevalue_conf admin_namevalue_conf
%type <val> admin_conf task_conf task_or_admin group_conf group start
%start start
%%

start   : start group
	{
		$$ = $1;
	}
        | start mount
	{
		$$ = $1;
	}
        |
	{
		$$ = 1;
	}
        ;

group   :       GROUP ID '{' group_conf '}'
	{
		$$ = $4;
		if ($$)
			cgroup_config_insert_cgroup($2);
		else {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;

group_conf
        :       ID '{' namevalue_conf '}'
	{
		$$ = cgroup_config_parse_controller_options($1, $3);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        |       group_conf ID '{' namevalue_conf '}' 
	{
		$$ = cgroup_config_parse_controller_options($2, $4);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        |       PERM '{' task_or_admin '}'
	{
		$$ = $3;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;

namevalue_conf
        :       ID '=' ID ';'
	{
		$1 = realloc($1, strlen($1) + strlen($3) + 2);
		$1 = strncat($1, " ", strlen(" "));
		$$ = strncat($1, $3, strlen($3));
		free($3);
	}
        |       namevalue_conf ID '=' ID ';'
	{
		int len = 0;
		if ($1)
			len = strlen($1);
		$2 = realloc($2, len + strlen($2) + strlen($4) + 3);
		$2 = strncat($2, " ", strlen(" "));
		$$ = strncat($2, $4, strlen($4));
		if ($1) {
			$2 = strncat($2, ":", strlen(":"));
			$$ = strncat($2, $1, strlen($1));
		}
		free($4);
	}
	|
	{
		$$ = NULL;
	}
        ;

task_namevalue_conf
        :       ID '=' ID ';'
	{
		$$ = cgroup_config_group_task_perm($1, $3);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        |       task_namevalue_conf ID '=' ID ';'
	{
		$$ = $1 && cgroup_config_group_task_perm($2, $4);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;

admin_namevalue_conf
        :       ID '=' ID ';'
	{
		$$ = cgroup_config_group_admin_perm($1, $3);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        |       admin_namevalue_conf ID '=' ID ';'
	{
		$$ = $1 && cgroup_config_group_admin_perm($2, $4);
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;

task_or_admin
        :       TASK '{' task_namevalue_conf '}' admin_conf
	{
		$$ = $3 && $5;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        |       ADMIN '{' admin_namevalue_conf '}' task_conf
	{
		$$ = $3 && $5;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;

admin_conf:	ADMIN '{' admin_namevalue_conf '}'
	{
		$$ = $3;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
	;

task_conf:	TASK '{' task_namevalue_conf '}'
	{
		$$ = $3;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
	;

mountvalue_conf
        :       ID '=' ID ';'
	{
		if (!cgroup_config_insert_into_mount_table($1, $3)) {
			cgroup_config_cleanup_mount_table();
			$$ = 0;
			return $$;
		}
		$$ = 1;
	}
        |       mountvalue_conf ID '=' ID ';'
	{
		if (!cgroup_config_insert_into_mount_table($2, $4)) {
			cgroup_config_cleanup_mount_table();
			$$ = 0;
			return $$;
		}
		$$ = 1;
	}
        ;

mount   :       MOUNT '{' mountvalue_conf '}'
	{
		$$ = $3;
		if (!$$) {
			fprintf(stderr, "parsing failed at line number %d\n",
				line_no);
			$$ = 0;
			return $$;
		}
	}
        ;


%%
