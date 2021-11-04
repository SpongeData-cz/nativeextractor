project = NativeExtractor
project_libname = nativeextractor
flags = -std=c11 -D_GNU_SOURCE -pthread
links = glib-2.0
test_dir = build/tests
miners_install_dir = /usr/lib/nativeextractor_miners

# Use config=release for release builds
ifeq ($(config), release)
dir = build/release
flags := $(flags) -O4 -march=native
else
dir = build/debug
flags := $(flags) -O0 -g3 -DDEBUG
endif

ifeq ($(install), true)
flags := $(flags) -DNE_PRODUCTION
endif

.PHONY: clean
clean:
	-rm *.o
	-rm -f $(dir)/lib$(project).so $(dir)/$(project)_example
	-rm -rf $(dir)/lib/

.PHONY: build
build:
	-mkdir -p $(dir)
	$(CC) $(flags) -fPIC -Iinclude -rdynamic \
		`pkg-config --cflags $(links)` \
		-c src/*.c
	$(CC) -shared -o $(dir)/lib$(project_libname).so *.o

.PHONY: examples
examples:
	-mkdir -p $(dir)
	make example name=glob
	make example name=ngrep
	make example name=naive_email_miner
	# naive email miner as .so module
	$(CC) $(flags) -DSO_MODULE `pkg-config --cflags $(links)` \
		`pkg-config --libs $(links)` \
		-Iinclude/ -shared -o $(dir)/naive_email_miner.so \
		-fPIC src/example/naive_email_miner.c

.PHONY: miner
miner:
	-mkdir -p $(dir)/lib/
	$(CC) $(flags) `pkg-config --cflags $(links)` \
	`pkg-config --libs $(links)` \
	-Iinclude/ -shared -o $(dir)/lib/$(name).so \
	-fPIC src/miners/$(name).c

.PHONY: example
example:
	$(CC) $(flags) -Iinclude -rdynamic \
	`pkg-config --cflags $(links)` \
	src/example/$(name).c \
	-L$(dir) -l$(project_libname) \
	`pkg-config --libs $(links)` -ldl \
	-o $(dir)/$(name)


.PHONY: all-miners
all-miners:
	for f in src/miners/*.c; do \
		make miner name=`basename $$f .c` config=$(config); \
	done

.PHONY: install
install:
	make config=release install=true build
	make config=release install=true all-miners
	rm -rf /usr/include/$(project_libname)/
	mkdir -p /usr/include/$(project_libname)/
	cp -rf ./include/$(project_libname)/ /usr/include/$(project_libname)/
	mkdir -p /usr/lib/
	rm -f /usr/lib/$(project_libname).so
	cp build/release/lib$(project_libname).so /usr/lib/lib$(project_libname).so
	mkdir -p $(miners_install_dir)
	cp build/release/lib/* $(miners_install_dir)
	ldconfig
	mkdir -p /usr/lib/pkgconfig/
	cp nativeextractor.pc /usr/lib/pkgconfig/
	rm *.o
.PHONY: test
test:
	-mkdir -p $(test_dir)
	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/extractor.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_extractor \

	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/patricia.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_patricia \

	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/glob.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_glob \

	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/finite_automaton.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_finite_automaton \

	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/regex_miner.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_regex_miner \

	$(CC) $(flags) -Iinclude -rdynamic \
		`find ./src/ -maxdepth 1 -type f ! -name "main.c" -name "*.c"` tests/enclosed.c \
		`pkg-config --cflags $(links)` \
		`pkg-config --cflags --libs cmocka` \
		`pkg-config --libs $(links)` -ldl \
		-o $(test_dir)/$(project)_enclosed \

.PHONY: default
default: all-miners build

.PHONY: doc
doc:
	-cd ./doc && git clone https://github.com/MaJerle/doxygen-dark-theme.git
	doxygen ./doc/Doxyfile

.DEFAULT_GOAL := default
