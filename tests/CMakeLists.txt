add_executable(test_c test_c.c)
target_include_directories(test_c PUBLIC ../include)
target_link_libraries(test_c PRIVATE astraos_kernel)
add_test(NAME test_c COMMAND test_c)

add_executable(test_mm test_mm.cpp)
target_include_directories(test_mm PUBLIC ../include)
target_link_libraries(test_mm PRIVATE astraos_kernel)
add_test(NAME test_mm COMMAND test_mm)

