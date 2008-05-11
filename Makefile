# 
#  Copyright IBM Corporation. 2007
# 
#  Authors:	Balbir Singh <balbir@linux.vnet.ibm.com>
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of version 2.1 of the GNU Lesser General Public License
#  as published by the Free Software Foundation.
# 
#  This program is distributed in the hope that it would be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
YACC_DEBUG=-t
DEBUG=-DDEBUG
INC=-I.
CFLAGS=-g -O2 -Wextra $(INC) 
LIBS= -lcg
LDFLAGS= -L .
INSTALLPREFIX=

all: cgconfig libcg.so

cgconfig: libcg.so config.c y.tab.c lex.yy.c libcg.h file-ops.c
	$(CC) $(CFLAGS) -o $@ y.tab.c lex.yy.c config.c file-ops.c $(LDFLAGS) $(LIBS)

y.tab.c: parse.y lex.yy.c
	byacc -v -d parse.y

lex.yy.c: lex.l
	flex lex.l

libcg.so: api.c libcg.h
	$(CXX) $(CFLAGS) -shared -fPIC -o $@ api.c

install: libcg.so
	\cp libcg.h $(INSTALLPREFIX)/usr/include
	\cp libcg.so $(INSTALLPREFIX)/usr/lib

uninstall: libcg.so
	\rm $(INSTALLPREFIX)/usr/include/libcg.h
	\rm $(INSTALLPREFIX)/usr/lib/libcg.so

clean:
	\rm -f y.tab.c y.tab.h lex.yy.c y.output cgconfig libcg.so
