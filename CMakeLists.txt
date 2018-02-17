IF(APPLE)
  CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12 FATAL_ERROR)
  CMAKE_POLICY(VERSION 2.8.12)
ELSE()
  CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR)
  CMAKE_POLICY(VERSION 2.8)
ENDIF()

option(USE_LUA53 "Controls whether the library will be for Lua 5.3 or Ravi, default is OFF" OFF)

SET(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_MODULE_PATH}")

find_package(Lua REQUIRED)

message(STATUS "LUA_INCLUDE_DIR: ${LUA_INCLUDE_DIR}")
message(STATUS "LUA_LIBRARIES  : ${LUA_LIBRARIES}")

message(STATUS "LUA_INCDIR     : ${LUA_INCDIR}")
message(STATUS "LUA_LIBDIR     : ${LUA_LIBDIR}")
message(STATUS "LUA_BINDIR     : ${LUA_BINDIR}")
message(STATUS "LUALIB         : ${LUALIB}")
message(STATUS "LIBDIR (LUA_CPATH)  : ${LIBDIR}")
message(STATUS "LUADIR (LUA_PATH)   : ${LUADIR}")

IF (NOT MSVC)
  IF (MINGW)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=format")
  ELSE()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-function-declaration -Werror=format")
  ENDIF(MINGW)
ENDIF(NOT MSVC)

# Flags
# When using MSVC
IF(MSVC)
  # we want to respect the standard, and we are bored of those **** .
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE=1)
  set(CMAKE_C_FLAGS_DEBUG "/Od /D_DEBUG /MDd /Zi /RTC1 /EHsc")
  set(CMAKE_C_FLAGS_RELEASE "/DNDEBUG /O2 /MD /EHsc")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/DNDEBUG /O2 /MD /Zi /EHsc")
  set(CMAKE_C_FLAGS_MINSIZEREL "/DNDEBUG /O2 /MD /EHsc")

  set(CMAKE_CXX_FLAGS "/EHsc")
  set(CMAKE_CXX_FLAGS_DEBUG "/Od /D_DEBUG /MDd /Zi /RTC1 /EHsc")
  set(CMAKE_CXX_FLAGS_RELEASE "/DNDEBUG /O2 /MD /EHsc")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/DNDEBUG /O2 /MD /Zi /EHsc")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "/DNDEBUG /O2 /MD /EHsc")

  # Disable signed/unsigned int conversion warnings.
  add_definitions("/wd4018")
  # Disable warning about using struct/class for the same symobl.
  add_definitions("/wd4099")
  # Disable performance warning about int-to-bool conversion.
  add_definitions("/wd4800")
  # Disable signed/unsigned int conversion warnings.
  add_definitions("/wd4018")
  # Disable warning about the insecurity of using "std::copy".
  add_definitions("/wd4996")
  add_definitions("/wd4291")
  add_definitions("/wd4624")
  add_definitions("/wd4141")
  add_definitions("/DLUA_COMPAT_5_2")
  add_definitions("/DLUA_COMPAT_5_1")
ENDIF(MSVC)

# OpenMP support?
SET(WITH_OPENMP ON CACHE BOOL "OpenMP support if available?")
IF (APPLE AND CMAKE_COMPILER_IS_GNUCC)
  EXEC_PROGRAM (uname ARGS -v  OUTPUT_VARIABLE DARWIN_VERSION)
  STRING (REGEX MATCH "[0-9]+" DARWIN_VERSION ${DARWIN_VERSION})
  MESSAGE (STATUS "MAC OS Darwin Version: ${DARWIN_VERSION}")
  IF (DARWIN_VERSION GREATER 9)
    SET(APPLE_OPENMP_SUCKS 1)
  ENDIF (DARWIN_VERSION GREATER 9)
  EXECUTE_PROCESS (COMMAND ${CMAKE_C_COMPILER} -dumpversion
    OUTPUT_VARIABLE GCC_VERSION)
  IF (APPLE_OPENMP_SUCKS AND GCC_VERSION VERSION_LESS 4.6.2)
    MESSAGE(STATUS "Warning: Disabling OpenMP (unstable with this version of GCC)")
    MESSAGE(STATUS " Install GCC >= 4.6.2 or change your OS to enable OpenMP")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
    SET(WITH_OPENMP OFF CACHE BOOL "OpenMP support if available?" FORCE)
  ENDIF ()
ENDIF ()

IF (WITH_OPENMP)
  FIND_PACKAGE(OpenMP)
  IF(OPENMP_FOUND)
    MESSAGE(STATUS "Compiling with OpenMP support")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  ENDIF(OPENMP_FOUND)
ENDIF (WITH_OPENMP)

# Includes
INCLUDE(TorchPaths)
INCLUDE(TorchPathsInit)
INCLUDE(TorchPackage)
INCLUDE(TorchWrap)
INCLUDE(TorchExports)

message(STATUS "Torch_INSTALL_PREFIX                : ${Torch_INSTALL_PREFIX}")
message(STATUS "Torch_INSTALL_BIN_SUBDIR            : ${Torch_INSTALL_BIN_SUBDIR}")


# Torch libraries
ADD_SUBDIRECTORY(lib)

CONFIGURE_FILE(paths.lua.in "${CMAKE_CURRENT_BINARY_DIR}/paths.lua")

INCLUDE_DIRECTORIES(BEFORE "${LUA_INCDIR}")
INCLUDE_DIRECTORIES(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/lib/TH")
INCLUDE_DIRECTORIES(BEFORE "${CMAKE_CURRENT_BINARY_DIR}/lib/TH")
INCLUDE_DIRECTORIES(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/lib/luaT")
LINK_DIRECTORIES("${LUA_LIBDIR}")

SET(src DiskFile.c File.c MemoryFile.c PipeFile.c Storage.c Tensor.c Timer.c utils.c init.c TensorOperator.c TensorMath.c random.c Generator.c)
SET(luasrc init.lua File.lua Tensor.lua CmdLine.lua FFInterface.lua Tester.lua TestSuite.lua ${CMAKE_CURRENT_BINARY_DIR}/paths.lua test/test.lua)

# Necessary do generate wrapper
ADD_TORCH_WRAP(tensormathwrap TensorMath.lua)
ADD_TORCH_WRAP(randomwrap random.lua)

ADD_TORCH_PACKAGE(torch "${src}" "${luasrc}")

TARGET_LINK_LIBRARIES(torch luaT TH)

IF(LUALIB)
  TARGET_LINK_LIBRARIES(torch ${LUALIB})
ENDIF()

INSTALL(FILES "README.md" DESTINATION "${Torch_INSTALL_LUA_PATH_SUBDIR}/torch")
INSTALL(DIRECTORY "doc" DESTINATION "${Torch_INSTALL_LUA_PATH_SUBDIR}/torch")
