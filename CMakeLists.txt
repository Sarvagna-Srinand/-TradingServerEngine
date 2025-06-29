cmake_minimum_required(VERSION 3.16)

project(TradingEngineServer 
    VERSION 1.0.0
    DESCRIPTION "High-Performance Trading Engine Server"
    LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
    
    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined")
    
    # Common warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Find protobuf and gRPC using pkg-config for better compatibility
find_package(PkgConfig REQUIRED)

# Find protobuf
pkg_check_modules(PROTOBUF REQUIRED protobuf)
if(NOT PROTOBUF_FOUND)
    find_package(Protobuf REQUIRED)
endif()

# Find gRPC
pkg_check_modules(GRPC REQUIRED grpc++)
if(NOT GRPC_FOUND)
    find_package(gRPC REQUIRED)
endif()

# Check if we have the protobuf compiler
find_program(PROTOC_EXECUTABLE protoc)
if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "Protocol buffer compiler (protoc) not found")
endif()

# Check if we have the gRPC plugin
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "gRPC C++ plugin not found")
endif()

# Function to generate protobuf and gRPC files
function(compile_proto_files)
    set(PROTO_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/trading_optimized.proto
    )
    
    set(PROTO_SRCS)
    set(PROTO_HDRS)
    set(GRPC_SRCS)
    set(GRPC_HDRS)
    
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        
        # Protobuf files
        set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
        
        # gRPC files  
        set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
        
        list(APPEND PROTO_SRCS ${PROTO_SRC})
        list(APPEND PROTO_HDRS ${PROTO_HDR})
        list(APPEND GRPC_SRCS ${GRPC_SRC})
        list(APPEND GRPC_HDRS ${GRPC_HDR})
        
        # Generate protobuf files
        add_custom_command(
            OUTPUT ${PROTO_SRC} ${PROTO_HDR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                 --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating protobuf files for ${PROTO_NAME}"
        )
        
        # Generate gRPC files
        add_custom_command(
            OUTPUT ${GRPC_SRC} ${GRPC_HDR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                 --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                 --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
                 --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC files for ${PROTO_NAME}"
        )
    endforeach()
    
    # Set variables in parent scope
    set(PROTO_SOURCES ${PROTO_SRCS} ${GRPC_SRCS} PARENT_SCOPE)
    set(PROTO_HEADERS ${PROTO_HDRS} ${GRPC_HDRS} PARENT_SCOPE)
endfunction()

# Generate protobuf and gRPC files
compile_proto_files()

# Create protobuf library
add_library(trading_proto STATIC ${PROTO_SOURCES})

# Use pkg-config for linking
if(PROTOBUF_FOUND AND GRPC_FOUND)
    target_link_libraries(trading_proto 
        PUBLIC 
            ${PROTOBUF_LIBRARIES}
            ${GRPC_LIBRARIES}
    )
    target_include_directories(trading_proto 
        PUBLIC 
            ${CMAKE_CURRENT_BINARY_DIR}
            ${PROTOBUF_INCLUDE_DIRS}
            ${GRPC_INCLUDE_DIRS}
    )
    target_compile_options(trading_proto
        PUBLIC
            ${PROTOBUF_CFLAGS_OTHER}
            ${GRPC_CFLAGS_OTHER}
    )
else()
    target_link_libraries(trading_proto 
        PUBLIC 
            protobuf::libprotobuf
            gRPC::grpc++
    )
    target_include_directories(trading_proto 
        PUBLIC 
            ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

# Set compile features for protobuf library
target_compile_features(trading_proto PUBLIC cxx_std_20)

# Source files for the main library
set(TRADING_ENGINE_SOURCES
    Constants.cpp
    Orderbook.cpp
    TradingEngineServer.cpp
)

# Header files (for IDE organization)
set(TRADING_ENGINE_HEADERS
    Constants.hpp
    Host.hpp
    LevelInfo.hpp
    Logging.hpp
    Order.hpp
    OrderCore.hpp
    OrderModify.hpp
    OrderType.hpp
    Orderbook.hpp
    OrderbookLevelInfos.hpp
    Side.hpp
    Trade.hpp
    TradeInfo.hpp
    TradingEngineServer.hpp
    Usings.hpp
    api.hpp
)

# Create the main trading engine library
add_library(trading_engine STATIC 
    ${TRADING_ENGINE_SOURCES}
    ${TRADING_ENGINE_HEADERS}
)

# Link libraries
if(PROTOBUF_FOUND AND GRPC_FOUND)
    target_link_libraries(trading_engine
        PUBLIC
            trading_proto
            Threads::Threads
        PRIVATE
            ${PROTOBUF_LIBRARIES}
            ${GRPC_LIBRARIES}
    )
else()
    target_link_libraries(trading_engine
        PUBLIC
            trading_proto
            Threads::Threads
        PRIVATE
            protobuf::libprotobuf
            gRPC::grpc++
    )
endif()

# Include directories
target_include_directories(trading_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Set compile features
target_compile_features(trading_engine PUBLIC cxx_std_20)

# Create the executable
add_executable(trading_server main.cpp)

# Link the executable
if(PROTOBUF_FOUND AND GRPC_FOUND)
    target_link_libraries(trading_server
        PRIVATE
            trading_engine
            trading_proto
            Threads::Threads
            ${PROTOBUF_LIBRARIES}
            ${GRPC_LIBRARIES}
    )
else()
    target_link_libraries(trading_server
        PRIVATE
            trading_engine
            trading_proto
            Threads::Threads
            protobuf::libprotobuf
            gRPC::grpc++
    )
endif()

# Set executable properties
set_target_properties(trading_server PROPERTIES
    OUTPUT_NAME "trading_server"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Enable testing (optional)
option(ENABLE_TESTING "Enable unit testing" ON)
if(ENABLE_TESTING)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    else()
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
            message(STATUS "Tests directory not found. Skipping tests.")
        else()
            message(WARNING "Google Test not found. Tests will not be built.")
        endif()
    endif()
endif()

# Install targets
install(TARGETS trading_server
    RUNTIME DESTINATION bin
)

install(TARGETS trading_engine trading_proto
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${TRADING_ENGINE_HEADERS}
    DESTINATION include/trading_engine
)

# Create a compile_commands.json for development tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "gRPC plugin: ${GRPC_CPP_PLUGIN}")

# Custom targets for convenience
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
)

# Performance tuning target
add_custom_target(build-optimized
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -flto -ffast-math" ..
    COMMAND ${CMAKE_BUILD_TOOL}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building with maximum optimizations for production"
)

# Development build target
add_custom_target(build-debug
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ..
    COMMAND ${CMAKE_BUILD_TOOL}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building debug version with sanitizers"
)
