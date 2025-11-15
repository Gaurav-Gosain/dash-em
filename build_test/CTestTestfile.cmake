# CMake generated Testfile for 
# Source directory: /home/gaurav/dev/dash-em
# Build directory: /home/gaurav/dev/dash-em/build_test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(DashemTests "/home/gaurav/dev/dash-em/build_test/test_dashem")
set_tests_properties(DashemTests PROPERTIES  _BACKTRACE_TRIPLES "/home/gaurav/dev/dash-em/CMakeLists.txt;194;add_test;/home/gaurav/dev/dash-em/CMakeLists.txt;0;")
add_test(ComprehensiveTests "/home/gaurav/dev/dash-em/build_test/comprehensive_tests")
set_tests_properties(ComprehensiveTests PROPERTIES  _BACKTRACE_TRIPLES "/home/gaurav/dev/dash-em/CMakeLists.txt;201;add_test;/home/gaurav/dev/dash-em/CMakeLists.txt;0;")
add_test(FuzzingTests "/home/gaurav/dev/dash-em/build_test/fuzzer")
set_tests_properties(FuzzingTests PROPERTIES  _BACKTRACE_TRIPLES "/home/gaurav/dev/dash-em/CMakeLists.txt;208;add_test;/home/gaurav/dev/dash-em/CMakeLists.txt;0;")
