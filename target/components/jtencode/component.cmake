# This file contains the build logic for the ESP-IDF component.
# It is only included when IDF_TARGET is defined.

idf_component_register(SRCS "JTEncode.cpp"
                      INCLUDE_DIRS "include")
