# Defines a single INTERFACE target, intsect_options, that every real target
# links. It carries three things:
#   1. The C++23 requirement.
#   2. Warnings-as-errors.
#   3. AddressSanitizer + UndefinedBehaviorSanitizer, in Debug builds only.
#
# Kept in one place so the dialect rules can never drift between targets.

add_library(intsect_options INTERFACE)

target_compile_features(intsect_options INTERFACE cxx_std_23)

# AddressSanitizer on Windows is incompatible with the debug C runtime
# (msvcrtd / ucrtbased): the CRT's own heap bookkeeping trips ASan's malloc/free
# interceptors and reports spurious bad-free errors at startup. So for the
# sanitized Debug build we link the release CRT while still compiling with -O0 -g.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  # GNU-style driver (this includes clang++ on Windows targeting MSVC).
  target_compile_options(intsect_options INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

  # doctest (and many libraries) use the widely-supported __COUNTER__ macro,
  # which Clang 22 flags under -Wpedantic as a "C2y extension". Silence only
  # that one extension warning; every other pedantic check stays on.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(intsect_options INTERFACE -Wno-c2y-extensions)
  endif()

  # Sanitizers only in Debug. ASan finds memory errors; UBSan is run in
  # "trap" mode so undefined behavior aborts immediately without needing the
  # UBSan runtime library to be linked (simplest and most portable on Windows).
  target_compile_options(intsect_options INTERFACE
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Debug>:-fsanitize=undefined>
    $<$<CONFIG:Debug>:-fsanitize-trap=undefined>)
  target_link_options(intsect_options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address>)

elseif(MSVC)
  # Fallback for a plain MSVC (cl.exe) configure. MSVC has AddressSanitizer
  # but no UndefinedBehaviorSanitizer, so only ASan is enabled here.
  target_compile_options(intsect_options INTERFACE
    /W4 /permissive- /WX
    $<$<CONFIG:Debug>:/fsanitize=address>)
endif()

# On Windows, a Clang ASan-instrumented executable depends on the dynamic ASan
# runtime DLL, which lives in LLVM's clang lib folder rather than on PATH. Find
# it once so we can copy it next to each executable (Windows loads DLLs from the
# exe's own directory first), letting the tests run without PATH changes.
set(INTSECT_ASAN_RUNTIME_DLL "")
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  get_filename_component(_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_llvm_root "${_llvm_bin}" DIRECTORY)
  file(GLOB _asan_dlls
    "${_llvm_root}/lib/clang/*/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
  if(_asan_dlls)
    list(GET _asan_dlls 0 INTSECT_ASAN_RUNTIME_DLL)
  endif()
endif()

# Copies the ASan runtime DLL next to the given executable target, if needed.
# A no-op when the DLL was not found (e.g. Release builds or non-Clang toolchains).
function(intsect_copy_runtime_deps target)
  if(INTSECT_ASAN_RUNTIME_DLL)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${INTSECT_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${target}>"
      COMMENT "Copying ASan runtime next to ${target}"
      VERBATIM)
  endif()
endfunction()
