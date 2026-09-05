cmake_minimum_required(VERSION 3.24)

project(surface_lenovo_active3pen_mapping LANGUAGES CXX RC)

add_executable(surface-pen-map WIN32
    src/main.cpp
    src/app.rc
)

target_compile_features(surface-pen-map PRIVATE cxx_std_17)
target_compile_definitions(surface-pen-map PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(surface-pen-map PRIVATE
    hid
    user32
    shell32
    advapi32
    comctl32
    crypt32
    ole32
    windowscodecs
)

if(MSVC)
    target_compile_options(surface-pen-map PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
    )
    target_link_options(surface-pen-map PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/app.manifest"
    )
endif()
