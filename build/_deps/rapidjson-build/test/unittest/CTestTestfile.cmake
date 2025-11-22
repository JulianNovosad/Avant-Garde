# CMake generated Testfile for 
# Source directory: /home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-src/test/unittest
# Build directory: /home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-build/test/unittest
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unittest "/home/pc/Avant-Garde/Avant-Garde-1/build/bin/unittest")
set_tests_properties(unittest PROPERTIES  WORKING_DIRECTORY "/home/pc/Avant-Garde/Avant-Garde-1/bin" _BACKTRACE_TRIPLES "/home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;79;add_test;/home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;0;")
add_test(valgrind_unittest "valgrind" "--suppressions=/home/pc/Avant-Garde/Avant-Garde-1/test/valgrind.supp" "--leak-check=full" "--error-exitcode=1" "/home/pc/Avant-Garde/Avant-Garde-1/build/bin/unittest" "--gtest_filter=-SIMD.*")
set_tests_properties(valgrind_unittest PROPERTIES  WORKING_DIRECTORY "/home/pc/Avant-Garde/Avant-Garde-1/bin" _BACKTRACE_TRIPLES "/home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;85;add_test;/home/pc/Avant-Garde/Avant-Garde-1/build/_deps/rapidjson-src/test/unittest/CMakeLists.txt;0;")
