cmake_minimum_required(VERSION 3.16)
project(r_cal_II LANGUAGES CXX)

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

add_executable(r-cal
    main.cpp
    src/RCalHeaderFramework.cpp
    include/RCalHeaderFramework.h
)
target_compile_features(r-cal PRIVATE cxx_std_17)
set_target_properties(r-cal PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(r-cal PRIVATE include ${CURSES_INCLUDE_DIRS})
target_link_libraries(r-cal PRIVATE ${CURSES_LIBRARIES})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(r-cal PRIVATE -Wall -Wextra)
endif()
