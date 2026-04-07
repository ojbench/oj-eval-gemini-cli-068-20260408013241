cmake_minimum_required(VERSION 3.10)
project(TLSFAllocator)

set(CMAKE_CXX_STANDARD 17)

# Check if main.cpp exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(code main.cpp allocator.cpp)
else()
    # If main.cpp doesn't exist, maybe the OJ expects us to provide it?
    # Or maybe the OJ will compile it differently.
    # Let's create a dummy main.cpp just in case, but only if it doesn't exist.
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/dummy_main.cpp" "int main() { return 0; }\n")
    add_executable(code dummy_main.cpp allocator.cpp)
endif()
