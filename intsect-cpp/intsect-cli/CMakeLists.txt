# The CLI executable: user-facing entry point to the engine.

add_executable(intsect-cli
  src/main.cpp)

target_link_libraries(intsect-cli PRIVATE intsect-engine intsect_options)

intsect_copy_runtime_deps(intsect-cli)
