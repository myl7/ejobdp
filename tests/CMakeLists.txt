include(CTest)
set(CMAKE_CXX_STANDARD 17)

add_executable(test_dispatch test_dispatch.cpp)
target_link_libraries(test_dispatch PRIVATE ejobdp)
add_test(NAME test_dispatch COMMAND test_dispatch)
