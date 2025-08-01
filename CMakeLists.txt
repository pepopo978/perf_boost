cmake_minimum_required(VERSION 3.12)

foreach(policy
        CMP0074 # CMake 3.12
)
    if(POLICY ${policy})
        cmake_policy(SET ${policy} NEW)
    endif()
endforeach()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(PROJECT_NAME perf_boost)

project(${PROJECT_NAME})

# build in Release-mode by default if not explicitly set
if( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
set(BOOST_ROOT "C:/software/boost_1_80_0/boost")
set(BOOST_INCLUDEDIR "C:/software/boost_1_80_0")
set(BOOST_LIBRARYDIR "C:/software/boost_1_80_0/lib32-msvc-14.3")
set (Boost_DETAILED_FAILURE_MSG ON)
set (Boost_DEBUG ON)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost REQUIRED COMPONENTS filesystem program_options)

if (Boost_FOUND)
    message(STATUS "boost found at ${Boost_INCLUDE_DIR}.  Library dir: ${Boost_LIBRARY_DIR_DEBUG}")
else()
    message(FATAL_ERROR "boost not found")
endif()

if ("x_${CMAKE_BUILD_TYPE}" STREQUAL "x_Debug")
    set(HADESMEM_ROOT "C:/software/hadesmem-v142-Debug-Win32")
    set(HADESMEM_BUILD "Debug")
    link_directories("${Boost_LIBRARY_DIR_DEBUG}")
else()
    set(HADESMEM_ROOT "C:/software/hadesmem-v142-Release-Win32")
    set(HADESMEM_BUILD "Release")
    link_directories("${Boost_LIBRARY_DIR_RELEASE}")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64-bit unsupported.  perf_boost integrates with the 1.12.1 client which is only 32 bit")
else()
    set(HADESMEM_ARCH "Win32")
endif()

if ("x_${HADESMEM_ROOT}" STREQUAL "x_")
    if (EXISTS "${CMAKE_SOURCE_DIR}/hadesmem-v${MSVC_TOOLSET_VERSION}-${HADESMEM_BUILD}-${HADESMEM_ARCH}")
        set(HADESMEM_ROOT "${CMAKE_SOURCE_DIR}/hadesmem-v${MSVC_TOOLSET_VERSION}-${HADESMEM_BUILD}-${HADESMEM_ARCH}")
        set(HADESMEM_LIB_DIR "${HADESMEM_ROOT}/lib")
    else()
        message(FATAL_ERROR "HADESMEM_ROOT not set.  ${PROJECT_NAME} requires hadesmem, available at https://github.com/namreeb/hadesmem")
    endif()
else()
    set(HADESMEM_LIB_DIR "${HADESMEM_ROOT}/lib")
endif()

if (NOT EXISTS "${HADESMEM_ROOT}/include/memory/hadesmem")
    message(FATAL_ERROR "hadesmem not found at ${HADESMEM_ROOT}")
else()
    message(STATUS "hadesmem found at ${HADESMEM_ROOT}")
endif()

message(STATUS "hadesmem library directory: ${HADESMEM_LIB_DIR}")

# threading library is required
find_package(Threads REQUIRED)

add_definitions(-DUNICODE -D_UNICODE -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DASMJIT_STATIC -DASMJIT_BUILD_X86 -DHADESMEM_NO_PUGIXML)
include_directories(
        "Include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${Boost_INCLUDE_DIR}"
        "${HADESMEM_ROOT}/include/memory/"
        "${HADESMEM_ROOT}/deps/udis86/udis86"
        "${HADESMEM_ROOT}/deps/asmjit/asmjit/src"
)

link_directories("${HADESMEM_LIB_DIR}")

# Set static runtime library
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")

# Link static libraries
add_subdirectory(perf_boost)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")