CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

project(kcp LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -std=c++11")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O0 -Wall -std=c++11")

 

add_library(kcp STATIC ikcp.c)

install(FILES ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

if (BUILD_TESTING)
    enable_language(CXX)
    
    add_executable(client client.c ikcp.c delay.c)
    target_link_libraries(client pthread)
    add_executable(server server.c ikcp.c)
    add_executable(test_kcp test.cpp ikcp.c)
    add_executable(chat_server chat_server.cc kcp_server.cc kcp_session.cc udp_socket.cc ikcp.c)
    target_link_libraries(chat_server pthread)
    add_executable(chat_client chat_client.cc kcp_client.cc kcp_session.cc udp_socket.cc ikcp.c)
    target_link_libraries(chat_client pthread)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif ()
