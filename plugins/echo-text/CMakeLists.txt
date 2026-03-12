cmake_minimum_required(VERSION 3.10)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    link_libraries(pthread)
endif()

add_library(echotext SHARED
    ${PROJECT_SOURCE_DIR}/plugins/echo-text/EchoText.cpp
)

if(UNIX)
    set_target_properties(echotext PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(MINGW)
    target_link_libraries(echotext PRIVATE "-static -static-libgcc -static-libstdc++ -lpthread")
elseif(WIN32)
    # MSVC does not use GCC-style static flags.
else()
    find_package(Threads REQUIRED)
    target_link_libraries(echotext PRIVATE Threads::Threads)
endif()

target_compile_definitions(echotext PRIVATE ECHOTEXT_EXPORTS)
target_include_directories(echotext PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/interface)
