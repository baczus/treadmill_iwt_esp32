#!/bin/sh
# Host-side logic tests: compile the sketch against Arduino stubs and run.
cd "$(dirname "$0")/.." || exit 1
g++ -std=gnu++17 -Wall -Itest/stubs -I. -o /tmp/opencode/treadmill_tests \
    test/sketch.cpp test/test_main.cpp buttons.cpp settings.cpp display.cpp \
    test/stubs/fake.cpp test/stubs/wire.cpp || exit 1
/tmp/opencode/treadmill_tests
