add_subdirectory(game)
add_subdirectory(app)

# The executable is only main.cpp; everything else lives in the libraries.
add_executable(blaster main.cpp)
target_link_libraries(blaster PRIVATE blaster_app)
gamelib_target_warnings(blaster)
gamelib_copy_runtime(blaster)
