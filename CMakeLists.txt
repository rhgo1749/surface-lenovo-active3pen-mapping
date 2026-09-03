cmake_minimum_required(VERSION 3.24)

project(surface_lenovo_active3pen_mapping LANGUAGES CXX)

add_executable(surface-pen-map WIN32
    src/main.cpp
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
)

if(MSVC)
    target_compile_options(surface-pen-map PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
    )
endif()
