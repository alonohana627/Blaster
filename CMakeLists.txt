# Blaster - an action platformer campaign in the spirit of the 8-bit robot
# shooters, built on GameLib: an intro stage, six robot masters in any order,
# their weapons, a three-stage fortress and an ending; every visual a shader,
# every sound synthesized, a deterministic fixed-tick simulation.
cmake_minimum_required(VERSION 3.28)

# C is enabled too because GameLib's dependencies (GLFW, miniaudio) are C.
project(Blaster VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- GameLib -----------------------------------------------------------------
# GameLib is consumed as a library target, never copied: downloaded from
# GitHub at a pinned commit (with its GLFW/miniaudio submodules) into the
# build tree. GameLib keeps no backwards compatibility, so the pin is a full
# commit hash, never a branch or a tag; bump it together with
# docs/GAMELIB_REVISION.md. To develop against a local checkout instead:
#   cmake --preset debug -DFETCHCONTENT_SOURCE_DIR_GAMELIB=<path to GameLib>
# (best kept in CMakeUserPresets.json, which is not committed).
include(FetchContent)
FetchContent_Declare(GameLib
    GIT_REPOSITORY https://github.com/alonohana627/GameLib.git
    GIT_TAG        dfb75a359c82bcfaced0a67e976c49fb72072935
    GIT_SHALLOW    FALSE)  # a shallow clone cannot check out an arbitrary commit
FetchContent_MakeAvailable(GameLib)

# Print the GameLib revision being built against, for bug reports and for the
# known-good combination recorded in docs/GAMELIB_REVISION.md.
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${gamelib_SOURCE_DIR}" rev-parse --short HEAD
                    OUTPUT_VARIABLE _gamelib_revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    message(STATUS "Blaster: using GameLib ${_gamelib_revision} from ${gamelib_SOURCE_DIR}")
endif()

add_subdirectory(src)

# Smoke test: launch, render 120 frames with the validation layer required,
# exit cleanly. Exit code 77 (no Vulkan GPU) counts as skipped. The profile
# file goes to the build folder, not the player's. Blaster has no other
# tests (see AGENTS.md).
enable_testing()
gamelib_validation_layer_dir(_layer_dir)
add_test(NAME blaster_smoke COMMAND blaster --hidden --require-validation --frames 120)
set_tests_properties(blaster_smoke PROPERTIES LABELS "gpu;smoke" TIMEOUT 120 SKIP_RETURN_CODE 77
    ENVIRONMENT "BLASTER_DATA_DIR=${CMAKE_BINARY_DIR}/smoke_data")
if(_layer_dir)
    set_tests_properties(blaster_smoke PROPERTIES
        ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${_layer_dir}")
endif()
