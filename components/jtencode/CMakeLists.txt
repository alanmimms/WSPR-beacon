# CMakeLists.txt for the jtencode ESP-IDF component

# Define all the source files for this component.
set(COMPONENT_SRCS
    "JTEncode.cpp"
    "RSEncode.cpp"
    "jtencode-util.cpp"
    "nhash.c"
)

# Define the public include directory.
# This makes the "include" subdirectory visible to other components.
set(COMPONENT_INCLUDE_DIRS "include")

# Register the component with the ESP-IDF build system.
idf_component_register(
    SRCS "${COMPONENT_SRCS}"
    INCLUDE_DIRS "${COMPONENT_INCLUDE_DIRS}"
    # This component requires the 'json' component for its own implementation.
    REQUIRES json
)
