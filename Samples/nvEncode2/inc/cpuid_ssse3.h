// cpuid.cpp 
// processor: x86, x64
// Use the __cpuid intrinsic to get information about a CPU
//

bool get_cpuinfo_has_sse3(); // SSE3        (Prescott 2004), haddps
bool get_cpuinfo_has_ssse3();// Streaming SSE3 (Conroe 2006), haddpw, shuffle_epi8
bool get_cpuinfo_has_avx();  // Intel-AVX   (Sandy Bridge 2011)
bool get_cpuinfo_has_avx2(); // Intel-AVX2  (Haswell 2013)