cmake_minimum_required(VERSION 3.22.1)

project(opengothic)

# Path to the main OpenGothic project (parent of android folder)
set(OPENGOTHIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

# Include the main project's CMakeLists.txt
# The main CMakeLists.txt already handles ANDROID builds and creates a shared library
add_subdirectory(${OPENGOTHIC_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/opengothic)

# The main project creates Gothic2Notr target, we need to rename it to opengothic
# for Android to find it as libopengothic.so
set_target_properties(Gothic2Notr PROPERTIES OUTPUT_NAME "opengothic")
