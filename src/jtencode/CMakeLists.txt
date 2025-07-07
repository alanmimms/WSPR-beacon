# jtencode library - WSPR/JT encoder library
# Works with both ESP-IDF and regular CMake builds

if(DEFINED ESP_PLATFORM)
    # ESP-IDF Component build
    idf_component_register(
        SRCS 
            "JTEncode.cpp"
            "RSEncode.cpp" 
            "jtencode-util.cpp"
            "nhash.c"
        INCLUDE_DIRS 
            "include"
        REQUIRES
            # No special ESP-IDF components required
    )
    
    # Set C++ standard for this component
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
else()
    # Regular CMake build (host-mock)
    cmake_minimum_required(VERSION 3.16)
    
    # Create the library
    add_library(jtencode
        JTEncode.cpp
        RSEncode.cpp
        jtencode-util.cpp
        nhash.c
    )
    
    # Set include directories
    target_include_directories(jtencode PUBLIC include)
    
    # Set C++ standard
    target_compile_features(jtencode PUBLIC cxx_std_17)
endif()