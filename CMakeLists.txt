cmake_minimum_required(VERSION 3.10)
project(Fuser_CustomSongCreator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Platform detection ────────────────────────────────────────────────────────
if(APPLE)
    set(PLATFORM_MAC TRUE)
    list(APPEND CMAKE_PREFIX_PATH /usr/local /opt/homebrew)
    find_package(OpenGL REQUIRED)
    find_package(glfw3  REQUIRED)
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
        glfw
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bass/mac/libbass.dylib"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND install_name_tool -add_rpath "@executable_path"
            $<TARGET_FILE:${PROJECT_NAME}>
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
