#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "soem_rsl" for configuration ""
set_property(TARGET soem_rsl APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(soem_rsl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libsoem_rsl.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS soem_rsl )
list(APPEND _IMPORT_CHECK_FILES_FOR_soem_rsl "${_IMPORT_PREFIX}/lib/libsoem_rsl.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
