#ifndef PREPROCCESSER_H
#define PREPROCCESSER_H
#include "nodes.h"
#include <algorithm>
#include <sstream>
#include <string>
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
struct CommentMask {
    std::vector<unsigned char> bits;
    size_t size = 0;
};
enum class ParserState { Code, String, SingleLineComment, MultiLineComment };
static CommentMask build_comment_mask(const std::string& source) {
    CommentMask m;
    m.size = source.size();
    m.bits.assign(source.size() + 1, 0);
    ParserState state = ParserState::Code;
    size_t n = source.size();
    for (size_t i = 0; i < n; i++) {
        m.bits[i] = (state == ParserState::SingleLineComment || state == ParserState::MultiLineComment);
        char current = source[i];
        char next = (i + 1 < n) ? source[i + 1] : '\0';
        switch (state) {
        case ParserState::Code:
            if (current == '/' && next == '/') {
                state = ParserState::SingleLineComment;
                m.bits[i + 1] = 1;
                i++;
            } else if (current == '/' && next == '*') {
                state = ParserState::MultiLineComment;
                m.bits[i + 1] = 1;
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
            if (current == '\n') state = ParserState::Code;
            break;
        case ParserState::MultiLineComment:
            if (current == '*' && next == '/') {
                state = ParserState::Code;
                m.bits[i + 1] = 1;
                i++;
            }
            break;
        }
    }
    return m;
}
static bool inside_comment(const CommentMask& mask, size_t pos) {
    return pos < mask.size && mask.bits[pos];
}
#endif
