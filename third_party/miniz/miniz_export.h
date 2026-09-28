/* Hand-written replacement for the CMake-generated export header miniz's own
   build normally produces (via generate_export_header()) -- not used here
   since these sources are vendored and compiled directly into MediaFlow's
   single static executable, not built as miniz's own shared/static library
   target. No cross-DLL export/import is needed, so every macro is empty. */
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED

#endif
