// Benchmark for the BigVector change (fix/knlm-bigalloc-std-vector).
//
// KnLangModel's large load-time buffers (restored_floats / restored_leaf_ll / d_node_size)
// were Vector<T> = mimalloc. mimalloc uses reserve + optimistic commit for huge objects
// (third_party/mimalloc src/page.c), which on low-memory machines writes past committed
// pages (EXCEPTION_ACCESS_VIOLATION) instead of failing cleanly. They are now BigVector<T>,
// which defaults to std::vector (system allocator). Define KIWI_BIGALLOC_MIMALLOC to switch
// them back to mimalloc for A/B comparison.
//
// Axes:
//   1) micro  : resize+fill large float buffers, std::vector vs mimalloc vector (this build)
//   2) load   : time KiwiBuilder(...).build()  (affected by the BigVector allocator)
//   3) analyze: throughput over a corpus + result fingerprint (hot path; must be unchanged)
//
// Compare two builds (default vs -DKIWI_BIGALLOC_MIMALLOC):
//   - load time   : allocator impact on model loading
//   - analyze tput: should be identical (hot path keeps mimalloc)
//   - fingerprint : MUST be identical (allocator must never change results)

#include <kiwi/Kiwi.h>
#include <kiwi/Types.h>
#include <kiwi/Utils.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace kiwi;
using Clock = std::chrono::steady_clock;
static double msOf(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

template<class Vec>
static double benchAlloc(size_t nFloats, int iters)
{
	volatile float sink = 0;
	auto t0 = Clock::now();
	for (int it = 0; it < iters; ++it)
	{
		Vec v;
		v.resize(nFloats);
		for (size_t i = 0; i < nFloats; i += 1024) v[i] = static_cast<float>(i); // touch every 4KB page
		sink += v[nFloats / 2];
	}
	auto t1 = Clock::now();
	(void)sink;
	return msOf(t1 - t0) / iters;
}

int main(int argc, char** argv)
{
	const char* modelPath = argc > 1 ? argv[1] : "./models/cong/base";
	const char* corpusPath = argc > 2 ? argv[2] : "./eval_data/written.txt";

#ifdef KIWI_USE_MIMALLOC
	const char* mode = "KnLM big buffers = unique_ptr<T[]> (system alloc) ; hot path = mimalloc";
#else
	const char* mode = "KIWI_USE_MIMALLOC off (everything std)";
#endif
	std::printf("=== kiwi bigalloc benchmark | %s ===\n", mode);

	// (1) micro: directly compare allocators in THIS build (load-time large-buffer analog)
	for (size_t mb : { (size_t)8, (size_t)30, (size_t)64 })
	{
		size_t n = mb * 1024 * 1024 / sizeof(float);
		double tStd = benchAlloc<std::vector<float>>(n, 30);
#ifdef KIWI_USE_MIMALLOC
		double tMi = benchAlloc<std::vector<float, mi_stl_allocator<float>>>(n, 30);
		std::printf("[micro] %2zu MB resize+fill : std=%8.3f ms | mimalloc=%8.3f ms | mi/std=%.2fx\n",
			mb, tStd, tMi, tMi / tStd);
#else
		std::printf("[micro] %2zu MB resize+fill : std=%8.3f ms\n", mb, tStd);
#endif
	}

	// (2) load time
	auto l0 = Clock::now();
	Kiwi kiwi = KiwiBuilder{ modelPath, 0, BuildOption::default_, ModelType::none }.build();
	auto l1 = Clock::now();
	std::printf("[load]  KiwiBuilder(\"%s\").build() = %.1f ms\n", modelPath, msOf(l1 - l0));

	// load corpus (one sentence per line, optional TSV: take field 0)
	std::vector<std::u16string> corpus;
	{
		std::ifstream ifs(corpusPath);
		std::string line;
		while (std::getline(ifs, line) && corpus.size() < 5000)
		{
			size_t tab = line.find('\t');
			if (tab != std::string::npos) line = line.substr(0, tab);
			if (!line.empty()) corpus.push_back(utf8To16(line));
		}
	}
	std::printf("[corpus] %zu sentences from %s\n", corpus.size(), corpusPath);
	if (corpus.empty()) { std::printf("!! empty corpus, skipping analyze\n"); return 0; }

	// warmup
	for (size_t i = 0; i < 3 && i < corpus.size(); ++i) kiwi.analyze(corpus[i], Match::allWithNormalizing);

	// (3a) correctness fingerprint (FNV-1a over form+tag+pos+len), one pass
	uint64_t fp = 1469598103934665603ull;
	auto fold = [&fp](uint64_t x) { fp ^= x; fp *= 1099511628211ull; };
	size_t tokenCount = 0;
	for (const auto& s : corpus)
	{
		auto res = kiwi.analyze(s, Match::allWithNormalizing).first;
		for (const auto& t : res)
		{
			for (char16_t c : t.str) fold(c);
			fold(static_cast<uint64_t>(t.tag));
			fold(t.position);
			fold(t.length);
			++tokenCount;
		}
	}

	// (3b) analyze throughput, repeated passes for stable timing
	const int reps = 50;
	volatile size_t sink = 0;
	auto a0 = Clock::now();
	for (int r = 0; r < reps; ++r)
		for (const auto& s : corpus)
			sink += kiwi.analyze(s, Match::allWithNormalizing).first.size();
	auto a1 = Clock::now();
	(void)sink;
	double sec = msOf(a1 - a0) / 1000.0;
	std::printf("[analyze] %zu tokens/pass x%d passes in %.1f ms => %.0f sent/s | %.0f tok/s\n",
		tokenCount, reps, msOf(a1 - a0), corpus.size() * reps / sec, tokenCount * reps / sec);
	std::printf("[correctness] fingerprint = 0x%016llx  (MUST match across both builds)\n",
		static_cast<unsigned long long>(fp));
	return 0;
}
