# CMakeLists.txt for the tinyfsm component
# This makes the header-only library available to other components.

idf_component_register(
    # This component has no source files, only headers.
    # The INCLUDE_DIRS line makes the 'include' subdirectory
    # available to any component that requires 'tinyfsm'.
    INCLUDE_DIRS "include"
)
