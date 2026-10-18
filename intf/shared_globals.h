#ifndef GLOBALS_H
#define GLOBALS_H
#include "parser.h"
#include <string>
#include <vector>
#include <unordered_map>
inline bool loose;
inline std::vector<std::string> to_link;
inline std::vector<std::string> to_link_dir;
inline std::unordered_map<std::string, std::string> aliases;
inline std::unordered_map<std::string, std::string> dir_aliases;
inline std::unordered_map<std::string, std::string> definitions;
inline std::vector<std::string> include_paths;
inline bool isHeader = false;
inline bool no_main = false;
inline std::string entrypointName = "main";
#endif

