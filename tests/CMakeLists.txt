# Test Configuration
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, downloading...")
    include(FetchContent)
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
endif()

# Test executable
add_executable(trading_engine_tests
    test_order.cpp
    test_orderbook.cpp
    test_trading_engine_server.cpp
)

target_link_libraries(trading_engine_tests
    PRIVATE
        trading_engine
        trading_proto
        GTest::gtest
        GTest::gtest_main
        ${PROTOBUF_LIBRARIES}
        ${GRPC_LIBRARIES}
)

target_include_directories(trading_engine_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(trading_engine_tests)

# Add custom test targets
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS trading_engine_tests
    COMMENT "Running all tests"
)
