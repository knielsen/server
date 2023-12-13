message("* Configuring build...")

function(set_compiler_and_linker_flags build_type compiler_flags linker_flags)     
  SET(CMAKE_CXX_FLAGS_${build_type}
      "${compiler_flags}"
      CACHE STRING "Flags used by C++ compiler during ${build_type} builds"
      FORCE)
  SET(CMAKE_C_FLAGS_${build_type}
      "${compiler_flags}"
      CACHE STRING "Flags used by C compiler during ${build_type} builds"
      FORCE)
  SET(CMAKE_ASM_FLAGS_${build_type}
      "${compiler_flags}"
      CACHE STRING "Flags used by assembler during ${build_type} builds"
      FORCE)
  SET(CMAKE_EXE_LINKER_FLAGS_${build_type}
      "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${linker_flags}"
      CACHE STRING "Flags used for linking binaries during ${build_type} builds"
      FORCE)
  SET(CMAKE_SHARED_LINKER_FLAGS_${build_type}
      "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${linker_flags}"
      CACHE STRING "Flags used by shared libraries linker during ${build_type} builds"
      FORCE)
  SET(CMAKE_STATIC_LINKER_FLAGS_${build_type}
      "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}"
      CACHE STRING "Flags used by static libraries linker during ${build_type} builds"
      FORCE)
  SET(CMAKE_MODULE_LINKER_FLAGS_${build_type}
      "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${linker_flags}"
      CACHE STRING "Flags used by module libraries linker during ${build_type} builds"
      FORCE)
  SET(CMAKE_BUILD_TYPE ${build_type}
      CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo MinSizeRel ${build_type}"
      FORCE)
  MARK_AS_ADVANCED(FORCE
      CMAKE_CXX_FLAGS_${build_type}
      CMAKE_C_FLAGS_${build_type}
      CMAKE_ASM_FLAGS_${build_type}
      CMAKE_EXE_LINKER_FLAGS_${build_type}
      CMAKE_SHARED_LINKER_FLAGS_${build_type}
      CMAKE_STATIC_LINKER_FLAGS_${build_type}
      CMAKE_MODULE_LINKER_FLAGS_${build_type})
endfunction()

# TODO: if cached variable not yet exists it will have empty HELPSTRING,
# we don't duplicate them there. CMake is very limited on overriding cached
# variables with normal variables (-D and normal variables work differently).
macro(setc var value)
  if("$CACHE{${var}}" STREQUAL "")
    set (extra_args ${ARGN})
    list(GET extra_args 0 type)
    if ("${type}" STREQUAL "NOTFOUND")
      set(${var} "${value}" CACHE BOOL "" FORCE)
    else()
      set(${var} "${value}" CACHE ${type} "" FORCE)
    endif()
  else()
    get_property(doc CACHE ${var} PROPERTY HELPSTRING)
    get_property(type CACHE ${var} PROPERTY TYPE)
    set(${var} "${value}" CACHE ${type} "${doc}" FORCE)
  endif()
endmacro()

function(exclude_plugins plugins_to_exclude)
  foreach(plugin ${plugins_to_exclude})
    setc(${plugin} NO)
  endforeach()
endfunction()

function(convert_dynamic_to_static)
  get_cmake_property(vars VARIABLES)
  list (SORT vars)
  foreach (var ${vars})
    string(REGEX MATCH ^PLUGIN_ m ${var})
    if (m)
      string(REGEX MATCH ^DYNAMIC$ m "${${var}}")
      if (m)
        setc(${var} STATIC STRING)
      endif()
    endif()
  endforeach()
endfunction()

function(make_plugins_static_if_shared_libs_are_disabled)
  if(DISABLE_SHARED)
    # Make DYNAMIC plugins STATIC because of DISABLE_SHARED
    convert_dynamic_to_static()
  else()
    # PAM plugin does not compile as static (see MODULE_ONLY in auth_pam/CMakeLists.txt)
    setc(PLUGIN_AUTH_PAM DYNAMIC STRING)
  endif()
endfunction()



