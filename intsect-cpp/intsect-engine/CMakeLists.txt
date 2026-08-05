# The engine library: all game/search/eval logic will live here (Phase 1+).
# For Phase 0 it holds only a version function so the target builds cleanly.

add_library(intsect-engine STATIC
  src/version.cpp)

target_include_directories(intsect-engine PUBLIC include)

target_link_libraries(intsect-engine PUBLIC intsect_options)
