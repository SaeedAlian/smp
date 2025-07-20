cmake_minimum_required(VERSION 3.14)
project(MusicPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# Source files
set(SRC_FILES
    src/main.cpp
    src/db.cpp
    src/library.cpp
    src/common/utils.cpp
)

# Executable
add_executable(musicplayer ${SRC_FILES})

# Link libraries
target_link_libraries(musicplayer PRIVATE ncurses fmt sqlite3 tag)

# -----------------------------------
# Unit Tests
# -----------------------------------
enable_testing()

# Test files
set(TEST_FILES
    test/library_test.cpp
    src/db.cpp
    src/library.cpp
    src/common/utils.cpp
)

add_executable(musicplayer_test ${TEST_FILES})
target_include_directories(musicplayer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(musicplayer_test PRIVATE gtest gtest_main pthread fmt sqlite3 tag)

add_test(NAME LibraryTest COMMAND musicplayer_test)
