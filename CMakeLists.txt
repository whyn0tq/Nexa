cmake_minimum_required(VERSION 3.24)

project(Nexa VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)

function(nexa_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    endif()
endfunction()

add_library(nexa_core STATIC
    src/core/types.h
    src/core/format.h
    src/core/format.cpp
    src/core/transferplan.h
    src/core/connection.h
    src/core/connection.cpp
    src/core/sftpchannel.h
    src/core/sftpchannel.cpp
    src/core/sftpconnection.h
    src/core/sftpconnection.cpp
    src/core/sftptransfer.cpp
    src/core/ftpconnection.h
    src/core/ftpconnection.cpp
    src/core/transferengine.h
    src/core/transferengine.cpp
    src/core/browseworker.h
    src/core/browseworker.cpp
    src/core/session.h
    src/core/session.cpp
    src/core/hoststore.h
    src/core/hoststore.cpp
    src/core/listingcache.h
    src/core/listingcache.cpp
)
target_include_directories(nexa_core PUBLIC src/core)
target_link_libraries(nexa_core PUBLIC Qt6::Core Qt6::Network PkgConfig::LIBSSH)
if(WIN32)
    target_link_libraries(nexa_core PUBLIC ws2_32 crypt32)
endif()
nexa_warnings(nexa_core)

if(EXISTS ${CMAKE_SOURCE_DIR}/src/ui/mainwindow.cpp)
    file(GLOB NEXA_UI_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/ui/*.h
        ${CMAKE_SOURCE_DIR}/src/ui/*.cpp)
    qt_add_executable(Nexa WIN32
        src/main.cpp
        ${NEXA_UI_SOURCES}
    )
    if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/assets/nexa.ico)
        configure_file(assets/nexa.rc.in ${CMAKE_CURRENT_BINARY_DIR}/nexa.rc @ONLY)
        target_sources(Nexa PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/nexa.rc)
    endif()
    target_include_directories(Nexa PRIVATE src/ui)
    target_link_libraries(Nexa PRIVATE nexa_core Qt6::Gui Qt6::Widgets)
    if(WIN32)
        target_link_libraries(Nexa PRIVATE dwmapi shell32 ole32 uxtheme)
    endif()
    nexa_warnings(Nexa)

    install(TARGETS Nexa
        RUNTIME DESTINATION .
        BUNDLE DESTINATION .
    )
endif()

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(NexaTransferPlanTest tests/transferplan_test.cpp)
    target_link_libraries(NexaTransferPlanTest PRIVATE nexa_core)
    add_test(NAME transfer_plan COMMAND NexaTransferPlanTest)

    qt_add_executable(NexaSftpCodecTest tests/sftpcodec_test.cpp)
    target_link_libraries(NexaSftpCodecTest PRIVATE nexa_core)
    add_test(NAME sftp_codec COMMAND NexaSftpCodecTest)
endif()

if(BUILD_TESTING)
    qt_add_executable(NexaTransferBench tests/transfer_bench.cpp)
    target_link_libraries(NexaTransferBench PRIVATE nexa_core)
endif()

if(BUILD_TESTING)
    qt_add_executable(NexaConnectBench tests/connect_bench.cpp)
    target_link_libraries(NexaConnectBench PRIVATE nexa_core)
endif()
