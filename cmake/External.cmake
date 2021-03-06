include(CMakeParseArguments)
include(ExternalProject)

function(_argument_default VARIABLE)
  if(args_${VARIABLE})
    set(${VARIABLE} "${args_${VARIABLE}}" PARENT_SCOPE)
  else()
    set(${VARIABLE} "${ARGN}" PARENT_SCOPE)
  endif()
endfunction(_argument_default)

#
# Adds an external project.
#
# See ExternalProject_Add(...) for usage.
#
function(add_external_project TARGET)
  set(ARGS_ONE_VALUE GIT_TAG)
  cmake_parse_arguments(args "" "${ARGS_ONE_VALUE}" "" ${ARGN})

  if(args_GIT_TAG)
    message(STATUS "${TARGET} tag: ${args_GIT_TAG}")
  endif()
  
  # Compile arguments for ExternalProject_Add.
  set(ARGN_EXT "${ARGN}")
  list(APPEND ARGN_EXT CMAKE_ARGS 
    "-G${CMAKE_GENERATOR}"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>")
  
  # Add external project.
  ExternalProject_Add(${TARGET} "${ARGN_EXT}")
endfunction(add_external_project)

#
# Adds an external library, depending on an external project.
#
# add_external_library(<target> (STATIC|SHARED|INTERFACE)
#     DEPENDS <external_project>
#     LIBRARY_DEBUG "Debug.dll"
#     LIBRARY_RELEASE "Release.dll"
#     IMPORT_LIBRARY_DEBUG "Debug.lib"
#     IMPORT_LIBRARY_RELEASE "Release.lib"
#     INTERFACE_LIBRARIES <external_library>*")
#
function(add_external_library TARGET LINKAGE)
  set(ARGS_ONE_VALUE DEPENDS COMPILE_DEFINITIONS INCLUDE_DIR LIBRARY LIBRARY_DEBUG LIBRARY_RELEASE IMPORT_LIBRARY IMPORT_LIBRARY_DEBUG IMPORT_LIBRARY_RELEASE INTERFACE_LIBRARIES)
  cmake_parse_arguments(args "" "${ARGS_ONE_VALUE}" "" ${ARGN})
 
   if(NOT args_DEPENDS)
     message(FATAL_ERROR "Missing external project for ${TARGET} to depend on")
   endif()
  _argument_default(DEPENDS "")
 
  # Guess library properties, unless set.
  ExternalProject_Get_Property(${DEPENDS} INSTALL_DIR)
  _argument_default(COMPILE_DEFINITIONS "")
  _argument_default(INCLUDE_DIR include)
  _argument_default(LIBRARY "NOTFOUND")
  _argument_default(LIBRARY_DEBUG "${LIBRARY}")
  _argument_default(LIBRARY_RELEASE "${LIBRARY}")
  _argument_default(IMPORT_LIBRARY "NOTFOUND")
  _argument_default(IMPORT_LIBRARY_DEBUG "${IMPORT_LIBRARY}")
  _argument_default(IMPORT_LIBRARY_RELEASE "${IMPORT_LIBRARY}")

  # Create include directory as required by INTERFACE_INCLUDE_DIRECTORIES.
  file(MAKE_DIRECTORY "${INSTALL_DIR}/${INCLUDE_DIR}")

  # Add an imported library.
  add_library(${TARGET} ${LINKAGE} IMPORTED GLOBAL)
  add_dependencies(${TARGET} ${DEPENDS})
  set_target_properties(${TARGET} PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS}"
    INTERFACE_INCLUDE_DIRECTORIES "${INSTALL_DIR}/${INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${INTERFACE_LIBRARIES}")
  if(LINKAGE STREQUAL "STATIC" OR LINKAGE STREQUAL "SHARED")
    set_target_properties(${TARGET} PROPERTIES
      IMPORTED_CONFIGURATIONS "Debug;Release"
      IMPORTED_LOCATION_DEBUG "${INSTALL_DIR}/${LIBRARY_DEBUG}"
      IMPORTED_LOCATION_RELEASE "${INSTALL_DIR}/${LIBRARY_RELEASE}")
  endif()
  if(LINKAGE STREQUAL "SHARED")
    set_target_properties(${TARGET} PROPERTIES
      IMPORTED_IMPLIB_DEBUG "${INSTALL_DIR}/${IMPORT_LIBRARY_DEBUG}"
      IMPORTED_IMPLIB_RELEASE "${INSTALL_DIR}/${IMPORT_LIBRARY_RELEASE}")
  endif()
  
  #message(STATUS
  #  "${DEPENDS} / ${TARGET} settings:\n"
  #  "  Definitions:\t${COMPILE_DEFINITIONS}\n"
  #  "  Include:\t\t${INSTALL_DIR}/${INCLUDE_DIR}\n"
  #  "  Library (debug):\t${INSTALL_DIR}/${LIBRARY_DEBUG}\n"
  #  "  Library (release):\t${INSTALL_DIR}/${LIBRARY_RELEASE}\n"
  #  "  Import (debug):\t${INSTALL_DIR}/${IMPORT_LIBRARY_DEBUG}\n"
  #  "  Import (release):\t${INSTALL_DIR}/${IMPORT_LIBRARY_RELEASE}\n"
  #  "  Interface:\t${INTERFACE_LIBRARIES}")
endfunction(add_external_library)

#
# Installs external targets.
#
# install_external(TARGET libA BLib C)
#
# This is a workaround for limitations of install(TARGETS ...) [1][2].
# [1] https://gitlab.kitware.com/cmake/cmake/issues/14311
# [2] https://gitlab.kitware.com/cmake/cmake/issues/14444
#
function(install_external)
  set(ARGS_MULTI_VALUE TARGETS)
  cmake_parse_arguments(args "" "" "${ARGS_MULTI_VALUE}" ${ARGN})
 
  foreach(target ${args_TARGETS})
    get_target_property(IMPORTED_IMPLIB_DEBUG ${target} IMPORTED_IMPLIB_DEBUG)
    get_target_property(IMPORTED_IMPLIB_RELEASE ${target} IMPORTED_IMPLIB_RELEASE)
    get_target_property(IMPORTED_LOCATION_DEBUG ${target} IMPORTED_LOCATION_DEBUG)
    get_target_property(IMPORTED_LOCATION_RELEASE ${target} IMPORTED_LOCATION_RELEASE)

    if(WIN32)
      install(FILES ${IMPORTED_IMPLIB_DEBUG} DESTINATION "lib" OPTIONAL)
      install(FILES ${IMPORTED_IMPLIB_RELEASE} DESTINATION "lib" OPTIONAL)
      set(TARGET_DESTINATION "bin")
    else()
      set(TARGET_DESTINATION "lib")
    endif()

    # Wildcard-based install to catch PDB files and symlinks.
    install(CODE "\
      file(GLOB DEBUG_FILES \"${IMPORTED_LOCATION_DEBUG}*\")\n \
      file(GLOB RELEASE_FILES \"${IMPORTED_LOCATION_RELEASE}*\")\n \
      file(INSTALL \${DEBUG_FILES} \${RELEASE_FILES} DESTINATION \"${CMAKE_INSTALL_PREFIX}/${TARGET_DESTINATION}\")")
  endforeach()
endfunction(install_external)
