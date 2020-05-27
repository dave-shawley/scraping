cmake_minimum_required(VERSION 3.10)
project(scrape)
add_executable(scrape scrape.cpp)
target_compile_features(scrape PUBLIC cxx_std_11)

find_library(LEXBOR_LIBRARY NAMES lexbor)
find_path(LEXBOR_INCLUDE_PATH NAMES lexbor/core/core.h)

find_package(Boost REQUIRED COMPONENTS log program_options)
find_package(CURL REQUIRED)

target_include_directories(scrape PRIVATE "${LEXBOR_INCLUDE_PATH}" "${CURL_INCLUDE_DIRS}" "${Boost_INCLUDE_DIRS}")
target_link_libraries(scrape PRIVATE "${LEXBOR_LIBRARY}" "${CURL_LIBRARIES}" "${Boost_LIBRARIES}")
