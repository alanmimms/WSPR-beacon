# CMakeLists.txt for the jtencode ESP-IDF component

# Define the source files for this component.
# This assumes RSEncoder.cpp is located directly in the component directory.
set(COMPONENT_SRCS "RSEncoder.cpp")

# Define the include directories for this component.
# The current directory (where CMakeLists.txt resides) should be included
# so other components that depend on 'jtencode' can find RSEncoder.h.
set(COMPONENT_PRIV_INCLUDE_DIRS ".")

# Register the component with the ESP-IDF build system.
# idf_component_register is the primary command for defining an IDF component.
# It handles all the underlying CMake magic, including finding its own component.cmake.
#
# SRCS: Specifies the C, C++, or assembly source files to compile.
# INCLUDE_DIRS: Specifies public include directories. Any component that
#               requires this component will also have these directories added
#               to its include paths.
# PRIV_INCLUDE_DIRS: Specifies private include directories. These are only
#                    used by this component itself.
# REQUIRES / PRIV_REQUIRES: Specifies dependencies on other ESP-IDF components.
#                           For RSEncoder, it primarily uses standard C++ features
#                           and cstring, so no specific IDF component dependencies
#                           are explicitly required for its core functionality here.
idf_component_register(
    SRCS "${COMPONENT_SRCS}"
    INCLUDE_DIRS "${COMPONENT_PRIV_INCLUDE_DIRS}"
    # Example of adding a dependency if needed (uncomment and modify if applicable):
    # PRIV_REQUIRES freertos esp_heap_caps
)

# You should NOT include component.cmake or similar files directly here.
# The ESP-IDF build system handles that.
