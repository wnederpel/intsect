// Version identity for the engine library.
// Placeholder content for Phase 0: proves the library target compiles and links.
// Real engine types (Phase 1+) will be added under include/intsect/ and src/.

#ifndef INTSECT_VERSION_HPP
#define INTSECT_VERSION_HPP

namespace intsect {

// Human-readable version string for the C++ rewrite.
const char* version() noexcept;

}  // namespace intsect

#endif  // INTSECT_VERSION_HPP
