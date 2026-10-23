CXX      ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
RAYLIB   := $(shell pkg-config --cflags --libs raylib)
FRAMEWORKS := -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL

LIB_SRC := rm/raymanim.cpp markup/viz/viz.cpp markup/viz/space.cpp
PLACE_SRC := $(wildcard markup/place/*.cpp)
VIZ_SRC := $(wildcard markup/viz/*.cpp)
MK_SRC  := $(PLACE_SRC) markup/placement.cpp markup/annotate.cpp markup/area.cpp markup/controls.cpp markup/camera.cpp markup/fields.cpp markup/trails.cpp markup/binding.cpp markup/expr.cpp markup/script.cpp markup/engine.cpp markup/layout.cpp markup/space.cpp markup/render.cpp markup/tts.cpp markup/tex.cpp

all: tutor manim_demo

tutor: main.cpp $(LIB_SRC) $(MK_SRC) rm/raymanim.hpp markup/*.hpp markup/place/*.hpp markup/viz/*.hpp
	$(CXX) $(CXXFLAGS) main.cpp $(LIB_SRC) $(MK_SRC) -o $@ $(RAYLIB) $(FRAMEWORKS) -lcurl

manim_demo: examples/manim_demo.cpp $(LIB_SRC) rm/raymanim.hpp
	$(CXX) $(CXXFLAGS) examples/manim_demo.cpp $(LIB_SRC) -o $@ $(RAYLIB) $(FRAMEWORKS)

check: tutor
	@for f in scripts/*.dan chat/examples/*.dan chat/idioms/*.dan; do printf "%-36s " $$f; ./tutor $$f --check > /dev/null 2>&1 && echo ok || echo FAIL; done
	@node tests/toolbox.mjs

# the placement module on its own: no raylib on the include path, so it can't depend on the engine
TEST_FLAGS := -std=c++20 -O1 -g -Wall -Wextra -Wno-unused-parameter -Ithird_party
tests/test_place: tests/test_place.cpp $(PLACE_SRC) markup/place/*.hpp
	$(CXX) $(TEST_FLAGS) tests/test_place.cpp $(PLACE_SRC) -o $@

# the camera / field / trail maths, likewise without raylib
tests/test_viz: tests/test_viz.cpp $(VIZ_SRC) markup/viz/*.hpp markup/expr.cpp markup/expr.hpp
	$(CXX) $(TEST_FLAGS) tests/test_viz.cpp $(VIZ_SRC) markup/expr.cpp -o $@

tests/test_script: tests/test_script.cpp markup/script.cpp markup/script.hpp markup/expr.cpp markup/expr.hpp
	$(CXX) $(TEST_FLAGS) tests/test_script.cpp markup/script.cpp markup/expr.cpp -o $@

tests/test_engine: tests/test_engine.cpp $(LIB_SRC) $(MK_SRC) rm/raymanim.hpp markup/*.hpp markup/place/*.hpp markup/viz/*.hpp
	$(CXX) $(TEST_FLAGS) tests/test_engine.cpp $(LIB_SRC) $(MK_SRC) -o $@ $(RAYLIB) $(FRAMEWORKS) -lcurl

test: tests/test_place tests/test_viz tests/test_script tests/test_engine
	./tests/test_place
	./tests/test_viz
	./tests/test_script
	./tests/test_engine
	node tests/compute.mjs

tests/bench_place: tests/bench_place.cpp $(PLACE_SRC) markup/place/*.hpp
	$(CXX) -std=c++20 -O2 -Ithird_party tests/bench_place.cpp $(PLACE_SRC) -o $@

bench: tests/bench_place
	./tests/bench_place

tests/test_fonts: tests/test_fonts.cpp $(LIB_SRC) rm/raymanim.hpp
	$(CXX) $(CXXFLAGS) tests/test_fonts.cpp $(LIB_SRC) -o $@ $(RAYLIB) $(FRAMEWORKS)

test-fonts: tests/test_fonts
	./tests/test_fonts

# layout regression over every script and exemplar (node tests/golden.mjs --save re-baselines)
golden: tutor
	node tests/golden.mjs
	node tests/golden.mjs --placer old

# streamed (chunked --stdin) playback ends exactly like whole-script playback
append: tutor
	./tests/append.sh

tex: tools/tex/node_modules

tools/tex/node_modules: tools/tex/package.json
	cd tools/tex && npm install --no-audit --no-fund

clean:
	rm -f tutor manim_demo tests/test_place tests/test_viz tests/test_script tests/test_engine tests/test_fonts tests/bench_place

.PHONY: all check clean tex test test-fonts golden bench append
