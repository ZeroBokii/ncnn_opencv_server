#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "inotify-cpp::inotify-cpp-shared" for configuration ""
set_property(TARGET inotify-cpp::inotify-cpp-shared APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(inotify-cpp::inotify-cpp-shared PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libinotify-cpp.so.0.2.0"
  IMPORTED_SONAME_NOCONFIG "libinotify-cpp.so.0.2.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS inotify-cpp::inotify-cpp-shared )
list(APPEND _IMPORT_CHECK_FILES_FOR_inotify-cpp::inotify-cpp-shared "${_IMPORT_PREFIX}/lib/libinotify-cpp.so.0.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
