cmake_minimum_required(VERSION 3.20)

project(EOM_Controller_NI
    VERSION 1.0.0
    DESCRIPTION "EOM controller DLL (NI-DAQmx based)"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR
        "EOM_Controller_NI targets Windows (it uses NI-DAQmx and builds a Win32 DLL).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a Release build.
# - Single-config generators (Ninja, Makefiles): set CMAKE_BUILD_TYPE to Release
#   unless the user picked something else.
# - Multi-config generators (Visual Studio): the config is chosen at build time
#   (use --config Release, or the "release" preset in CMakePresets.json).
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Release, Debug, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# ---------------------------------------------------------------------------
# Locate NI-DAQmx (NIDAQmx.h + NIDAQmx.lib).
#
# Install "NI-DAQmx" with the ANSI C / C development support, then this should
# be found automatically. If you installed it somewhere non-standard, pass:
#     cmake -B build -DNIDAQMX_ROOT="C:/path/to/NI/ExternalCompilerSupport/C"
# ---------------------------------------------------------------------------
set(_nidaqmx_search_roots
    "${NIDAQMX_ROOT}"
    "$ENV{ProgramFiles\(x86\)}/National Instruments/Shared/ExternalCompilerSupport/C"
    "$ENV{ProgramFiles\(x86\)}/National Instruments/NI-DAQ/DAQmx ANSI C Dev"
    "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C"
    "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev"
)

find_path(NIDAQMX_INCLUDE_DIR
    NAMES NIDAQmx.h
    PATHS ${_nidaqmx_search_roots}
    PATH_SUFFIXES include
)

find_library(NIDAQMX_LIBRARY
    NAMES NIDAQmx
    PATHS ${_nidaqmx_search_roots}
    PATH_SUFFIXES lib64/msvc lib/msvc lib
)

if(NOT NIDAQMX_INCLUDE_DIR OR NOT NIDAQMX_LIBRARY)
    message(FATAL_ERROR
        "NI-DAQmx not found.\n"
        "  Install the NI-DAQmx C/ANSI development support, or pass -DNIDAQMX_ROOT=<path>.\n"
        "    NIDAQmx.h  -> ${NIDAQMX_INCLUDE_DIR}\n"
        "    NIDAQmx.lib-> ${NIDAQMX_LIBRARY}")
endif()

message(STATUS "NI-DAQmx include: ${NIDAQMX_INCLUDE_DIR}")
message(STATUS "NI-DAQmx library: ${NIDAQMX_LIBRARY}")

# ---------------------------------------------------------------------------
# The DLL target
# ---------------------------------------------------------------------------
add_library(EOM_Controller_NI SHARED
    src/dllmain.cpp
    src/DAC_module.cpp
    src/DualEOM_module.cpp
    src/DAC_functions.cpp
    src/pch.cpp
)

target_include_directories(EOM_Controller_NI
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${NIDAQMX_INCLUDE_DIR}"
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(EOM_Controller_NI PRIVATE "${NIDAQMX_LIBRARY}")

target_compile_definitions(EOM_Controller_NI PRIVATE
    EOMCONTROLLERNI_EXPORTS
    _WINDOWS
    _USRDLL
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING
        "Configuring a 32-bit build. The default NI-DAQmx library path is 64-bit; "
        "use the 64-bit generator (e.g. -A x64) to match the shipped .lib.")
endif()

# Put the resulting DLL/LIB in build/bin.
set_target_properties(EOM_Controller_NI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy the DLL into the Python package (eom_controller/bin), which ships it.
option(EOM_COPY_TO_PYTHON "Copy the built DLL into ../eom_controller/bin" ON)
if(EOM_COPY_TO_PYTHON)
    add_custom_command(TARGET EOM_Controller_NI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:EOM_Controller_NI>"
                "${CMAKE_CURRENT_SOURCE_DIR}/../eom_controller/bin/EOM_Controller_NI.dll"
        COMMENT "Copying EOM_Controller_NI.dll into the eom_controller Python package")
endif()
