cmake_minimum_required(VERSION 3.20)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    link_libraries(pthread)
endif()

add_library(gistool SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/GisTool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ToolRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geojson/GeoJsonTraversal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Boolean/BooleanTools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Classification/ClassificationTool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeoMath/GeoMath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeoAlgorithms/GeoAlgorithms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeoJsonParsing/GeoJsonParsingUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Measurement/MeasurementTools.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}
)