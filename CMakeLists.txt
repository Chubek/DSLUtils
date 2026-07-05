cmake_minimum_required(VERSION 3.24)

project(
    MetaTk
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# ------------------------------------------------------------
# Third party
# ------------------------------------------------------------

add_subdirectory(third_party/Catch2 EXCLUDE_FROM_ALL)

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/SerdeTk/CMakeLists.txt")
    add_subdirectory(third_party/SerdeTk EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/QaMRpp/CMakeLists.txt")
    add_subdirectory(third_party/QaMRpp EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/Klyspec/CMakeLists.txt")
    add_subdirectory(third_party/Klyspec EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/PikoRL/CMakeLists.txt")
    add_subdirectory(third_party/PikoRL EXCLUDE_FROM_ALL)
endif()

# ------------------------------------------------------------
# Libraries
# ------------------------------------------------------------

add_subdirectory(DSLtk)
add_subdirectory(Parzek)
add_subdirectory(EkippX)