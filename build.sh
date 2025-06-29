#!/bin/bash

# Trading Engine Build Script
# Usage: ./build.sh [debug|release|clean|help]

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project settings
PROJECT_NAME="TradingEngineServer"
BUILD_DIR="build"
SOURCE_DIR="."

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check for required tools
    local missing_deps=()
    
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v make &> /dev/null && ! command -v ninja &> /dev/null; then
        missing_deps+=("make or ninja")
    fi
    
    if ! command -v protoc &> /dev/null; then
        missing_deps+=("protobuf-compiler")
    fi
    
    if ! command -v grpc_cpp_plugin &> /dev/null; then
        missing_deps+=("grpc-tools")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        echo
        echo "Install them with:"
        echo "  Ubuntu/Debian: sudo apt install cmake build-essential protobuf-compiler libprotobuf-dev libgrpc++-dev protobuf-compiler-grpc"
        echo "  macOS: brew install cmake protobuf grpc"
        echo "  Arch Linux: sudo pacman -S cmake protobuf grpc"
        exit 1
    fi
    
    print_success "All dependencies found"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_warning "Build directory doesn't exist"
    fi
}

# Function to create build directory
create_build_dir() {
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        print_status "Created build directory"
    fi
}

# Function to configure cmake
configure_cmake() {
    local build_type=$1
    local extra_flags=$2
    
    print_status "Configuring CMake for $build_type build..."
    
    cd "$BUILD_DIR"
    
    cmake .. \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        $extra_flags
    
    cd ..
    
    print_success "CMake configuration complete"
}

# Function to build project
build_project() {
    local jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    print_status "Building project with $jobs parallel jobs..."
    
    cd "$BUILD_DIR"
    make -j"$jobs"
    cd ..
    
    print_success "Build completed successfully"
}

# Function to run the server
run_server() {
    if [ -f "$BUILD_DIR/bin/trading_server" ]; then
        print_status "Starting trading server..."
        "$BUILD_DIR/bin/trading_server"
    else
        print_error "Trading server executable not found. Please build first."
        exit 1
    fi
}

# Function to show build information
show_info() {
    print_status "Build Information:"
    echo "  Project: $PROJECT_NAME"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Source Directory: $SOURCE_DIR"
    echo
    
    if [ -f "$BUILD_DIR/bin/trading_server" ]; then
        echo "  Executable: $BUILD_DIR/bin/trading_server"
        echo "  Size: $(du -h "$BUILD_DIR/bin/trading_server" | cut -f1)"
        echo "  Modified: $(stat -c %y "$BUILD_DIR/bin/trading_server" 2>/dev/null || stat -f %Sm "$BUILD_DIR/bin/trading_server" 2>/dev/null)"
    else
        echo "  Executable: Not built yet"
    fi
}

# Function to show help
show_help() {
    echo "Trading Engine Build Script"
    echo
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo
    echo "Commands:"
    echo "  debug     Build debug version with sanitizers"
    echo "  release   Build optimized release version (default)"
    echo "  clean     Clean build directory"
    echo "  run       Build and run the server"
    echo "  info      Show build information"
    echo "  help      Show this help message"
    echo
    echo "Examples:"
    echo "  $0                # Build release version"
    echo "  $0 debug          # Build debug version"
    echo "  $0 clean release  # Clean and build release"
    echo "  $0 run            # Build and run server"
}

# Main execution
main() {
    local command=${1:-release}
    
    # Handle clean command first
    if [[ "$*" == *"clean"* ]]; then
        clean_build
        # Remove clean from arguments
        set -- "${@/clean/}"
        command=${1:-release}
    fi
    
    case "$command" in
        debug)
            check_dependencies
            create_build_dir
            configure_cmake "Debug" "-DENABLE_TESTING=ON"
            build_project
            show_info
            ;;
        release)
            check_dependencies
            create_build_dir
            configure_cmake "Release" ""
            build_project
            show_info
            ;;
        run)
            if [ ! -f "$BUILD_DIR/bin/trading_server" ]; then
                print_warning "Server not built. Building release version first..."
                check_dependencies
                create_build_dir
                configure_cmake "Release" ""
                build_project
            fi
            run_server
            ;;
        info)
            show_info
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $command"
            echo
            show_help
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
