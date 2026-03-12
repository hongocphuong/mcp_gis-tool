cmake_minimum_required(VERSION 3.10)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    link_libraries(pthread)
endif()

add_library(gistool SHARED
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool/GisTool.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool/ToolRegistry.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool/geojson/GeoJsonTraversal.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool/tools/Wgs84Tools.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool/tools/GeoJsonTools.cpp
)

if(UNIX)
    set_target_properties(gistool PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(MINGW)
    target_link_libraries(gistool PRIVATE "-static -static-libgcc -static-libstdc++ -lpthread")
elseif(WIN32)
    # MSVC does not use GCC-style static flags.
else()
    find_package(Threads REQUIRED)
    target_link_libraries(gistool PRIVATE Threads::Threads)
endif()

target_compile_definitions(gistool PRIVATE GISTOOL_EXPORTS)
target_include_directories(gistool PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/interface
    ${PROJECT_SOURCE_DIR}/plugins/gis-tool
)