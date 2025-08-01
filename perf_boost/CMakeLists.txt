set(DLL_NAME perf_boost)

# Only include local headers here;
include_directories(
    Include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

set(SOURCE_FILES
        logging.hpp
        logging.cpp
        main.hpp
        main.cpp
        offsets.hpp
)

add_library(${DLL_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${DLL_NAME} shlwapi.lib asmjit.lib udis86.lib)

install(TARGETS ${DLL_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
