.PHONY: all configure test clean

all: configure
	ninja -C build -j 6

configure:
	cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

test: all
	ninja -C build test

clean:
	rm -f -r build/
