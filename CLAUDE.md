# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is ACE Studio's patch fork of [Kiwi](https://github.com/bab2min/Kiwi) (Korean Intelligent Word Identifier), a high-performance Korean morphological analyzer written in C++17. The upstream repository is maintained by bab2min.

### Patches Over Upstream

- **C++ DLL import/export for Windows**: Upstream only provides C API DLL exports; this fork adds proper import/export macros for C++ APIs
- **Crash fixes**: Fixed crashes in `KnLangModel` and `nstSearchSSE2` on SSE2 architectures, and `hasPopcnt()` compilation errors on ARM
- **UTF-8 path handling on Windows**: Fixed `makeFilesystemProvider` and `openFile` in `src/Utils.cpp` and `src/FileUtils.cpp` to use `std::filesystem::u8path()` for proper UTF-8 path support on Windows (upstream uses raw `std::ifstream` which fails with non-ASCII paths)

> **Note for upstream updates**: When rebasing onto a new upstream version, verify if these patches is still needed. The upstream code may regress these fixes.

## Build Commands

### Prerequisites
```bash
git lfs pull                           # Pull model files stored in LFS
git submodule sync
git submodule update --init --recursive
```

### Build (Linux/macOS)
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
make -j$(nproc)
```

### Key CMake Options
- `KIWI_USE_MIMALLOC` - Use mimalloc allocator (default: ON)
- `KIWI_USE_CPUINFO` - Dynamic CPU dispatching (default: ON)
- `KIWI_BUILD_TEST` - Build test suite (default: ON)
- `KIWI_BUILD_CLI` - Build CLI tool (default: ON)
- `KIWI_JAVA_BINDING` - Build Java binding (default: OFF)
- `KIWI_CPU_ARCH` - Override CPU architecture for SIMD selection (required for cross-compilation, e.g., `x86_64` or `arm64`)

### Windows
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
cmake --build . --config RelWithDebInfo
```

### macOS Universal Binary (for ACE Studio)

Build for both architectures with deployment target 13.0, then combine with lipo.

> **Important**: When cross-compiling for x86_64 on Apple Silicon, you must explicitly set `-DKIWI_CPU_ARCH=x86_64`. This is because `CMAKE_SYSTEM_PROCESSOR` always returns the host architecture (`arm64`), but Kiwi's CMakeLists.txt uses this to select architecture-specific SIMD implementations. Without this flag, the x86_64 build will incorrectly use ARM NEON code paths.

```bash
# Build for ARM64
mkdir build-arm && cd build-arm
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
      ../
make -j$(sysctl -n hw.ncpu)
cd ..

# Build for x86_64 (note: KIWI_CPU_ARCH is required for cross-compilation)
mkdir build-x86 && cd build-x86
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
      -DKIWI_CPU_ARCH=x86_64 \
      ../
make -j$(sysctl -n hw.ncpu)
cd ..

# Combine into universal binary
lipo -create \
     build-arm/libkiwi.0.22.2.dylib \
     build-x86/libkiwi.0.22.2.dylib \
     -output libkiwi.0.22.2.dylib
```

## Testing

### Run All Tests
```bash
cd build
ctest
# or run directly:
./test/kiwi-test
```

### Test Architecture-Specific Implementations
```bash
KIWI_ARCH_TYPE=none ./kiwi-cli-* -m ./models/base -e test.txt
KIWI_ARCH_TYPE=sse2 ./kiwi-cli-* -m ./models/base -e test.txt
KIWI_ARCH_TYPE=avx2 ./kiwi-cli-* -m ./models/base -e test.txt
```

### Run Evaluator (Performance Benchmark)
```bash
./kiwi-evaluator --model ./models/base ./eval_data/* --largest
./kiwi-evaluator --model ./models/base ./eval_data/* --largest --typo 6
```

## Architecture

### Core Components

**Morphological Analysis Flow:**
1. **SwTokenizer** (`src/SwTokenizer.cpp`) - Sentence/word tokenization
2. **KTrie** (`src/KTrie.cpp`) - Trie-based dictionary for morpheme lookup
3. **WordDetector** (`src/WordDetector.cpp`) - Unknown/OOV word detection
4. **Language Models** - Disambiguation:
   - `Knlm.cpp` - N-gram language model
   - `SkipBigramModel.cpp` - Long-distance context
   - `CoNgramModel.cpp` - Context-sensitive n-gram
5. **Combiner** (`src/Combiner.cpp`) - Grammatical rule application
6. **TypoTransformer** (`src/TypoTransformer.cpp`) - Optional typo correction

**Main Classes:**
- `Kiwi` / `KiwiBuilder` (`include/kiwi/Kiwi.h`) - Main API entry points
- `Morpheme`, `Form` - Core data structures

### SIMD Implementations (`src/archImpl/`)

Architecture-specific optimizations with dynamic CPU dispatching:
- `sse2.cpp`, `sse4_1.cpp` - 128-bit SIMD
- `avx2.cpp` - 256-bit SIMD with quantized GEMM
- `avx512bw.cpp` - 512-bit SIMD
- `avx_vnni.cpp` - Vector Neural Network Instructions
- `neon.cpp` - ARM SIMD
- `none.cpp` - Portable fallback

### Public APIs

**C++ API** (`include/kiwi/Kiwi.h`):
```cpp
kiwi::KiwiBuilder builder(modelPath, numThreads, options);
auto kiwi = builder.build();
auto result = kiwi->analyze(text);
```

**C API** (`include/kiwi/capi.h`):
```c
kiwi_builder_h builder = kiwi_builder_init(modelPath, numThreads, options);
kiwi_h kiwi = kiwi_builder_build(builder, NULL, 0);
kiwi_res_h result = kiwi_analyze(kiwi, text);
```

## Key Directories

- `include/kiwi/` - Public headers
- `src/` - Core implementation
- `src/archImpl/` - CPU-specific SIMD implementations
- `src/capi/` - C API bindings
- `test/` - Google Test suite
- `bindings/` - Java and WASM bindings
- `tools/` - CLI tools (runner, evaluator, model builder)
- `models/` - Model files (Git LFS)
- `eval_data/` - Evaluation corpus

## Development Notes

- C++17 standard required
- Model files are stored in Git LFS - run `git lfs pull` after cloning
- Use `KIWI_ARCH_TYPE` environment variable to override CPU architecture detection
- POS tags follow Sejong corpus conventions with extensions (W_URL, W_EMAIL, W_HASHTAG, etc.)
- Irregular forms marked with `-R` (regular) and `-I` (irregular) suffixes
- **Always run tests after patching**: When modifying any logic-related code (especially in `src/`, `src/archImpl/`, or language model files), always build and run the full test suite (`kiwi-test`) before committing to catch regressions early
