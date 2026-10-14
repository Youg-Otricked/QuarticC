#ifndef PREPROCCESSER_H
#define PREPROCCESSER_H
#include "nodes.h"
#include <algorithm>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
std::string resolve_path(const std::string& current_file, const std::string& include_path);
struct PreprocessResult {
    std::string clean_source;
    std::vector<std::string> dependency_paths;
    std::unordered_map<std::string, std::vector<std::string>> namespace_depends;
    std::unordered_map<std::string, std::vector<std::string>> included_namespaces;
    std::unordered_set<std::string> accessible_namespaces;
    std::vector<Diagnostic> errors;
};

PreprocessResult preprocess_includes(std::string& source, const std::string& current_file, const std::vector<std::string>& c_args, bool fci);
struct MatchResult {
    size_t else_pos;
    size_t endif_pos;
};
static MatchResult findMatch(const std::string& src, size_t searchStart) {
    int depth = 0;
    size_t elsePos = std::string::npos;
    size_t pos = searchStart;
    while (pos < src.size()) {
        size_t nextIf = src.find("#if", pos);
        size_t nextElse = src.find("#else", pos);
        size_t nextEndif = src.find("#endif", pos);
        size_t next = std::min({nextIf, nextElse, nextEndif});
        if (next == std::string::npos) break;
        if (next == nextIf) {
            depth++;
            pos = next + 3;
        } else if (next == nextElse) {
            if (depth == 0) { elsePos = next; }
            pos = next + 5;
        } else { // nextEndif
            if (depth == 0) { return {elsePos, next}; }
            depth--;
            pos = next + 6;
        }
    }
    return {std::string::npos, std::string::npos};
}
enum class ParserState { Code, String, SingleLineComment, MultiLineComment };
static bool inside_comment(const std::string& source, size_t pos) {
    if (pos >= source.length()) return false;
    ParserState state = ParserState::Code;
    for (size_t i = 0; i < pos; i++) {
        char current = source[i];
        char next = (i + 1 < source.length()) ? source[i + 1] : '\0';
        switch (state) {
        case ParserState::Code:
            if (current == '/' && next == '/') {
                state = ParserState::SingleLineComment;
                i++;
            } else if (current == '/' && next == '*') {
                state = ParserState::MultiLineComment;
                i++;
            } else if (current == '"') {
                state = ParserState::String;
            }
            break;
        case ParserState::String:
            if (current == '\\') {
                i++;
            } else if (current == '"') {
                state = ParserState::Code;
            }
            break;
        case ParserState::SingleLineComment:
            if (current == '\n') { state = ParserState::Code; }
            break;
        case ParserState::MultiLineComment:
            if (current == '*' && next == '/') {
                state = ParserState::Code;
                i++;
            }
            break;
        }
    }
    return (state == ParserState::SingleLineComment || state == ParserState::MultiLineComment);
}
#endif
