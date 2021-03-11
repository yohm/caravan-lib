cmake_minimum_required(VERSION 3.15)
project(CARAVAN_LIB)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

set(SOURCE_FILES caravan.hpp Logger.hpp Base.hpp TaskResult.hpp Producer.hpp Buffer.hpp Consumer.hpp)
find_package(MPI)

add_executable(test_caravan.out test_caravan.cpp ${SOURCE_FILES})
target_link_libraries(test_caravan.out ${MPI_LIBRARIES})
target_include_directories(test_caravan.out
        PUBLIC ${MPI_INCLUDE_PATH} ${CMAKE_SOURCE_DIR}/json/include ${CMAKE_SOURCE_DIR}/icecream)
