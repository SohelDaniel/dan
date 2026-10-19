// place: the placement module. Pure C++ (no raylib, no engine), unit-tested in tests/test_place.cpp.
//   solveBodies  relations + regions + proposals -> positions (kiwi), then overlap removal (separate)
//   placeLabels  names and values beside their things, map-style, with hysteresis
//   Skyline      stacking annotations along a row
//   Spring       smooth, stable motion to the solved positions
//   check        the exact layout checker and its messages
#pragma once
#include "check.hpp"
#include "labels.hpp"
#include "regions.hpp"
#include "skyline.hpp"
#include "solve.hpp"
#include "spring.hpp"
