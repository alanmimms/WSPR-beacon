# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(beacon)

# This is the command that handles the web assets.
# It finds the partition named "storage" in your partitions.csv,
# takes all the files from the "web" directory, and creates a
# binary SPIFFS image to be flashed to that partition.
spiffs_create_partition_image(storage web FLASH_IN_PROJECT)
