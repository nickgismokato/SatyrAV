// (1.8.1) doctest entry point. Kept in its own translation unit so the
// (large) doctest implementation is compiled once instead of per test
// file — every other file includes only the header.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
