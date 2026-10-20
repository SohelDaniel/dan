// place: the placement module. Pure C++ (no raylib, no engine), unit-tested in tests/test_place.cpp.
//   layoutFlow   the graph schema: things in columns by their links, lanes by part-of, routed links
//   placeLabels  names and values beside their things, map-style, with hysteresis
//   solve1d      1-D least squares under separation constraints
//   Spring       smooth, stable motion to the solved positions
//   check        the exact layout checker and its messages
#pragma once
#include "check.hpp"
#include "labels.hpp"
#include "qp.hpp"
#include "spring.hpp"
