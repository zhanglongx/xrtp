cmake_minimum_required (VERSION 3.8)

project(xrtp)

if(WIN32)
    add_library(npcap INTERFACE)
    add_library(wpcap STATIC IMPORTED)
    add_library(packet STATIC IMPORTED)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set_property(TARGET wpcap PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/npcap/Lib/x64/wpcap.lib)
        set_property(TARGET packet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/npcap/Lib/x64/Packet.lib)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|aarch64|AARCH64")
        set_property(TARGET wpcap PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/npcap/Lib/wpcap.lib)
        set_property(TARGET packet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/npcap/Lib/Packet.lib)
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()

    target_link_libraries(npcap INTERFACE wpcap packet)
    target_include_directories(npcap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/npcap/Include)
endif()

set(src_common 
        src/common/xrtp_printf.c
        src/common/xrtp_printf.h
)

if (WIN32)
    set(src_common ${src_common} 
            src/common/getopt.c
            src/common/getopt.h
            src/common/datatype.h
    )
endif()

set(src_xrtp 
        src/bitstream.c
        src/bitstream.h
        src/payload.c
        src/payload.h
        src/pcap_interface.c
        src/pcap_interface.h
        src/rtp.c
        src/rtp.h
        src/sdp.h
        src/session.c
        src/session.h
        src/xrtp.c
        src/xrtp.h
)

add_executable(xrtp ${src_common} ${src_xrtp})

if (WIN32)
    add_compile_definitions(xrtp _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(xrtp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                                        ${CMAKE_CURRENT_SOURCE_DIR}/src/common)

if (WIN32)
    target_link_libraries(xrtp PRIVATE npcap)
    
    target_link_libraries(xrtp PRIVATE ws2_32.lib)
else()
endif()
