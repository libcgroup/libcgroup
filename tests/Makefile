LDFLAGS = -L .. -L.
LIBS = -lcgroup -lpthread -ltest_functions
INC = -I ..
CXXFLAGS = -g -O2 -Wall -DDEBUG $(INC)
CFLAGS = -g -O2 -Wall -DDEBUG

TARGET= libtest_functions.a \
	libcgrouptest01 \
	libcg_ba \
	setuid \
	pathtest

all:	$(TARGET)

test_functions.o: test_functions.c
	$(CXX) $(CXXFLAGS) -c $<

libtest_functions.a: test_functions.o
	$(AR) -cr $@ $^

libcgrouptest01: test_functions.o libcgrouptest01.c
	$(CXX) $(CXXFLAGS) -o $@ libcgrouptest01.c $(LDFLAGS) $(LIBS)

libcg_ba: libcg_ba.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

setuid: setuid.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

pathtest: pathtest.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	\rm -f $(TARGET) test_functions.o
