################
# BigVector allocator benchmark (fix/knlm-bigalloc-std-vector)
add_executable(kiwi-bigalloc-bench bench_bigalloc.cpp)
add_dependencies(kiwi-bigalloc-bench ${CMAKE_PROJECT_NAME}_static)
target_link_libraries(kiwi-bigalloc-bench "${CMAKE_PROJECT_NAME}_static")
target_compile_definitions(kiwi-bigalloc-bench PRIVATE KIWI_STATIC)

# Mirror the main target's mimalloc wiring so the micro-benchmark can compare
# std::vector vs mi_stl_allocator in the same binary, and so BigVector resolves identically.
if(KIWI_USE_MIMALLOC)
  target_compile_definitions(kiwi-bigalloc-bench PRIVATE KIWI_USE_MIMALLOC)
  target_include_directories(kiwi-bigalloc-bench PRIVATE ${CMAKE_SOURCE_DIR}/third_party/mimalloc/include)
endif()
