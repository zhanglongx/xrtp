# Visual Studio
cmake_minimum_required (VERSION 3.8)

project(XRTP)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

find_package(PCAP REQUIRED)

set(src_common src/common/xrtp_printf.c)
set(src_ca src/xrtpca.c src/pcap_interface.c)

add_executable(cli ${src_common} ${src_ca})

target_include_directories(cli PUBLIC ${PCAP_INCLUDE_DIRS})
target_include_directories(cli PUBLIC src)
target_link_libraries(cli ${PCAP_LIBRARIES})
