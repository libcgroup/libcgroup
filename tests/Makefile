LDFLAGS = -L ..
LIBS = -lcgroup -lpthread
INC = -I ..
CXXFLAGS = -g -O2 -Wall -DDEBUG $(INC)

TARGET= libcgrouptest01 \
	libcg_ba

all:	$(TARGET)

libcgrouptest01: libcgrouptest01.c
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

libcg_ba: libcg_ba.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	\rm -f $(TARGET)
