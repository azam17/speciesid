cmake_minimum_required(VERSION 3.16)
project(SpeciesID C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Core library (everything except main.c) -----------------------
file(GLOB CORE_SRCS ${CMAKE_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM CORE_SRCS ${CMAKE_SOURCE_DIR}/src/main.c)

add_library(speciesid_core STATIC ${CORE_SRCS})
target_include_directories(speciesid_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib)
if(MSVC)
    target_compile_options(speciesid_core PRIVATE /W4)
else()
    target_compile_options(speciesid_core PRIVATE -Wall -Wextra)
endif()

find_package(ZLIB REQUIRED)
target_link_libraries(speciesid_core PRIVATE ZLIB::ZLIB)
if(UNIX)
    target_link_libraries(speciesid_core PRIVATE m pthread)
endif()

# ---- CLI binary ----------------------------------------------------
add_executable(speciesid ${CMAKE_SOURCE_DIR}/src/main.c)
target_link_libraries(speciesid PRIVATE speciesid_core)

# ---- GUI binary (only if SDL2 is available) ------------------------
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(SpeciesIDGUI WIN32 MACOSX_BUNDLE
        ${CMAKE_SOURCE_DIR}/gui/main_gui.c
        ${CMAKE_SOURCE_DIR}/gui/gui_analysis.c
        ${CMAKE_SOURCE_DIR}/lib/tinyfiledialogs.c)

    target_include_directories(SpeciesIDGUI PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/gui)

    target_link_libraries(SpeciesIDGUI PRIVATE
        speciesid_core
        SDL2::SDL2)

    if(APPLE)
        target_link_libraries(SpeciesIDGUI PRIVATE "-framework Cocoa")
        set_target_properties(SpeciesIDGUI PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/gui/Info.plist
            MACOSX_BUNDLE_BUNDLE_NAME "SpeciesID"
            MACOSX_BUNDLE_BUNDLE_VERSION "0.2.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.2.0")

        # --- Embed SDL2 dylib in .app bundle for self-contained distribution ---
        add_custom_command(TARGET SpeciesIDGUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:SpeciesIDGUI>/Contents/Frameworks"
            COMMENT "Creating Frameworks directory in app bundle")

        # Copy SDL2 dylib into the bundle and rewrite the load path
        add_custom_command(TARGET SpeciesIDGUI POST_BUILD
            COMMAND bash -c "
                SDL2_DYLIB=$$(otool -L '$<TARGET_FILE:SpeciesIDGUI>' | grep -o '[^ ]*libSDL2[^ ]*\\.dylib' | head -1) && \
                if [ -n \"$$SDL2_DYLIB\" ] && [ -f \"$$SDL2_DYLIB\" ]; then \
                    DYLIB_NAME=$$(basename \"$$SDL2_DYLIB\") && \
                    cp \"$$SDL2_DYLIB\" '$<TARGET_BUNDLE_DIR:SpeciesIDGUI>/Contents/Frameworks/'\"$$DYLIB_NAME\" && \
                    chmod 755 '$<TARGET_BUNDLE_DIR:SpeciesIDGUI>/Contents/Frameworks/'\"$$DYLIB_NAME\" && \
                    install_name_tool -change \"$$SDL2_DYLIB\" \
                        '@executable_path/../Frameworks/'\"$$DYLIB_NAME\" \
                        '$<TARGET_FILE:SpeciesIDGUI>'; \
                fi"
            COMMENT "Embedding SDL2 dylib in app bundle")

        # Ad-hoc codesign (wrapped in bash so || true works)
        add_custom_command(TARGET SpeciesIDGUI POST_BUILD
            COMMAND bash -c "codesign --force --sign - --deep '$<TARGET_BUNDLE_DIR:SpeciesIDGUI>' 2>/dev/null || true"
            COMMENT "Ad-hoc codesigning app bundle")
    endif()

    if(WIN32)
        target_link_libraries(SpeciesIDGUI PRIVATE comdlg32 ole32)

        # --- Copy SDL2.dll alongside the executable on Windows ---
        get_target_property(SDL2_DLL_LOCATION SDL2::SDL2 IMPORTED_LOCATION)
        if(SDL2_DLL_LOCATION)
            add_custom_command(TARGET SpeciesIDGUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SDL2_DLL_LOCATION}"
                    "$<TARGET_FILE_DIR:SpeciesIDGUI>"
                COMMENT "Copying SDL2.dll alongside executable")
        else()
            # Try to find SDL2.dll relative to the SDL2 library
            add_custom_command(TARGET SpeciesIDGUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SDL2_DIR}/../../../bin/SDL2.dll"
                    "$<TARGET_FILE_DIR:SpeciesIDGUI>"
                COMMENT "Copying SDL2.dll alongside executable (fallback path)")
        endif()
    endif()

    message(STATUS "SDL2 found — GUI target enabled")
else()
    message(STATUS "SDL2 not found — GUI target disabled (CLI only)")
endif()

# ---- Tests ---------------------------------------------------------
enable_testing()
file(GLOB TEST_SRCS ${CMAKE_SOURCE_DIR}/test/test_*.c)
foreach(test_src ${TEST_SRCS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE speciesid_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
