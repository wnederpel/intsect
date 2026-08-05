# Developer tools executable: perft drivers, fixture generators, parity helpers.

add_executable(intsect-tools
  src/main.cpp)

target_link_libraries(intsect-tools PRIVATE intsect-engine intsect_options)

intsect_copy_runtime_deps(intsect-tools)
