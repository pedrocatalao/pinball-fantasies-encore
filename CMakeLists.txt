cmake_minimum_required(VERSION 3.24)
project(PinballFantasiesEncore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# SDL comes from the system (Homebrew, a distribution's package, the Windows development
# files). On macOS it can instead be the official universal framework, which is what a build
# for both Apple Silicon and Intel needs: -DENCORE_SDL3_FRAMEWORK=<path to SDL3.framework>.
set(ENCORE_SDL3_FRAMEWORK "" CACHE PATH "SDL3.framework to build against instead of the system's SDL3")
if(ENCORE_SDL3_FRAMEWORK)
  get_filename_component(_sdl3_dir "${ENCORE_SDL3_FRAMEWORK}" DIRECTORY)
  add_library(encore_sdl3 INTERFACE)
  target_include_directories(encore_sdl3 INTERFACE "${ENCORE_SDL3_FRAMEWORK}/Headers")
  target_compile_options(encore_sdl3 INTERFACE "-F${_sdl3_dir}")
  target_link_options(encore_sdl3 INTERFACE "-F${_sdl3_dir}" "-framework" "SDL3"
                      "-Wl,-rpath,@executable_path/../Frameworks")
  add_library(SDL3::SDL3 ALIAS encore_sdl3)
else()
  find_package(SDL3 REQUIRED CONFIG)
endif()
# libGL.so.1 rather than libOpenGL.so.0: every Linux with working graphics has the first,
# while the second comes with libglvnd, which older systems do not have at all.
set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# encore_core: everything that does not touch SDL or OpenGL. Unit-testable.
# ---------------------------------------------------------------------------
add_library(encore_core STATIC
  src/core/Log.cpp
  src/core/File.cpp
  src/core/Png.cpp
  src/core/Inflate.cpp
  src/core/Json.cpp
  src/core/Zip.cpp
  src/data/IffImage.cpp
  src/data/GameFiles.cpp
  src/data/OpenGame.cpp
  src/assets/Exe.cpp
  src/core/Sha256.cpp
  src/data/GameVersion.cpp
  src/assets/TableAssets.cpp
  src/assets/OpenFormat.cpp
  src/assets/TableAssetsIo.cpp
  src/sound/Mod.cpp
  src/sound/Sequencer.cpp
  src/sound/Player.cpp
  src/game/Config.cpp
  src/intro/IntroAssets.cpp
  src/intro/IntroAssetsIo.cpp
  src/intro/Intro.cpp
  src/table/Table.cpp
  src/table/TablePhysics.cpp
  src/table/TableScript.cpp
  src/table/TableTasks.cpp
  src/table/TableGame.cpp
  src/table/Party.cpp
  src/table/Speed.cpp
  src/table/Show.cpp
  src/table/Stones.cpp
  src/gfx/Framebuffer.cpp
  src/gfx/Palette.cpp
)
target_include_directories(encore_core PUBLIC src)
if(MSVC)
  target_compile_options(encore_core PUBLIC /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(encore_core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(encore_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# ---------------------------------------------------------------------------
# Executable: SDL3 window, OpenGL renderer, audio device, game loop.
# ---------------------------------------------------------------------------
add_executable(pinball-fantasies WIN32 MACOSX_BUNDLE
  src/main.cpp
  src/platform/Window.cpp
  src/platform/DataLocator.cpp
  src/platform/AudioDevice.cpp
  src/platform/ImageFile.cpp
  src/platform/Download.cpp
  src/game/Fantasy.cpp
  src/gfx/Gl.cpp
  src/gfx/Renderer.cpp
  src/gfx/ShaderProgram.cpp
  src/game/App.cpp
)
# Pictures are decoded by this project's own PNG reader, and every OpenGL function past 1.1
# is fetched from the driver, so nothing here is platform-specific but the library names.
target_link_libraries(pinball-fantasies PRIVATE encore_core SDL3::SDL3 OpenGL::GL)
# Fetching a set of pictures uses what the platform already has, so there is nothing to
# install: libcurl ships with macOS and every Linux, WinHTTP is part of Windows.
if(WIN32)
  target_link_libraries(pinball-fantasies PRIVATE winhttp)
else()
  target_link_libraries(pinball-fantasies PRIVATE ${CMAKE_DL_LIBS})
endif()
target_compile_definitions(pinball-fantasies PRIVATE GL_SILENCE_DEPRECATION=1 ENCORE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(pinball-fantasies PROPERTIES OUTPUT_NAME "Pinball Fantasies")

if(APPLE)
  set_target_properties(pinball-fantasies PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Pinball Fantasies"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.encore.pinball-fantasies"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "icon")

  # The application's icon, built from the one picture into the sizes the Finder asks for.
  set(ICON_SET "${CMAKE_BINARY_DIR}/icon.iconset")
  set(ICON_FILE "${CMAKE_BINARY_DIR}/icon.icns")
  add_custom_command(OUTPUT "${ICON_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ICON_SET}"
    COMMAND sips -z 16 16     "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_16x16.png" > /dev/null
    COMMAND sips -z 32 32     "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_16x16@2x.png" > /dev/null
    COMMAND sips -z 32 32     "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_32x32.png" > /dev/null
    COMMAND sips -z 64 64     "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_32x32@2x.png" > /dev/null
    COMMAND sips -z 128 128   "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_128x128.png" > /dev/null
    COMMAND sips -z 256 256   "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_128x128@2x.png" > /dev/null
    COMMAND sips -z 256 256   "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_256x256.png" > /dev/null
    COMMAND sips -z 512 512   "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_256x256@2x.png" > /dev/null
    COMMAND sips -z 512 512   "${CMAKE_SOURCE_DIR}/assets/app/icon.png" --out "${ICON_SET}/icon_512x512.png" > /dev/null
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/app/icon.png" "${ICON_SET}/icon_512x512@2x.png"
    COMMAND iconutil -c icns "${ICON_SET}" -o "${ICON_FILE}"
    DEPENDS "${CMAKE_SOURCE_DIR}/assets/app/icon.png"
    COMMENT "Building the application icon")
  add_custom_target(encore_icon DEPENDS "${ICON_FILE}")
  add_dependencies(pinball-fantasies encore_icon)
endif()

# Shaders and the replacement pictures travel with the application: inside the bundle on
# macOS, beside the executable elsewhere. This runs on every build, not only when the
# executable is relinked, so editing a shader alone still reaches the application.
if(APPLE)
  set(PFR_RESOURCE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:pinball-fantasies>/Resources")
else()
  set(PFR_RESOURCE_DIR "$<TARGET_FILE_DIR:pinball-fantasies>")
endif()

add_custom_target(encore_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/shaders" "${PFR_RESOURCE_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/hd" "${PFR_RESOURCE_DIR}/hd"
  DEPENDS pinball-fantasies
  COMMENT "Copying shaders and pictures next to the application")

if(APPLE)
  add_custom_command(TARGET encore_shaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_FILE}" "${PFR_RESOURCE_DIR}/icon.icns")
endif()

# ---------------------------------------------------------------------------
# Command-line tools (asset inspection, no SDL).
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(encore-tests
  tests/main.cpp
  tests/test_core.cpp
  tests/test_game.cpp
  tests/test_open_format.cpp
)
target_compile_definitions(encore-tests PRIVATE ENCORE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(encore-tests PRIVATE encore_core)
add_test(NAME unit COMMAND encore-tests)

add_executable(encore-assets tools/assets_check.cpp)
target_link_libraries(encore-assets PRIVATE encore_core)

add_executable(encore-play tools/play.cpp)
target_link_libraries(encore-play PRIVATE encore_core)

add_executable(encore-extract tools/extract.cpp)
target_link_libraries(encore-extract PRIVATE encore_core)
add_executable(encore-convert tools/convert.cpp)
target_link_libraries(encore-convert PRIVATE encore_core)
