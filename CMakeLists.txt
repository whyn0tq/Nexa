cmake_minimum_required(VERSION 3.24)

project(Nexa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)

qt_add_executable(Nexa WIN32
    src/main.cpp
    src/types.h
    src/connectdialog.h
    src/connectdialog.cpp
    src/remotefilemodel.h
    src/remotefilemodel.cpp
    src/fileviews.h
    src/fileviews.cpp
    src/sftpworker.h
    src/sftpworker.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)

target_link_libraries(Nexa PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    PkgConfig::LIBSSH
)

target_compile_definitions(Nexa PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

if(MSVC)
    target_compile_options(Nexa PRIVATE /W4 /permissive-)
else()
    target_compile_options(Nexa PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
    )
endif()

if(WIN32)
    target_link_libraries(Nexa PRIVATE ws2_32 shell32 ole32 crypt32)
endif()

install(TARGETS Nexa
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(NexaDragDropTest
        tests/dragdrop_smoke.cpp
        src/types.h
        src/remotefilemodel.h
        src/remotefilemodel.cpp
        src/fileviews.h
        src/fileviews.cpp
    )
    target_include_directories(NexaDragDropTest PRIVATE src)
    target_link_libraries(NexaDragDropTest PRIVATE
        Qt6::Core Qt6::Widgets
    )
    if(MSVC)
        target_compile_options(NexaDragDropTest PRIVATE /W4 /permissive-)
    else()
        target_compile_options(NexaDragDropTest PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
        )
    endif()
    if(WIN32)
        target_link_libraries(NexaDragDropTest PRIVATE shell32 ole32)
    endif()
    add_test(NAME dragdrop_smoke COMMAND NexaDragDropTest)
    set_tests_properties(dragdrop_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
