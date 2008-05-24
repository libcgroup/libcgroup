CXXFLAGS = -g -O2 -Wall -DDEBUG
LDFLAGS = -L ..
LIBS = -lcgroup

libcg_ba: libcg_ba.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	\rm libcg_ba
