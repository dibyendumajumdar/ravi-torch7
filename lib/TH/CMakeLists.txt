cmake_minimum_required(VERSION 3.0)

IF(POLICY CMP0026)
 CMAKE_POLICY(SET CMP0026 OLD)
ENDIF()

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindCUDA
  /usr/lib/x86_64-linux-gnu/
  ${CMAKE_CURRENT_SOURCE_DIR}/src/TH/cmake
  ${CMAKE_MODULE_PATH})
SET(CMAKE_LIBRARY_PATH /usr/lib/x86_64-linux-gnu/ ${CMAKE_LIBRARY_PATH})

cmake_policy(SET CMP0012 NEW)
# Can be compiled standalone
IF(NOT TH_INSTALL_BIN_SUBDIR
    OR NOT TH_INSTALL_LIB_SUBDIR
    OR NOT TH_INSTALL_INCLUDE_SUBDIR
    OR NOT TH_INSTALL_CMAKE_SUBDIR)

  SET(TH_INSTALL_BIN_SUBDIR "bin" CACHE PATH "TH install binary subdirectory")
  SET(TH_INSTALL_LIB_SUBDIR "lib" CACHE PATH "TH install library subdirectory")
  SET(TH_INSTALL_INCLUDE_SUBDIR "include" CACHE PATH "TH install include subdirectory")
  SET(TH_INSTALL_CMAKE_SUBDIR "share/cmake/TH" CACHE PATH "TH install cmake subdirectory")
ENDIF()

######################################################################
###### macros section
#####################################################################
IF(NOT ADD_TORCH_LIBRARY)
MACRO(ADD_TORCH_LIBRARY package type src)
  IF ("${type}" STREQUAL "STATIC")
    if ("${src}" MATCHES "cu$" OR "${src}" MATCHES "cu;")
      CUDA_ADD_LIBRARY(${package} STATIC ${src})
    else()
      ADD_LIBRARY(${package} STATIC ${src})
    endif()
  ELSE()
    if ("${src}" MATCHES "cu$" OR "${src}" MATCHES "cu;")
      CUDA_ADD_LIBRARY(${package} ${type} ${src})
    else()
      ADD_LIBRARY(${package} ${type} ${src})
    endif()
  ENDIF()
ENDMACRO()
ENDIF()

#######################################################################
##### flags section
######################################################################

IF(NOT MSVC)
  set(CMAKE_CXX_FLAGS "--std=c++11 -Wall -Wno-unknown-pragmas -Wno-vla -fexceptions ${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "-fexceptions ${CMAKE_C_FLAGS}")
ENDIF(NOT MSVC)


# Top-level build config
############################################
# Flags
# When using MSVC

# Detect CUDA architecture and get best NVCC flags
# finding cuda must be first because other things depend on the result
IF(NOT CUDA_FOUND)
  FIND_PACKAGE(CUDA 5.5)
ENDIF()

IF(MSVC)
  # we want to respect the standard, and we are bored of those **** .
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE=1)
  LIST(APPEND CUDA_NVCC_FLAGS "-Xcompiler /wd4819 -Xcompiler /wd4503")
  ADD_DEFINITIONS(-DTH_EXPORTS)
  IF (NOT NO_CUDA)
    ADD_DEFINITIONS(-DTHC_EXPORTS)
  ENDIF()
ENDIF(MSVC)

IF (NOT MSVC)
  IF (CMAKE_VERSION VERSION_LESS "3.1")
    SET(CMAKE_C_FLAGS "-std=c11 ${CMAKE_C_FLAGS}")
  ELSE ()
    SET(CMAKE_C_STANDARD 11)
  ENDIF ()
ENDIF(NOT MSVC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
    if(CUDA_VERSION VERSION_LESS "8.0")
      MESSAGE(STATUS "Found gcc >=5 and CUDA <= 7.5, adding workaround C++ flags")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORCE_INLINES -D_MWAITXINTRIN_H_INCLUDED -D__STRICT_ANSI__")
    endif(CUDA_VERSION VERSION_LESS "8.0")
  endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

LIST(APPEND CUDA_NVCC_FLAGS -Wno-deprecated-gpu-targets)
LIST(APPEND CUDA_NVCC_FLAGS --expt-extended-lambda)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  SET(CMAKE_CXX_STANDARD 11)
endif()

IF(NOT COMMAND CUDA_SELECT_NVCC_ARCH_FLAGS)
  INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/select_compute_arch.cmake)
ENDIF()
LIST(APPEND CUDA_NVCC_FLAGS $ENV{TORCH_NVCC_FLAGS})
CUDA_SELECT_NVCC_ARCH_FLAGS(NVCC_FLAGS_EXTRA $ENV{TORCH_CUDA_ARCH_LIST})
LIST(APPEND CUDA_NVCC_FLAGS ${NVCC_FLAGS_EXTRA})
IF(CMAKE_POSITION_INDEPENDENT_CODE AND NOT MSVC)
  LIST(APPEND CUDA_NVCC_FLAGS "-Xcompiler -fPIC")
ENDIF()

IF(CUDA_HAS_FP16 OR NOT ${CUDA_VERSION} LESS 7.5)
  MESSAGE(STATUS "Found CUDA with FP16 support, compiling with torch.CudaHalfTensor")
  LIST(APPEND CUDA_NVCC_FLAGS "-DCUDA_HAS_FP16=1 -D__CUDA_NO_HALF_OPERATORS__")
  SET(CMAKE_C_FLAGS "-DCUDA_HAS_FP16=1 ${CMAKE_C_FLAGS}")
ELSE(CUDA_HAS_FP16 OR NOT ${CUDA_VERSION} LESS 7.5)
  MESSAGE(STATUS "Could not find CUDA with FP16 support, compiling without torch.CudaHalfTensor")
ENDIF(CUDA_HAS_FP16 OR NOT ${CUDA_VERSION} LESS 7.5)

OPTION(NDEBUG "disable asserts (WARNING: this may result in silent UB e.g. with out-of-bound indices)")
IF(NOT NDEBUG)
  MESSAGE(STATUS "Removing -DNDEBUG from compile flags")
  STRING(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS "" ${CMAKE_C_FLAGS})
  STRING(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_DEBUG "" ${CMAKE_C_FLAGS_DEBUG})
  STRING(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "" ${CMAKE_C_FLAGS_RELEASE})
  STRING(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS "" ${CMAKE_CXX_FLAGS})
  STRING(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_DEBUG "" ${CMAKE_CXX_FLAGS_DEBUG})
  STRING(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "" ${CMAKE_CXX_FLAGS_RELEASE})
ENDIF()


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

IF (WITH_OPENMP AND NOT CHECKED_OPENMP)
  FIND_PACKAGE(OpenMP)
  SET(CHECKED_OPENMP ON CACHE BOOL "already checked for OpenMP")

  # OPENMP_FOUND is not cached in FindOpenMP.cmake (all other variables are cached)
  # see https://github.com/Kitware/CMake/blob/master/Modules/FindOpenMP.cmake
  SET(OPENMP_FOUND ${OPENMP_FOUND} CACHE BOOL "OpenMP Support found")
ENDIF (WITH_OPENMP AND NOT CHECKED_OPENMP)

IF(OPENMP_FOUND)
  MESSAGE(STATUS "Compiling with OpenMP support")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
ENDIF(OPENMP_FOUND)


SET(CUDA_ATTACH_VS_BUILD_RULE_TO_CUDA_FILE OFF)

FIND_PACKAGE(MAGMA)
IF(CUDA_FOUND AND MAGMA_FOUND)
  INCLUDE_DIRECTORIES("${MAGMA_INCLUDE_DIR}")
  SET(CMAKE_REQUIRED_INCLUDES "${MAGMA_INCLUDE_DIR};${CUDA_INCLUDE_DIRS}")
  INCLUDE(CheckPrototypeDefinition)
  check_prototype_definition(magma_get_sgeqrf_nb
   "magma_int_t magma_get_sgeqrf_nb( magma_int_t m, magma_int_t n );"
   "0"
   "magma.h"
    MAGMA_V2)
  IF (MAGMA_V2)
    add_definitions(-DMAGMA_V2)
  ENDIF (MAGMA_V2)

  SET(USE_MAGMA 1)
  MESSAGE(STATUS "Compiling with MAGMA support")
  MESSAGE(STATUS "MAGMA INCLUDE DIRECTORIES: ${MAGMA_INCLUDE_DIR}")
  MESSAGE(STATUS "MAGMA LIBRARIES: ${MAGMA_LIBRARIES}")
  MESSAGE(STATUS "MAGMA V2 check: ${MAGMA_V2}")
ELSE()
  MESSAGE(STATUS "MAGMA not found. Compiling without MAGMA support")
ENDIF()

# ARM specific flags
FIND_PACKAGE(ARM)
IF (ASIMD_FOUND)
  MESSAGE(STATUS "asimd/Neon found with compiler flag : -D__NEON__")
  SET(CMAKE_C_FLAGS "-D__NEON__ ${CMAKE_C_FLAGS}")
ELSEIF (NEON_FOUND)
  MESSAGE(STATUS "Neon found with compiler flag : -mfpu=neon -D__NEON__")
  SET(CMAKE_C_FLAGS "-mfpu=neon -D__NEON__ ${CMAKE_C_FLAGS}")
ENDIF (ASIMD_FOUND)
IF (CORTEXA8_FOUND)
  MESSAGE(STATUS "Cortex-A8 Found with compiler flag : -mcpu=cortex-a8")
  SET(CMAKE_C_FLAGS "-mcpu=cortex-a8 -fprefetch-loop-arrays ${CMAKE_C_FLAGS}")
ENDIF (CORTEXA8_FOUND)
IF (CORTEXA9_FOUND)
  MESSAGE(STATUS "Cortex-A9 Found with compiler flag : -mcpu=cortex-a9")
  SET(CMAKE_C_FLAGS "-mcpu=cortex-a9 ${CMAKE_C_FLAGS}")
ENDIF (CORTEXA9_FOUND)

IF(UNIX)
  # prevent Unknown CMake command "check_function_exists".
  INCLUDE(CheckFunctionExists)
ENDIF(UNIX)

INCLUDE (CheckIncludeFile)
INCLUDE (CheckCSourceCompiles)
INCLUDE (CheckCSourceRuns)

# Check that our programs run.  This is different from the native CMake compiler
# check, which just tests if the program compiles and links.  This is important
# because with ASAN you might need to help the compiled library find some
# dynamic libraries.
CHECK_C_SOURCE_RUNS("
int main() { return 0; }
" COMPILER_WORKS)
IF(NOT COMPILER_WORKS)
  # Force cmake to retest next time around
  unset(COMPILER_WORKS CACHE)
  MESSAGE(FATAL_ERROR
      "Could not run a simple program built with your compiler. "
      "If you are trying to use -fsanitize=address, make sure "
      "libasan is properly installed on your system (you can confirm "
      "if the problem is this by attempting to build and run a "
      "small program.)")
ENDIF()

CHECK_INCLUDE_FILE(cpuid.h HAVE_CPUID_H)
# Check for a cpuid intrinsic
IF(HAVE_CPUID_H)
    CHECK_C_SOURCE_COMPILES("#include <cpuid.h>
        int main()
        {
            unsigned int eax, ebx, ecx, edx;
            return __get_cpuid(0, &eax, &ebx, &ecx, &edx);
        }" HAVE_GCC_GET_CPUID)
ENDIF()
IF(HAVE_GCC_GET_CPUID)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_GCC_GET_CPUID")
ENDIF(HAVE_GCC_GET_CPUID)

CHECK_C_SOURCE_COMPILES("#include <stdint.h>
    static inline void cpuid(uint32_t *eax, uint32_t *ebx,
    			 uint32_t *ecx, uint32_t *edx)
    {
      uint32_t a = *eax, b, c = *ecx, d;
      asm volatile ( \"cpuid\" : \"+a\"(a), \"=b\"(b), \"+c\"(c), \"=d\"(d) );
      *eax = a; *ebx = b; *ecx = c; *edx = d;
    }
    int main() {
      uint32_t a,b,c,d;
      cpuid(&a, &b, &c, &d);
      return 0;
    }" NO_GCC_EBX_FPIC_BUG)

IF(NOT NO_GCC_EBX_FPIC_BUG)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_GCC_GET_CPUID")
ENDIF(NOT NO_GCC_EBX_FPIC_BUG)

FIND_PACKAGE(SSE) # checks SSE, AVX and AVX2
IF(C_SSE2_FOUND)
  MESSAGE(STATUS "SSE2 Found")
  SET(CMAKE_C_FLAGS "${C_SSE2_FLAGS} -DUSE_SSE2 ${CMAKE_C_FLAGS}")
ENDIF(C_SSE2_FOUND)
IF(C_SSE4_1_FOUND AND C_SSE4_2_FOUND)
  SET(CMAKE_C_FLAGS "${C_SSE4_1_FLAGS} -DUSE_SSE4_1 ${C_SSE4_2_FLAGS} -DUSE_SSE4_2 ${CMAKE_C_FLAGS}")
ENDIF()
IF(C_SSE3_FOUND)
  MESSAGE(STATUS "SSE3 Found")
  SET(CMAKE_C_FLAGS "${C_SSE3_FLAGS} -DUSE_SSE3 ${CMAKE_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "${C_SSE3_FLAGS} -DUSE_SSE3 ${CMAKE_CXX_FLAGS}")
ENDIF(C_SSE3_FOUND)

# we don't set -mavx and -mavx2 flags globally, but only for specific files
# however, we want to enable the AVX codepaths, so we still need to
# add USE_AVX and USE_AVX2 macro defines
IF(C_AVX_FOUND)
  MESSAGE(STATUS "AVX Found")
  SET(CMAKE_C_FLAGS "-DUSE_AVX ${CMAKE_C_FLAGS}")
ENDIF(C_AVX_FOUND)
IF(C_AVX2_FOUND)
  MESSAGE(STATUS "AVX2 Found")
  SET(CMAKE_C_FLAGS "-DUSE_AVX2 ${CMAKE_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "-DUSE_AVX2 ${CMAKE_CXX_FLAGS}")
ENDIF(C_AVX2_FOUND)

CHECK_C_SOURCE_RUNS("
#include <stdatomic.h>
// ATOMIC_INT_LOCK_FREE is flaky on some older gcc versions
// so if this define is not usable a preprocessor definition
// we fail this check and fall back to GCC atomics
#if ATOMIC_INT_LOCK_FREE == 2
#define TH_ATOMIC_IPC_REFCOUNT 1
#endif
int main()
{
  int a;
  int oa;
  atomic_store(&a, 1);
  atomic_fetch_add(&a, 1);
  oa = atomic_load(&a);
  if(!atomic_compare_exchange_strong(&a, &oa, 3))
    return -1;
  return 0;
}
" HAS_C11_ATOMICS)

IF(NOT HAS_C11_ATOMICS)
  CHECK_C_SOURCE_RUNS("
#include <intrin.h>
int main()
{
  long a;
  _InterlockedExchange(&a, 1);
  _InterlockedExchangeAdd(&a, 1);
  if(_InterlockedCompareExchange(&a, 3, 2) != 2)
    return -1;
  return 0;
}
" HAS_MSC_ATOMICS)

  CHECK_C_SOURCE_RUNS("
int main()
{
  int a;
  __sync_lock_test_and_set(&a, 1);
  __sync_fetch_and_add(&a, 1);
  if(!__sync_bool_compare_and_swap(&a, 2, 3))
    return -1;
  return 0;
}
" HAS_GCC_ATOMICS)
ENDIF()

#######################################################################
##### sources section
######################################################################

set(extra_src)
IF(C_AVX2_FOUND OR C_AVX_FOUND OR C_SSE4_2_FOUND OR C_SSE4_1_FOUND)
  LIST(APPEND extra_src ${CMAKE_CURRENT_SOURCE_DIR}/generic/simd/convolve.c)
ENDIF(C_AVX2_FOUND OR C_AVX_FOUND OR C_SSE4_2_FOUND OR C_SSE4_1_FOUND)

# IF SSE4 FOUND
IF(C_SSE4_1_FOUND AND C_SSE4_2_FOUND)
  SET(CMAKE_C_FLAGS "${C_SSE4_1_FLAGS} -DUSE_SSE4_1 ${C_SSE4_2_FLAGS} -DUSE_SSE4_2 ${CMAKE_C_FLAGS}")
  IF(MSVC)
    SET_SOURCE_FILES_PROPERTIES(generic/simd/convolve5x5_sse.c PROPERTIES COMPILE_FLAGS "/fp:fast")
  ELSE(MSVC)
    SET_SOURCE_FILES_PROPERTIES(generic/simd/convolve5x5_sse.c PROPERTIES COMPILE_FLAGS "-O3 -ffast-math")
  ENDIF(MSVC)
  LIST(APPEND extra_src ${CMAKE_CURRENT_SOURCE_DIR}/generic/simd/convolve5x5_sse.c)
ENDIF(C_SSE4_1_FOUND AND C_SSE4_2_FOUND)

# IF AVX FOUND
IF(C_AVX_FOUND)
  IF(MSVC)
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/generic/simd/convolve5x5_avx.c PROPERTIES COMPILE_FLAGS "/fp:fast ${C_AVX_FLAGS}")
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX.c PROPERTIES COMPILE_FLAGS "/arch:AVX ${C_AVX_FLAGS}")
  ELSE(MSVC)
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/generic/simd/convolve5x5_avx.c PROPERTIES COMPILE_FLAGS "-O3 -ffast-math ${C_AVX_FLAGS}")
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX.c PROPERTIES COMPILE_FLAGS "-O3 ${C_AVX_FLAGS}")
  ENDIF(MSVC)
  LIST(APPEND extra_src ${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX.c)
  LIST(APPEND extra_src ${CMAKE_CURRENT_SOURCE_DIR}/generic/simd/convolve5x5_avx.c)
ENDIF(C_AVX_FOUND)

IF(C_AVX2_FOUND)
  IF(MSVC)
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2 ${C_AVX2_FLAGS}")
  ELSE(MSVC)
    SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX2.cpp PROPERTIES COMPILE_FLAGS "-O3 ${C_AVX2_FLAGS}")
  ENDIF(MSVC)
  LIST(APPEND extra_src ${CMAKE_CURRENT_SOURCE_DIR}/vector/AVX2.cpp)
ENDIF(C_AVX2_FOUND)

SET(hdr
  THGeneral.h THHalf.h THAllocator.h THSize.h THStorage.h THTensor.h THTensorApply.h THBlas.h THMath.h
  THLapack.h THLogAdd.h THRandom.h THVector.h THAtomic.h )

SET(src
  ${CMAKE_CURRENT_SOURCE_DIR}/THGeneral.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THHalf.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THAllocator.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THSize.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THStorage.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THTensor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/THBlas.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THLapack.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/THLogAdd.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THRandom.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/THFile.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THDiskFile.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THMemoryFile.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THAtomic.c
  ${CMAKE_CURRENT_SOURCE_DIR}/THVector.cpp
  ${extra_src}
)

SET(src ${src} ${hdr} ${simd})

#######################################################################
##### build section
######################################################################

ADD_TORCH_LIBRARY(TH SHARED "${src}")

IF (BUILD_STATIC OR "$ENV{STATIC_TH}" STREQUAL "YES")
  ADD_TORCH_LIBRARY(TH_static STATIC "${src}")
  SET_TARGET_PROPERTIES(TH_static PROPERTIES
    COMPILE_FLAGS "-fPIC")
  SET_TARGET_PROPERTIES(TH_static PROPERTIES
    PREFIX "lib" IMPORT_PREFIX "lib" OUTPUT_NAME "TH")
ENDIF()

IF(NOT TH_SO_VERSION)
  SET(TH_SO_VERSION 0)
ENDIF(NOT TH_SO_VERSION)
MESSAGE(STATUS "TH_SO_VERSION: ${TH_SO_VERSION}")
SET_TARGET_PROPERTIES(TH PROPERTIES
  VERSION   ${TH_SO_VERSION}
  SOVERSION ${TH_SO_VERSION})

IF(HAS_C11_ATOMICS)
  ADD_DEFINITIONS(-DUSE_C11_ATOMICS=1)
  MESSAGE(STATUS "Atomics: using C11 intrinsics")
ELSEIF(HAS_MSC_ATOMICS)
  ADD_DEFINITIONS(-DUSE_MSC_ATOMICS=1)
  MESSAGE(STATUS "Atomics: using MSVC intrinsics")
ELSEIF(HAS_GCC_ATOMICS)
  ADD_DEFINITIONS(-DUSE_GCC_ATOMICS=1)
    MESSAGE(STATUS "Atomics: using GCC intrinsics")
ELSE()
  SET(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  FIND_PACKAGE(Threads)
  IF(THREADS_FOUND)
    ADD_DEFINITIONS(-DUSE_PTHREAD_ATOMICS=1)
    TARGET_LINK_LIBRARIES(TH ${CMAKE_THREAD_LIBS_INIT})
    MESSAGE(STATUS "Atomics: using pthread")
  ENDIF()
ENDIF()

FIND_PACKAGE(BLAS)
IF(BLAS_FOUND)
  SET(USE_BLAS 1)
  IF ($ENV{TH_BINARY_BUILD})
    MESSAGE(STATUS "TH_BINARY_BUILD detected. Enabling special linkage.")
    TARGET_LINK_LIBRARIES(TH "${BLAS_LIBRARIES};${BLAS_LIBRARIES};${BLAS_LIBRARIES}")
  ELSE ($ENV{TH_BINARY_BUILD})
    TARGET_LINK_LIBRARIES(TH ${BLAS_LIBRARIES})
  ENDIF ($ENV{TH_BINARY_BUILD})
  
  IF(BLAS_INFO STREQUAL "mkl")
    ADD_DEFINITIONS(-DTH_BLAS_MKL)
  ENDIF()
ENDIF(BLAS_FOUND)

FIND_PACKAGE(LAPACK)
IF(LAPACK_FOUND)
  SET(USE_LAPACK 1)
  TARGET_LINK_LIBRARIES(TH ${LAPACK_LIBRARIES})
ENDIF(LAPACK_FOUND)

IF (UNIX AND NOT APPLE)
   INCLUDE(CheckLibraryExists)
   # https://github.com/libgit2/libgit2/issues/2128#issuecomment-35649830
   CHECK_LIBRARY_EXISTS(rt clock_gettime "time.h" NEED_LIBRT)
   IF(NEED_LIBRT)
     TARGET_LINK_LIBRARIES(TH rt)
     SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} rt)
   ENDIF(NEED_LIBRT)
ENDIF(UNIX AND NOT APPLE)

IF(UNIX)
  SET(CMAKE_EXTRA_INCLUDE_FILES "sys/mman.h")
  CHECK_FUNCTION_EXISTS(mmap HAVE_MMAP)
  IF(HAVE_MMAP)
    ADD_DEFINITIONS(-DHAVE_MMAP=1)
  ENDIF(HAVE_MMAP)
  # done for lseek: https://www.gnu.org/software/libc/manual/html_node/File-Position-Primitive.html
  ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)
  CHECK_FUNCTION_EXISTS(shm_open HAVE_SHM_OPEN)
  IF(HAVE_SHM_OPEN)
    ADD_DEFINITIONS(-DHAVE_SHM_OPEN=1)
  ENDIF(HAVE_SHM_OPEN)
  CHECK_FUNCTION_EXISTS(shm_unlink HAVE_SHM_UNLINK)
  IF(HAVE_SHM_UNLINK)
    ADD_DEFINITIONS(-DHAVE_SHM_UNLINK=1)
  ENDIF(HAVE_SHM_UNLINK)
  CHECK_FUNCTION_EXISTS(malloc_usable_size HAVE_MALLOC_USABLE_SIZE)
  IF(HAVE_MALLOC_USABLE_SIZE)
    ADD_DEFINITIONS(-DHAVE_MALLOC_USABLE_SIZE=1)
  ENDIF(HAVE_MALLOC_USABLE_SIZE)
ENDIF(UNIX)

IF(NOT MSVC)
  TARGET_LINK_LIBRARIES(TH m)
ENDIF(NOT MSVC)

# Is __thread supported?
IF(NOT MSVC)
  CHECK_C_SOURCE_COMPILES("static __thread int x = 1; int main() { return x; }" C_HAS_THREAD)
ELSE(NOT MSVC)
  CHECK_C_SOURCE_COMPILES("static __declspec( thread ) int x = 1; int main() { return x; }" C_HAS_THREAD)
ENDIF(NOT MSVC)
IF(NOT C_HAS_THREAD)
  MESSAGE(STATUS "Warning: __thread is not supported, generating thread-unsafe code")
ELSE(NOT C_HAS_THREAD)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTH_HAVE_THREAD")
ENDIF(NOT C_HAS_THREAD)

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")
CONFIGURE_FILE(THGeneral.h.in "${CMAKE_CURRENT_BINARY_DIR}/THGeneral.h")


#######################################################################
##### install section
######################################################################

INSTALL(TARGETS TH
  EXPORT TH-exports
  RUNTIME DESTINATION "${TH_INSTALL_BIN_SUBDIR}"
  LIBRARY DESTINATION "${TH_INSTALL_LIB_SUBDIR}"
  ARCHIVE DESTINATION "${TH_INSTALL_LIB_SUBDIR}")

INSTALL(FILES
  TH.h
  THAllocator.h
  THMath.h
  THBlas.h
  THDiskFile.h
  THFile.h
  THFilePrivate.h
  ${CMAKE_CURRENT_BINARY_DIR}/THGeneral.h
  THGenerateAllTypes.h
  THGenerateDoubleType.h
  THGenerateFloatType.h
  THGenerateHalfType.h
  THGenerateLongType.h
  THGenerateIntType.h
  THGenerateShortType.h
  THGenerateCharType.h
  THGenerateByteType.h
  THGenerateFloatTypes.h
  THGenerateIntTypes.h
  THLapack.h
  THLogAdd.h
  THMemoryFile.h
  THRandom.h
  THSize.h
  THStorage.h
  THTensor.h
  THTensorApply.h
  THTensorDimApply.h
  THTensorMacros.h
  THVector.h
  THAtomic.h
  THHalf.h
  DESTINATION "${TH_INSTALL_INCLUDE_SUBDIR}/TH")

INSTALL(FILES
  vector/AVX.h
  vector/AVX2.h
  vector/avx_mathfun.h
  DESTINATION "${TH_INSTALL_INCLUDE_SUBDIR}/TH/vector")

INSTALL(FILES
  generic/THBlas.c
  generic/THBlas.h
  generic/THLapack.cpp
  generic/THLapack.h
  generic/THStorage.c
  generic/THStorage.h
  generic/THStorageCopy.c
  generic/THStorageCopy.h
  generic/THTensor.cpp
  generic/THTensor.h
  generic/THTensorConv.cpp
  generic/THTensorConv.h
  generic/THTensorCopy.c
  generic/THTensorCopy.h
  generic/THTensorLapack.c
  generic/THTensorLapack.h
  generic/THTensorMath.c
  generic/THTensorMath.h
  generic/THTensorRandom.cpp
  generic/THTensorRandom.h
  generic/THVectorDispatch.cpp
  generic/THVector.h
  DESTINATION "${TH_INSTALL_INCLUDE_SUBDIR}/TH/generic")


IF (WIN32 AND NOT CYGWIN)
  SET(BLAS_INSTALL_LIBRARIES "OFF"
    CACHE BOOL "Copy the required BLAS DLLs into the TH install dirs")
ENDIF (WIN32 AND NOT CYGWIN)

MACRO(Install_Required_Library ln)
    get_filename_component(libpath ${ln} PATH)
    get_filename_component(libname ${ln} NAME_WE)
    file(GLOB libdlls "${libpath}/${libname}*.dll")
    install(PROGRAMS ${libdlls}
      DESTINATION "${TH_INSTALL_BIN_SUBDIR}")
ENDMACRO(Install_Required_Library libname)

IF (BLAS_FOUND AND BLAS_INSTALL_LIBRARIES)
  IF (BLAS_goto2_LIBRARY)
    Install_Required_Library(${BLAS_goto2_LIBRARY})
    Install_Required_Library("${libpath}/libgfortran")
    Install_Required_Library("${libpath}/libquadmath")
    Install_Required_Library("${libpath}/libgcc")
  ENDIF()
  IF (BLAS_openblas_LIBRARY)
    Install_Required_Library(${BLAS_openblas_LIBRARY})
    Install_Required_Library("${libpath}/libquadmath")
    Install_Required_Library("${libpath}/libgfortran")
    Install_Required_Library("${libpath}/libgcc")
  ENDIF()
ENDIF()

# Create THConfig.cmake
GET_TARGET_PROPERTY(TH_OUTPUT_NAME TH LOCATION)
GET_FILENAME_COMPONENT(TH_OUTPUT_NAME ${TH_OUTPUT_NAME} NAME)
SET(TH_LIBRARIES "${CMAKE_INSTALL_PREFIX}/${TH_INSTALL_LIB_SUBDIR}/${TH_OUTPUT_NAME}")
SET(TH_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/${TH_INSTALL_INCLUDE_SUBDIR}/TH")
CONFIGURE_FILE(THConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/cmake-exports/THConfig.cmake")
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake-exports/THConfig.cmake"
  DESTINATION "${TH_INSTALL_CMAKE_SUBDIR}")
