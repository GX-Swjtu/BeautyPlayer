#ifdef _WIN32
// Standard hybrid-GPU preference hints. The Windows per-app setting can override
// these; diagnostics always report the renderer actually selected.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
