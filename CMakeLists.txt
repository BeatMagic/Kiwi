cmake_minimum_required(VERSION 3.12)

project(kiwi VERSION 0.22.2 DESCRIPTION "Kiwi, Korean Intelligent Word Identifier")

set ( CMAKE_CXX_STANDARD 17 )
set ( CMAKE_VERBOSE_MAKEFILE true )

option(KIWI_USE_MIMALLOC  "Use mimalloc for faster memory allocation" ON)
option(KIWI_USE_CPUINFO  "Use cpuinfo for dynamic CPU dispatching" ON)
option(KIWI_BUILD_DYNAMIC  "Build dynamic library" ON)
option(KIWI_BUILD_CLI  "Build CLI tool" ON)
option(KIWI_BUILD_EVALUATOR  "Build Evaluator" ON)
option(KIWI_BUILD_MODEL_BUILDER  "Build Model Builder" ON)
option(KIWI_BUILD_TEST  "Build Test sets" ON)
option(KIWI_JAVA_BINDING  "Build Java binding" OFF)
set(KIWI_CPU_ARCH "" CACHE STRING "Set architecture type for macOS")


if (NOT CMAKE_BUILD_TYPE)
  if ("${CMAKE_BINARY_DIR}" MATCHES ".*(D|d)ebug$")
    message(STATUS "No build type selected, default to: Debug")
    set(CMAKE_BUILD_TYPE "Debug")
  else()
    message(STATUS "No build type selected, default to: Release")
    set(CMAKE_BUILD_TYPE "Release")
  endif()
endif()

if(NOT KIWI_CPU_ARCH)
  if (EMSCRIPTEN)
    set(KIWI_CPU_ARCH "wasm")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)"
      OR "${ANDROID_ABI}" STREQUAL "x86_64")
    set(KIWI_CPU_ARCH "x86_64")
  elseif (HOST_ARCHITECTURE MATCHES "^arm64" 
      OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)" 
      OR "${ANDROID_ABI}" STREQUAL "arm64-v8a")
    set(KIWI_CPU_ARCH "arm64")
  else()
    set(KIWI_CPU_ARCH "other")
  endif()
  set(KIWI_CPU_ARCH "${KIWI_CPU_ARCH}" PARENT_SCOPE)
endif()


if (KIWI_USE_CPUINFO AND 
  (MSVC OR 
   ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang") AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 11)
  )
)
  set ( AVX_VNNI_SUPPORTED ON )
else()
  set ( AVX_VNNI_SUPPORTED OFF )
endif()

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "${KIWI_CPU_ARCH}")
endif()

set ( CORE_SRCS
  src/ArchUtils.cpp
  src/Combiner.cpp
  src/CoNgramModel.cpp
  src/Dataset.cpp
  src/Form.cpp
  src/FeatureTestor.cpp
  src/FileUtils.cpp
  src/Joiner.cpp
  src/Kiwi.cpp
  src/KiwiBuilder.cpp
  src/Knlm.cpp
  src/KTrie.cpp
  src/PatternMatcher.cpp
  src/search.cpp
  src/ScriptType.cpp
  src/SkipBigramModel.cpp
  src/SubstringExtractor.cpp
  src/SwTokenizer.cpp
  src/TagUtils.cpp
  src/TypoTransformer.cpp
  src/UnicodeCase.cpp
  src/Utils.cpp
  src/WordDetector.cpp
  src/archImpl/none.cpp
)


if(KIWI_USE_MIMALLOC)
  message(STATUS "Use mimalloc allocators")
  set ( ADDITIONAL_FLAGS "-DKIWI_USE_MIMALLOC" )
  include_directories( third_party/mimalloc/include )
  set ( CORE_SRCS "${CORE_SRCS}"
    third_party/mimalloc/src/static.c
  )
endif()


include_directories( include/ )
include_directories( third_party/tclap/include )
include_directories( third_party/cpp-btree )
include_directories( third_party/eigen )
include_directories( third_party/json/include )
include_directories( third_party/streamvbyte/include )
add_subdirectory( third_party/streamvbyte )
set ( STREAMVBYTE_OBJECTS 
  $<TARGET_OBJECTS:streamvbyte>
)
if(KIWI_USE_CPUINFO)
  message(STATUS "Use cpuinfo")
  include_directories( third_party/cpuinfo/include )

  set(CPUINFO_LIBRARY_TYPE "shared" CACHE STRING "")
  set(CPUINFO_BUILD_TOOLS OFF CACHE BOOL "")
  set(CPUINFO_BUILD_UNIT_TESTS OFF CACHE BOOL "")
  set(CPUINFO_BUILD_MOCK_TESTS OFF CACHE BOOL "")
  set(CPUINFO_BUILD_BENCHMARKS OFF CACHE BOOL "")
  add_subdirectory( third_party/cpuinfo )

  set ( ADDITIONAL_FLAGS ${ADDITIONAL_FLAGS} "-DKIWI_USE_CPUINFO" )


  set ( CPUINFO_OBJECTS_STATIC 
    $<TARGET_OBJECTS:cpuinfo_internals>
  )
  set ( CPUINFO_OBJECTS_SHARED 
    $<TARGET_OBJECTS:cpuinfo>
  )
endif()

if (AVX_VNNI_SUPPORTED)
  message(STATUS "AVX-VNNI is supported")
  set ( ADDITIONAL_FLAGS ${ADDITIONAL_FLAGS} "-DKIWI_AVX_VNNI_SUPPORTED" )
endif()

# Let CMake handle defaults (runtime library, optimization, debug info, NDEBUG).
# Only add kiwi-specific extras on top.
if(MSVC)
  add_compile_options(/utf-8 /bigobj)
  add_compile_options(
    $<$<CONFIG:Debug>:-DDEBUG>
    $<$<NOT:$<CONFIG:Debug>>:-Oi>
    $<$<NOT:$<CONFIG:Debug>>:-Gy>
    $<$<NOT:$<CONFIG:Debug>>:-DKIWI_USE_BTREE>
  )
else()
  if(NOT ANDROID)
    link_libraries(pthread)
  endif()

  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

  add_compile_options(
    $<$<CONFIG:Debug>:-DDEBUG>
    $<$<NOT:$<CONFIG:Debug>>:-DKIWI_USE_BTREE>
  )

  if(APPLE)
    add_compile_options(-Wno-unqualified-std-cast-call)
  endif()
endif()

if (KIWI_CPU_ARCH MATCHES "x86_64")
  message("Compiling for x86_64")
  set( CORE_SRCS 
    ${CORE_SRCS}
    src/archImpl/sse2.cpp
    src/archImpl/sse4_1.cpp
  )
  if (KIWI_USE_CPUINFO)
    set( CORE_SRCS 
      ${CORE_SRCS}
      src/archImpl/avx2.cpp
      src/archImpl/avx512bw.cpp
      src/archImpl/avx512vnni.cpp
    )
    # If AVX-VNNI is supported (MSVC, GCC 11+ or Clang 11+)
    if (AVX_VNNI_SUPPORTED)
      set( CORE_SRCS 
        ${CORE_SRCS}
        src/archImpl/avx_vnni.cpp
      )
    endif()
  endif()

  if(MSVC)
    set_source_files_properties(src/archImpl/sse2.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
    set_source_files_properties(src/archImpl/sse4_1.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
    if (KIWI_USE_CPUINFO)
      set_source_files_properties(src/archImpl/avx2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
      set_source_files_properties(src/archImpl/avx_vnni.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
      set_source_files_properties(src/archImpl/avx512bw.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")
      set_source_files_properties(src/archImpl/avx512vnni.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    endif()
  else()
    set_source_files_properties(src/archImpl/sse2.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    set_source_files_properties(src/archImpl/sse4_1.cpp PROPERTIES COMPILE_FLAGS "-msse2 -msse4.1")
    if (KIWI_USE_CPUINFO)
      set_source_files_properties(src/archImpl/avx2.cpp PROPERTIES COMPILE_FLAGS "-mavx -mavx2 -mfma")
      set_source_files_properties(src/archImpl/avx512bw.cpp PROPERTIES COMPILE_FLAGS "-mavx -mavx2 -mfma -mavx512f -mavx512vl -mavx512dq -mavx512bw")
      set_source_files_properties(src/archImpl/avx512vnni.cpp PROPERTIES COMPILE_FLAGS "-mavx -mavx2 -mfma -mavx512f -mavx512vl -mavx512dq -mavx512bw -mavx512vnni")
      if (AVX_VNNI_SUPPORTED)
        set_source_files_properties(src/archImpl/avx_vnni.cpp PROPERTIES COMPILE_FLAGS "-mavx -mavx2 -mfma -mavxvnni")
      endif()
    endif()
  endif()
elseif (KIWI_CPU_ARCH MATCHES "arm64")
  message("Compiling for arm64")
  set( CORE_SRCS 
    ${CORE_SRCS}
    src/archImpl/neon.cpp
  )
  set_source_files_properties(src/archImpl/neon.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a -DKIWI_ARCH_ARM64=1")
elseif (KIWI_CPU_ARCH MATCHES "wasm")
  message("Compiling for wasm")
else()
  message("Compiling for other")
endif()

add_library( "${PROJECT_NAME}_static" STATIC
  ${CORE_SRCS}
  src/capi/kiwi_c.cpp
  ${CPUINFO_OBJECTS_STATIC}
  ${STREAMVBYTE_OBJECTS}
)

install(TARGETS ${PROJECT_NAME}_static PUBLIC_HEADER DESTINATION include/kiwi)

# Install the kiwi library as well as header files to (`include/kiwi` directory)
# so that a user can use it in their own projects that are not cmake projects.
file(GLOB KIWI_INCLUDE_FILES "include/kiwi/*.h" "include/kiwi/*.hpp")

if(KIWI_BUILD_DYNAMIC)
  add_library( "${PROJECT_NAME}" SHARED
    ${CORE_SRCS}
    src/capi/kiwi_c.cpp
    ${CPUINFO_OBJECTS_SHARED}
    ${STREAMVBYTE_OBJECTS}
  )
  set_target_properties("${PROJECT_NAME}" PROPERTIES
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
      PUBLIC_HEADER "${KIWI_INCLUDE_FILES}"
  )

  install(TARGETS ${PROJECT_NAME} PUBLIC_HEADER DESTINATION include/kiwi)
  target_compile_options("${PROJECT_NAME}" PRIVATE "${ADDITIONAL_FLAGS}")
endif()

target_compile_options("${PROJECT_NAME}_static" PRIVATE "${ADDITIONAL_FLAGS}")

# Define KIWI_STATIC for static library builds to ensure KIWI_API is empty
# (neither dllexport nor dllimport)
target_compile_definitions("${PROJECT_NAME}_static" PRIVATE KIWI_STATIC)

#target_link_libraries("${PROJECT_NAME}_static" cpuinfo_internals)
#target_link_libraries("${PROJECT_NAME}" cpuinfo)

if (KIWI_BUILD_CLI)
  add_executable( "${PROJECT_NAME}-cli-${PROJECT_VERSION}"
    tools/runner.cpp
  )

  target_link_libraries( "${PROJECT_NAME}-cli-${PROJECT_VERSION}"
    "${PROJECT_NAME}_static"
  )
endif()

if (KIWI_BUILD_EVALUATOR)
  add_executable( "${PROJECT_NAME}-evaluator"
    tools/Evaluator.cpp
    tools/evaluator_main.cpp
  )

  target_link_libraries( "${PROJECT_NAME}-evaluator"
    "${PROJECT_NAME}_static"
  )
endif()

if (KIWI_BUILD_MODEL_BUILDER)
  add_executable( "${PROJECT_NAME}-model-builder"
    tools/model_builder.cpp
  )

  target_link_libraries( "${PROJECT_NAME}-model-builder"
    "${PROJECT_NAME}_static"
  )
endif()


if(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries( "${PROJECT_NAME}_static"
    rt
  )

  if (KIWI_BUILD_CLI)
    target_link_libraries( "${PROJECT_NAME}-cli-${PROJECT_VERSION}"
      rt
    )
  endif()

  if (KIWI_BUILD_EVALUATOR)
    target_link_libraries( "${PROJECT_NAME}-evaluator"
      rt
    )
  endif()
endif()

if(KIWI_BUILD_DYNAMIC)
  # Define KIWI_EXPORTS when building the DLL so that KIWI_API exports symbols.
  # PRIVATE ensures consumers of the library don't get this definition,
  # allowing KIWI_API to be dllimport for them.
  target_compile_definitions("${PROJECT_NAME}"
    PRIVATE KIWI_EXPORTS
  )
endif()

if(KIWI_BUILD_TEST)
  add_subdirectory( third_party/googletest )
  add_subdirectory( test )
endif()

if(KIWI_JAVA_BINDING)
  add_subdirectory( bindings/java )
endif()

if(EMSCRIPTEN)
  add_subdirectory( bindings/wasm )
endif()