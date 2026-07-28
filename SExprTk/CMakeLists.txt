cmake_minimum_required(VERSION 3.20)
project(SExprTk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(SExprTk INTERFACE)
target_include_directories(SExprTk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)
install(TARGETS SExprTk
    EXPORT SExprTkTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT SExprTkTargets
    FILE SExprTkTargets.cmake
    NAMESPACE sexprtk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SExprTk
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SExprTkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SExprTkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SExprTk
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SExprTkConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SExprTkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SExprTkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SExprTk
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SExprTk.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/SExprTk.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SExprTk.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

include(CTest)
if(BUILD_TESTING)
    add_executable(SExprTk_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Catch2/extras/catch_amalgamated.cpp
        tests/test_sexprtk.cpp
    )
    target_include_directories(SExprTk_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Catch2/extras
    )
    target_link_libraries(SExprTk_tests PRIVATE SExprTk)
    add_test(NAME SExprTk_tests COMMAND SExprTk_tests)
endif()

add_executable(SExprTk_example_data_exchange examples/DataExchange/main.cpp)
target_link_libraries(SExprTk_example_data_exchange PRIVATE SExprTk)

add_executable(SExprTk_example_pikolisp examples/PikoLisp/main.cpp)
target_link_libraries(SExprTk_example_pikolisp PRIVATE SExprTk)

add_executable(SExprTk_example_xas_datagram examples/XASDatagram/main.cpp)
target_link_libraries(SExprTk_example_xas_datagram PRIVATE SExprTk)

add_executable(SExprTk_example_kernels examples/kernels/main.cpp)
target_link_libraries(SExprTk_example_kernels PRIVATE SExprTk)

install(TARGETS
    SExprTk_example_data_exchange
    SExprTk_example_pikolisp
    SExprTk_example_xas_datagram
    SExprTk_example_kernels
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/SExprTk-examples
)
