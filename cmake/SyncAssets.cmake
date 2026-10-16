# SyncAssets.cmake
# Copies runtime assets, fonts, and logos to the build output directory while
# excluding work-in-progress source files (*.af, *.kra, *~, *.mkv, *not_used*, inwork, .backup).

function(sync_clean_directory SRC_ROOT DST_ROOT)
    if(NOT EXISTS "${SRC_ROOT}")
        return()
    endif()
    file(MAKE_DIRECTORY "${DST_ROOT}")
    file(GLOB_RECURSE ALL_FILES RELATIVE "${SRC_ROOT}" "${SRC_ROOT}/*")
    foreach(REL_FILE ${ALL_FILES})
        string(TOLOWER "${REL_FILE}" LOWER_REL)
        # Skip work-in-progress / backup / authoring files
        if(LOWER_REL MATCHES "(^|/)inwork(/|$)" OR
           LOWER_REL MATCHES "not_used" OR
           LOWER_REL MATCHES "\\.backup" OR
           LOWER_REL MATCHES "~$" OR
           LOWER_REL MATCHES "\\.(af|kra|mkv|psd|xcf|bak)$")
            if(EXISTS "${DST_ROOT}/${REL_FILE}")
                file(REMOVE "${DST_ROOT}/${REL_FILE}")
            endif()
            continue()
        endif()

        get_filename_component(REL_DIR "${REL_FILE}" DIRECTORY)
        if(REL_DIR)
            file(MAKE_DIRECTORY "${DST_ROOT}/${REL_DIR}")
        endif()
        file(COPY_FILE "${SRC_ROOT}/${REL_FILE}" "${DST_ROOT}/${REL_FILE}" ONLY_IF_DIFFERENT)
    endforeach()
endfunction()

sync_clean_directory("${SOURCE_DIR}/assets" "${BINARY_OUT_DIR}/assets")
sync_clean_directory("${SOURCE_DIR}/fonts" "${BINARY_OUT_DIR}/fonts")
sync_clean_directory("${SOURCE_DIR}/logos" "${BINARY_OUT_DIR}/logos")

# Ensure no leftover plugin directory or .lib / .exp files remain in the runtime output directory
if(EXISTS "${BINARY_OUT_DIR}/plugins")
    file(REMOVE_RECURSE "${BINARY_OUT_DIR}/plugins")
endif()
file(GLOB RUNTIME_LIBS "${BINARY_OUT_DIR}/*.lib" "${BINARY_OUT_DIR}/*.exp")
if(RUNTIME_LIBS)
    file(REMOVE ${RUNTIME_LIBS})
endif()
