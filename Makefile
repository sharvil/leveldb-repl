TARGET := leveldb
LIBS := -llinenoise -lleveldb -lpthread
CXXFLAGS := \
  -Ideps/linenoise \
  -Ideps/leveldb/include \
  -Ldeps/lib

.PHONY: all clean

all: deps/lib/libleveldb.a deps/lib/liblinenoise.a
	$(CXX) $(CXXFLAGS) src/main.cpp $(LIBS) -o $(TARGET)

clean:
	rm -fr deps $(TARGET)

deps/lib/libleveldb.a: deps/leveldb deps/lib
	make -C deps/leveldb && mv deps/leveldb/out-static/libleveldb.a deps/lib/

deps/lib/liblinenoise.a: deps/linenoise deps/lib
	$(CC) -c deps/linenoise/linenoise.c -o deps/linenoise/linenoise.o
	$(AR) -r deps/lib/liblinenoise.a deps/linenoise/linenoise.o

deps/leveldb: deps
	git clone https://github.com/google/leveldb.git deps/leveldb

deps/linenoise: deps
	git clone https://github.com/antirez/linenoise.git deps/linenoise

deps/lib: deps
	mkdir deps/lib/

deps:
	mkdir deps/
