cmake_minimum_required(VERSION 3.15)
project(PhylaRAMTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(test_range_algebra test_range_algebra.cpp)
add_test(NAME RangeAlgebra COMMAND test_range_algebra)

add_executable(test_mock_acquire test_mock_acquire.cpp)
add_test(NAME MockAcquire COMMAND test_mock_acquire)

add_executable(test_map_json test_map_json.cpp)
add_test(NAME MapJson COMMAND test_map_json)

add_executable(test_cli_parser test_cli_parser.cpp)
add_test(NAME CliParser COMMAND test_cli_parser)
