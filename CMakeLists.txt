# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name and language
project(BitTorrentClient CXX)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Manually configure local libcurl dependency ---
add_library(CURL::libcurl UNKNOWN IMPORTED)

set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/curl"
)

# Set the library location based on the operating system
if(WIN32)
    # --- THIS IS THE FIX ---
    # Point to the MinGW static library file (.a)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/curl/libcurl.a"
    )
else()
    # Path to the .a or .so file on Linux/macOS
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/curl/libcurl.a"
    )
endif()


# Add the 'lib' directory to the include path for nlohmann/json.hpp and sha1.hpp
include_directories(lib)

# Define the executable target and list all of its source files
add_executable(
    your_client
    Main.cpp
    bencode.cpp
    torrent_file.cpp
    tracker.cpp
    peer_connection.cpp
)

# Link the executable against the libraries it needs
target_link_libraries(
    your_client
    PRIVATE
    CURL::libcurl
)

# Add platform-specific libraries
if(WIN32)
    target_link_libraries(your_client PRIVATE ws2_32)
endif()

message(STATUS "Configured BitTorrentClient project. Build with 'mingw32-make'")

