#include "test_framework.hpp"

int main(int argc, char** argv) { return sctest::RunAll(argc > 1 ? argv[1] : nullptr); }
