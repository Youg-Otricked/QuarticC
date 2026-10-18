#ifndef MAIN_H
#define MAIN_H
#include <string>
#include <vector>
#include <unordered_map>
#include "nodes.h"
#include "translate.h"
extern "C" const char _binary_runtime_ll_start[];
extern "C" const size_t _binary_runtime_ll_size;
namespace run {
enum class WarningLevel { Disabled, None, Warning, Error };
struct RunConfig {
    bool use_context = true;
    bool looser_types = false;
    bool print_ast = false;
    bool print_tokens = false;
    bool show_time = false;
    bool quiet_mode = false;
    bool raw = false;
    bool bst = false;
    bool compile_mode = true;
    bool interpret_mode = false;
    bool compile_only = false;
    bool object_only = false;
    bool debug = false;
    bool optimize = true;
    std::string opt_level = "O2";
    std::string output_file = "a.out";
    bool output_wasm = false;
    bool use_runtime = true;
    bool dump_tokens = false;
    std::string target = "";
    std::unordered_map<std::string, std::string> aliases = {};
    std::unordered_map<std::string, std::string> dir_aliases = {};
    std::vector<std::string> library_search_paths = {"."};
    std::vector<std::string> libraries = {};
    std::vector<std::string> link_with = {};
    std::unordered_map<std::string, std::string> definitions = {
#ifdef _WIN32
        {"_WIN32", "1"},
#endif
#ifdef _WIN64
        {"_WIN64", "1"},
#endif
#ifdef __APPLE__
        {"__APPLE__", "1"},
#endif
#ifdef __MACH__
        {"__MACH__", "1"},
#endif
#ifdef __linux__
        {"__linux__", "1"},
#endif
#ifdef __linux
        {"__linux", "1"},
#endif
#ifdef __ANDROID__
        {"__ANDROID__", "1"},
#endif
#ifdef __FreeBSD__
        {"__FreeBSD__", "1"},
#endif
#ifdef __OpenBSD__
        {"__OpenBSD__", "1"},
#endif
#ifdef __NetBSD__
        {"__NetBSD__", "1"},
#endif
#ifdef __unix__
        {"__unix__", "1"},
#endif
#ifdef __unix
        {"__unix", "1"},
#endif
#ifdef __i386__
        {"__i386__", "1"},
#endif
#ifdef __i386
        {"__i386", "1"},
#endif
#ifdef __x86_64__
        {"__x86_64__", "1"},
#endif
#ifdef __x86_64
        {"__x86_64", "1"},
#endif
#ifdef _M_IX86
        {"_M_IX86", "1"},
#endif
#ifdef _M_X64
        {"_M_X64", "1"},
#endif
#ifdef __arm__
        {"__arm__", "1"},
#endif
#ifdef __aarch64__
        {"__aarch64__", "1"},
#endif
#ifdef _M_ARM
        {"_M_ARM", "1"},
#endif
#ifdef _M_ARM64
        {"_M_ARM64", "1"},
#endif
#ifdef __riscv
        {"__riscv", "1"},
#endif
#ifdef __riscv_xlen
        {"__riscv_xlen", "1"},
#endif
#ifdef __wasm32__
        {"__wasm32__", "1"},
#endif
#ifdef __wasm64__
        {"__wasm64__", "1"},
#endif
#ifdef __powerpc__
        {"__powerpc__", "1"},
#endif
#ifdef __powerpc64__
        {"__powerpc64__", "1"},
#endif
#ifdef __powerpc64le__
        {"__powerpc64le__", "1"},
#endif
#ifdef __mips__
        {"__mips__", "1"},
#endif
#ifdef __mips64
        {"__mips64", "1"},
#endif
        {"__quarticc", "\"x1.0.48R\""}};
    bool progress = false;
    std::unordered_map<std::string, WarningLevel> warnings;
    bool translate_c = false;
    bool fci = true;
    std::vector<std::string> c_args;
    std::vector<std::string> link_o_files;
    bool prepro_only = false;
    bool dynamic = false;
    std::vector<std::string> include_dirs;
    bool debug_symbols = false;
};
Mer run(std::string file, std::string text, RunConfig config);
} // namespace Run
#endif

