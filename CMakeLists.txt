cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Piper C CXX)

# Include the library directory
add_subdirectory(libpiper)

# Include the app directory
add_subdirectory(app)
