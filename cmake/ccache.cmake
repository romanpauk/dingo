macro(enable_ccache)
    find_program(CCACHE ccache)
    if(CCACHE)
        if (WIN32)
            # https://github.com/ccache/ccache/wiki/MS-Visual-Studio#usage-with-cmake
            set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
            set(DINGO_CCACHE_CL_DIR "${CMAKE_BINARY_DIR}/ccache-cl")
            file(MAKE_DIRECTORY "${DINGO_CCACHE_CL_DIR}")
            file(COPY_FILE ${CCACHE} "${DINGO_CCACHE_CL_DIR}/cl.exe"
                ONLY_IF_DIFFERENT)

            set(CMAKE_VS_GLOBALS
                "CLToolExe=cl.exe"
                "CLToolPath=${DINGO_CCACHE_CL_DIR}"
                "UseMultiToolTask=true"
                "DebugInformationFormat=OldStyle"
            )
        else()
            set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
        endif()
    else()
        message(FATAL_ERROR "ccache not found")
    endif()
endmacro()
