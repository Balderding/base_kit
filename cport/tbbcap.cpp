// tbbcap.cpp (speed plan test (a), 2026-11-22): TBB's worker threads for Manifold, capped like the kit's bases at once
// (half the cores, at most 4; KIT_TBB_THREADS overrides). Called once from mf_join / mf_simplify before Manifold runs.
#include <oneapi/tbb/global_control.h>
#include <thread>
#include <cstdlib>
extern "C" void tbb_cap(void) {
    static oneapi::tbb::global_control *gc = nullptr;
    if (gc) return;
    int n = (int)std::thread::hardware_concurrency() / 2; if (n < 1) n = 1; if (n > 4) n = 4;
    if (const char *e = std::getenv("KIT_TBB_THREADS")) { int v = std::atoi(e); if (v >= 1) n = v; }
    gc = new oneapi::tbb::global_control(oneapi::tbb::global_control::max_allowed_parallelism, (size_t)n);
}
namespace { struct TbbCapInit { TbbCapInit() { tbb_cap(); } } tbb_cap_init; }   // at start-up: no kit source changes
