cmake_minimum_required(VERSION 3.10)
project(Fuser_CustomSongCreator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Platform detection ────────────────────────────────────────────────────────
if(APPLE)
    set(PLATFORM_MAC TRUE)
    list(APPEND CMAKE_PREFIX_PATH /usr/local /opt/homebrew)
    find_package(OpenGL REQUIRED)

    # Prefer static GLFW so the binary ships without a libglfw dependency.
    # Homebrew installs libglfw3.a alongside the dylib.
    find_library(GLFW_STATIC_LIB libglfw3.a
        PATHS /opt/homebrew/lib /usr/local/lib
        NO_DEFAULT_PATH)
    if(GLFW_STATIC_LIB)
        message(STATUS "Static GLFW: ${GLFW_STATIC_LIB}")
        find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h
            PATHS /opt/homebrew/include /usr/local/include
            NO_DEFAULT_PATH)
        add_library(glfw_import STATIC IMPORTED)
        set_target_properties(glfw_import PROPERTIES
            IMPORTED_LOCATION             "${GLFW_STATIC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}")
        set(GLFW_LINK_TARGET glfw_import)
    else()
        message(WARNING "libglfw3.a not found – falling back to dynamic GLFW. "
                        "Target Macs will need GLFW installed (brew install glfw).")
        find_package(glfw3 REQUIRED)
        set(GLFW_LINK_TARGET glfw)
    endif()
endif()

# ── Shared source files ───────────────────────────────────────────────────────
set(SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uasset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fuser_asset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sha1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hmx_midifile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/custom_song_creator.cpp
    $<$<BOOL:${PLATFORM_MAC}>:${CMAKE_CURRENT_SOURCE_DIR}/src/ImageFile.cpp>

    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/CCallbacks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/OggMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/oggvorbis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/VorbisEncrypter.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_stdlib.cpp
)

# imgui_tables.cpp only exists in newer imgui versions – add if present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp")
    list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp)
endif()

# ── Platform-specific ImGui backends ─────────────────────────────────────────
if(APPLE)
    list(APPEND SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_opengl3.cpp
    )
elseif(WIN32)
    list(APPEND SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_win32.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_dx11.cpp
    )
endif()

# SMF (Standard MIDI File) library + stream helpers
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/SMF.cpp)
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/stream-helpers.cpp)

# ── Executable ────────────────────────────────────────────────────────────────
if(WIN32)
add_executable(${PROJECT_NAME} WIN32 ${SRCS})
else()
    add_executable(${PROJECT_NAME} ${SRCS})
endif()

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/"
)

# ── Compile definitions ───────────────────────────────────────────────────────
if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PLATFORM_MAC
        GL_SILENCE_DEPRECATION
        # Tell imgui_impl_opengl3 we provide our own GL header (OpenGL/gl3.h)
        IMGUI_IMPL_OPENGL_LOADER_CUSTOM=""
    )
    # Suppress the bass.h macro-redefinition warnings
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-macro-redefined)
endif()

# ── Linking ───────────────────────────────────────────────────────────────────
if(APPLE)
    find_library(BASS_LIB bass
        PATHS "${CMAKE_CURRENT_SOURCE_DIR}/bass/mac"
        NO_DEFAULT_PATH)
    if(NOT BASS_LIB)
        message(FATAL_ERROR
            "libbass.dylib not found.\n"
            "Download macOS BASS from https://www.un4seen.com\n"
            "Place libbass.dylib in: ${CMAKE_CURRENT_SOURCE_DIR}/bass/mac/")
    endif()

    target_link_libraries(${PROJECT_NAME}
        ${BASS_LIB}
        OpenGL::GL
        ${GLFW_LINK_TARGET}
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework AudioUnit"
        "-framework CoreAudio"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # Copy libbass.dylib next to the executable
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bass/mac/libbass.dylib"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        # Rewrite any absolute/rpath reference to libbass so dyld finds it beside the exe
        COMMAND install_name_tool
            -change "/usr/local/lib/libbass.dylib" "@executable_path/libbass.dylib"
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMAND install_name_tool
            -change "@rpath/libbass.dylib" "@executable_path/libbass.dylib"
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMAND install_name_tool -add_rpath "@executable_path"
            $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Bundling libbass.dylib next to executable"
    )

elseif(WIN32)
    target_link_libraries(${PROJECT_NAME}
        "d3d11.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/bass/bass.lib"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bass/bass.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# Force aes.c compiled as C
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/moggcrypt/aes.c
    PROPERTIES LANGUAGE C)

set_property(TARGET ${PROJECT_NAME}
    PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
