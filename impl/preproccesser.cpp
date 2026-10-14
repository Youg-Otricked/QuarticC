#include "preproccesser.h"
#include "shared_globals.h"
#include "translate.h"
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}
std::string resolve_path(const std::string& current_file, const std::string& include_path) {
    std::filesystem::path current(current_file);
    std::filesystem::path include(include_path);
    if (include.is_absolute()) { return include.string(); }
    std::filesystem::path resolved = current.parent_path() / include;
    return std::filesystem::weakly_canonical(resolved).string();
}
static const std::vector<std::string>& system_include_dirs() {
    static std::vector<std::string> dirs;
    static bool loaded = false;
    if (loaded) return dirs;
    loaded = true;
    if (FILE* p = popen("cc -E -Wp,-v -xc /dev/null 2>&1", "r")) {
        std::string out;
        char buf[512];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
        pclose(p);
        std::istringstream in(out);
        std::string line;
        bool inList = false;
        while (std::getline(in, line)) {
            if (line.find("#include <...> search starts here:") != std::string::npos) {
                inList = true;
                continue;
            }
            if (line.find("End of search list.") != std::string::npos) break;
            if (!inList || line.empty() || line[0] != ' ') continue;
            std::string d = trim(line);
            auto fw = d.find(" (framework directory)");
            if (fw != std::string::npos) continue;
            dirs.push_back(d);
        }
    }
    if (dirs.empty()) { dirs = {"/usr/local/include", "/usr/include"}; }
    return dirs;
}
static std::string find_c_header(const std::string& relative_guess, const std::string& name) {
    if (std::filesystem::exists(relative_guess)) return relative_guess;
    for (const auto& d : system_include_dirs()) {
        std::filesystem::path cand = std::filesystem::path(d) / name;
        if (std::filesystem::exists(cand)) return cand.string();
    }
    return relative_guess;
}
PreprocessResult preprocess_includes(std::string& source, const std::string& current_file, const std::vector<std::string>& c_args, bool fci) {
    PreprocessResult res;
    res.accessible_namespaces.clear();
    res.accessible_namespaces.insert("Exported");
    res.accessible_namespaces.insert("");
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::vector<std::string>> namespace_depends;
    std::unordered_map<std::string, std::vector<std::string>> from_where;
    std::vector<std::string> to_c_include;
    size_t ep_pos = source.find("#entrypoint");
    size_t nomain_pos = source.find("#nomain");
    if (ep_pos != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < ep_pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, ep_pos)) { in_string = true; }
        if (!in_string) {
            size_t line_end = source.find('\n', ep_pos);
            if (line_end == std::string::npos) line_end = source.size();
            std::string directive = source.substr(ep_pos + 12, line_end - ep_pos - 12);
            entrypointName = trim(directive);
        }
    }
    if (nomain_pos != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < nomain_pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, nomain_pos)) { in_string = true; }
        if (!in_string) { no_main = true; }
    }
    std::function<std::string(const std::string&)> substituteDefined = [&](const std::string& expr) -> std::string {
        std::string out;
        size_t i = 0;
        while (i < expr.size()) {
            char c = expr[i];
            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t start = i;
                while (i < expr.size() && (std::isalnum((unsigned char)expr[i]) || expr[i] == '_')) i++;
                std::string ident = expr.substr(start, i - start);
                if (ident == "defined") {
                    size_t j = i;
                    while (j < expr.size() && std::isspace((unsigned char)expr[j])) j++;
                    bool paren = false;
                    if (j < expr.size() && expr[j] == '(') {
                        paren = true;
                        j++;
                        while (j < expr.size() && std::isspace((unsigned char)expr[j])) j++;
                    }
                    size_t nameStart = j;
                    while (j < expr.size() && (std::isalnum((unsigned char)expr[j]) || expr[j] == '_')) j++;
                    std::string name = expr.substr(nameStart, j - nameStart);
                    if (paren) {
                        while (j < expr.size() && std::isspace((unsigned char)expr[j])) j++;
                        if (j < expr.size() && expr[j] == ')') j++;
                    }
                    out += definitions.count(name) ? "1" : "0";
                    i = j;
                } else {
                    auto it = definitions.find(ident);
                    out += (it != definitions.end()) ? it->second : "0";
                }
            } else {
                out += c;
                i++;
            }
        }
        return out;
    };
    std::string estr;
    size_t epos = 0;
    std::function<void()> eSkipWs = [&]() {
        while (epos < estr.size() && std::isspace((unsigned char)estr[epos])) epos++;
    };
    std::function<long long()> eOr, eAnd, eEq, eRel, eAdd, eMul, eUnary, ePrim;
    ePrim = [&]() -> long long {
        eSkipWs();
        if (epos < estr.size() && estr[epos] == '(') {
            epos++;
            long long v = eOr();
            eSkipWs();
            if (epos < estr.size() && estr[epos] == ')') epos++;
            return v;
        }
        size_t start = epos;
        while (epos < estr.size() && std::isdigit((unsigned char)estr[epos])) epos++;
        if (epos == start) return 0;
        return std::stoll(estr.substr(start, epos - start));
    };
    eUnary = [&]() -> long long {
        eSkipWs();
        if (epos < estr.size() && estr[epos] == '!') {
            epos++;
            return eUnary() == 0 ? 1 : 0;
        }
        if (epos < estr.size() && estr[epos] == '-') {
            epos++;
            return -eUnary();
        }
        return ePrim();
    };
    eMul = [&]() -> long long {
        long long l = eUnary();
        eSkipWs();
        while (epos < estr.size() && (estr[epos] == '*' || estr[epos] == '/')) {
            char op = estr[epos];
            epos++;
            long long r = eUnary();
            l = op == '*' ? l * r : (r != 0 ? l / r : 0);
            eSkipWs();
        }
        return l;
    };
    eAdd = [&]() -> long long {
        long long l = eMul();
        eSkipWs();
        while (epos < estr.size() && (estr[epos] == '+' || estr[epos] == '-')) {
            char op = estr[epos];
            epos++;
            long long r = eMul();
            l = op == '+' ? l + r : l - r;
            eSkipWs();
        }
        return l;
    };
    eRel = [&]() -> long long {
        long long l = eAdd();
        eSkipWs();
        while (epos < estr.size() && (estr[epos] == '<' || estr[epos] == '>')) {
            bool lt = estr[epos] == '<';
            bool orEq = false;
            epos++;
            if (epos < estr.size() && estr[epos] == '=') {
                orEq = true;
                epos++;
            }
            long long r = eAdd();
            if (lt)
                l = orEq ? (l <= r ? 1 : 0) : (l < r ? 1 : 0);
            else
                l = orEq ? (l >= r ? 1 : 0) : (l > r ? 1 : 0);
            eSkipWs();
        }
        return l;
    };
    eEq = [&]() -> long long {
        long long l = eRel();
        eSkipWs();
        while (epos + 1 < estr.size() && ((estr[epos] == '=' && estr[epos + 1] == '=') || (estr[epos] == '!' && estr[epos + 1] == '='))) {
            bool eq = estr[epos] == '=';
            epos += 2;
            long long r = eRel();
            l = eq ? (l == r ? 1 : 0) : (l != r ? 1 : 0);
            eSkipWs();
        }
        return l;
    };
    eAnd = [&]() -> long long {
        long long l = eEq();
        eSkipWs();
        while (epos + 1 < estr.size() && estr[epos] == '&' && estr[epos + 1] == '&') {
            epos += 2;
            long long r = eEq();
            l = (l != 0 && r != 0) ? 1 : 0;
            eSkipWs();
        }
        return l;
    };
    eOr = [&]() -> long long {
        long long l = eAnd();
        eSkipWs();
        while (epos + 1 < estr.size() && estr[epos] == '|' && estr[epos + 1] == '|') {
            epos += 2;
            long long r = eAnd();
            l = (l != 0 || r != 0) ? 1 : 0;
            eSkipWs();
        }
        return l;
    };
    auto evalIf = [&](const std::string& rawExpr) -> bool {
        estr = substituteDefined(rawExpr);
        epos = 0;
        return eOr() != 0;
    };
    size_t pos = 0;
    while (true) {
        size_t define_pos = source.find("#define", pos);
        size_t undef_pos = source.find("#undef", pos);
        size_t if_pos = source.find("#if", pos); // catches #if, #ifdef, #ifndef
        size_t directive_pos = std::min({define_pos, undef_pos, if_pos});
        if (directive_pos == std::string::npos) break;
        bool in_string = false;
        for (size_t check = 0; check < directive_pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (directive_pos == if_pos) {
            bool isIfndef = source.compare(directive_pos, 7, "#ifndef") == 0;
            bool isIfdef = !isIfndef && source.compare(directive_pos, 6, "#ifdef") == 0;
            size_t keyword_len = isIfndef ? 7 : (isIfdef ? 6 : 3);
            if (in_string) {
                pos = directive_pos + keyword_len;
                continue;
            }
            size_t line_end = source.find('\n', directive_pos);
            if (line_end == std::string::npos) line_end = source.size();
            std::string directiveText = trim(source.substr(directive_pos + keyword_len, line_end - directive_pos - keyword_len));
            bool condition;
            if (isIfdef)
                condition = definitions.count(directiveText) > 0;
            else if (isIfndef)
                condition = definitions.count(directiveText) == 0;
            else
                condition = evalIf(directiveText);
            MatchResult m = findMatch(source, line_end + 1);
            if (m.endif_pos == std::string::npos) { throw std::runtime_error("unterminated #if/#ifdef/#ifndef — missing #endif"); }
            std::string keptRegion;
            if (condition) {
                size_t thenEnd = (m.else_pos != std::string::npos) ? m.else_pos : m.endif_pos;
                keptRegion = source.substr(line_end + 1, thenEnd - (line_end + 1));
            } else if (m.else_pos != std::string::npos) {
                size_t elseLineEnd = source.find('\n', m.else_pos);
                if (elseLineEnd == std::string::npos)
                    elseLineEnd = source.size();
                else
                    elseLineEnd++;
                keptRegion = source.substr(elseLineEnd, m.endif_pos - elseLineEnd);
            }
            size_t endifLineEnd = source.find('\n', m.endif_pos);
            if (endifLineEnd == std::string::npos)
                endifLineEnd = source.size();
            else
                endifLineEnd++;
            source = source.substr(0, directive_pos) + keptRegion + source.substr(endifLineEnd);
            pos = directive_pos;
            continue;
        }
        bool isDefine = (directive_pos == define_pos);
        size_t keyword_len = isDefine ? 8 : 7;
        size_t line_end = source.find('\n', directive_pos);
        if (line_end == std::string::npos) line_end = source.size();
        if (!in_string) {
            if (isDefine) {
                std::string directive = source.substr(directive_pos + keyword_len, line_end - directive_pos - keyword_len);
                size_t space_pos = directive.find(" ");
                std::string name = directive.substr(0, space_pos);
                std::string value = (space_pos == std::string::npos || trim(directive.substr(space_pos + 1)) == "") ? "1"
                                                                                                                    : directive.substr(space_pos + 1);
                definitions[name] = value;
            } else {
                std::string name = source.substr(directive_pos + keyword_len, line_end - directive_pos - keyword_len);
                definitions.erase(name);
            }
            source.erase(directive_pos, line_end - directive_pos + (line_end < source.size() ? 1 : 0));
            pos = directive_pos;
        } else {
            pos = directive_pos + keyword_len;
        }
    }
    pos = 0;
    std::string result;
    result.reserve(source.size());
    bool in_string = false;
    size_t i = 0;
    while (i < source.size()) {
        char c = source[i];
        if (c == '"' && (i == 0 || source[i - 1] != '\\')) {
            in_string = !in_string;
            result += c;
            i++;
            continue;
        }
        if (inside_comment(source, i)) { in_string = true; }
        if (!in_string && (std::isalpha((unsigned char)c) || c == '_')) {
            size_t start = i;
            while (i < source.size() && (std::isalnum((unsigned char)source[i]) || source[i] == '_')) { i++; }
            std::string identifier = source.substr(start, i - start);
            auto it = definitions.find(identifier);
            if (it != definitions.end()) {
                result += it->second;
            } else {
                result += identifier;
            }
            continue;
        }
        result += c;
        i++;
    }
    source = result;

    pos = 0;
    while ((pos = source.find("#link", pos)) != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }

        if (inside_comment(source, pos)) { in_string = true; }
        if (in_string) {
            pos++;
            continue;
        }
        size_t start = source.find('<', pos);
        size_t end = source.find('>', start);
        std::string directive = source.substr(start + 1, end - start - 1);
        std::vector<std::string> file_paths = {};
        size_t item_start = 0;
        pos = end + 1;
        while (item_start <= directive.size()) {
            size_t comma = directive.find(',', item_start);
            std::string path = trim(directive.substr(item_start, comma == std::string::npos ? std::string::npos : comma - item_start));
            if (!path.empty()) {
                if (path.front() == '"') path.erase(0, 1);
                if (!path.empty() && path.back() == '"') path.pop_back();
                std::string full_path;
                size_t slash = path.find('/');
                std::string alias = path.substr(0, slash);
                bool looks_like_path = path.find('/') != std::string::npos || path.find('\\') != std::string::npos || path.starts_with(".") ||
                                       std::filesystem::path(path).has_extension();
                if (aliases.contains(path)) {
                    full_path = resolve_path(current_file, aliases[path]);
                } else if (dir_aliases.contains(alias)) {
                    std::string rest = slash == std::string::npos ? "" : path.substr(slash + 1);
                    full_path = resolve_path(current_file, (std::filesystem::path(dir_aliases[alias]) / rest).string());
                } else if (looks_like_path) {
                    full_path = resolve_path(current_file, path);
                } else {
                    full_path = path;
                }
                to_link.push_back(full_path);
            }

            if (comma == std::string::npos) break;
            item_start = comma + 1;
        }
    }
    pos = 0;
    while ((pos = source.find("#searchdir", pos)) != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }

        if (inside_comment(source, pos)) { in_string = true; }
        if (in_string) {
            pos++;
            continue;
        }
        size_t start = source.find('<', pos);
        size_t end = source.find('>', start);
        std::string directive = source.substr(start + 1, end - start - 1);
        std::vector<std::string> file_paths = {};
        size_t item_start = 0;
        pos = end + 1;
        while (item_start <= directive.size()) {
            size_t comma = directive.find(',', item_start);
            std::string path = trim(directive.substr(item_start, comma == std::string::npos ? std::string::npos : comma - item_start));
            if (!path.empty()) {
                if (path.front() == '"') path.erase(0, 1);
                if (!path.empty() && path.back() == '"') path.pop_back();
                std::string full_path;
                size_t slash = path.find('/');
                std::string alias = path.substr(0, slash);
                if (aliases.contains(path)) {
                    full_path = resolve_path(current_file, aliases[path]);
                } else if (dir_aliases.contains(alias)) {
                    std::string rest = slash == std::string::npos ? "" : path.substr(slash + 1);

                    full_path = resolve_path(current_file, (std::filesystem::path(dir_aliases[alias]) / rest).string());
                } else {
                    full_path = resolve_path(current_file, path);
                }
                to_link_dir.push_back(full_path);
            }

            if (comma == std::string::npos) break;
            item_start = comma + 1;
        }
    }
    pos = 0;
    while ((pos = source.find("#depends", pos)) != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (in_string) {
            pos++;
            continue;
        }
        size_t start = source.find('(', pos);
        size_t end = source.find(')', start);
        if (start == std::string::npos || end == std::string::npos) {
            throw std::runtime_error("malformed #depends directive: expected #depends(<namespace>: <deps...>)");
        }
        std::string directive = source.substr(start + 1, end - start - 1);

        size_t colon = directive.find(':');
        if (colon == std::string::npos) { throw std::runtime_error("malformed #depends directive: missing ':' separator"); }
        std::string owner_ns = trim(directive.substr(0, colon));
        std::string rest = directive.substr(colon + 1);
        size_t segment_start = 0;
        size_t comma = rest.find(',', segment_start);
        while (comma != std::string::npos) {
            namespace_depends[owner_ns].push_back(trim(rest.substr(segment_start, comma - segment_start)));
            segment_start = comma + 1;
            comma = rest.find(',', segment_start);
        }
        namespace_depends[owner_ns].push_back(trim(rest.substr(segment_start)));

        pos = end + 1;
    }
    pos = 0;
    while ((pos = source.find("#include", pos)) != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (in_string) {
            pos++;
            continue;
        }
        size_t start = source.find('<', pos);
        size_t end = source.find('>', start);
        std::string directive = source.substr(start + 1, end - start - 1);
        std::vector<std::string> ns_names = {};
        size_t last_comma = directive.find_last_of(',');
        std::string path;
        if (last_comma == std::string::npos) {
            path = trim(directive);
            if (!path.empty() && path.front() == '"') { path = path.substr(1); }
            if (!path.empty() && path.back() == '"') { path = path.substr(0, path.size() - 1); }
            std::string full_path;
            auto slash = path.find('/');
            std::string alias = path.substr(0, slash);
            if (aliases.count(path)) {
                full_path = resolve_path(current_file, aliases[path]);
            } else if (dir_aliases.count(alias)) {
                std::string rest = slash == std::string::npos ? "" : path.substr(slash);
                full_path = resolve_path(current_file, (std::filesystem::path(dir_aliases[alias]) / rest.substr(1)).string());
            } else {
                full_path = resolve_path(current_file, path);
                if (full_path == path) full_path = find_c_header(resolve_path(current_file, path), path);
            }
            to_c_include.push_back(full_path);
            pos = end + 1;
        } else {
            path = trim(directive.substr(last_comma + 1));
            std::string namespaces_part = directive.substr(0, last_comma);
            size_t segment_start = 0;
            size_t comma = namespaces_part.find(',', segment_start);
            while (comma != std::string::npos) {
                ns_names.push_back(trim(namespaces_part.substr(segment_start, comma - segment_start)));
                segment_start = comma + 1;
                comma = namespaces_part.find(',', segment_start);
            }
            ns_names.push_back(trim(namespaces_part.substr(segment_start)));
            if (!path.empty() && path.front() == '"') { path = path.substr(1); }
            if (!path.empty() && path.back() == '"') { path = path.substr(0, path.size() - 1); }
            std::string full_path;
            auto slash = path.find('/');
            std::string alias = path.substr(0, slash);
            if (path == "std") {
                const char* home = std::getenv("QC_STDLIB");
                if (!home) { throw std::runtime_error("QC_STDLIB environment variable not set"); }
                full_path = std::string(home);
            } else if (aliases.count(path)) {
                full_path = resolve_path(current_file, aliases[path]);
            } else if (dir_aliases.count(alias)) {
                std::string rest = slash == std::string::npos ? "" : path.substr(slash);
                full_path = resolve_path(current_file, (std::filesystem::path(dir_aliases[alias]) / rest.substr(1)).string());
            } else {
                full_path = resolve_path(current_file, path);
            }
            if (!from_where.contains(full_path)) { from_where[full_path] = std::vector<std::string>(); }
            for (std::string ns_name : ns_names) { from_where[full_path].push_back(ns_name); }
            dependencies.push_back(full_path);
        }
        pos = end + 1;
    }
    std::string result2 = "";
    size_t last_pos = 0;
    pos = 0;
    while ((pos = source.find("#include", pos)) != std::string::npos) {
        bool in_string = false;
        for (size_t check = 0; check < pos; check++) {
            if (source[check] == '"' && (check == 0 || source[check - 1] != '\\')) { in_string = !in_string; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (in_string) {
            pos++;
            continue;
        }
        result2 += source.substr(last_pos, pos - last_pos);
        size_t end = source.find('>', pos);
        last_pos = end + 1;
        if (last_pos < source.size() && source[last_pos] == '\n') { last_pos++; }
        pos = last_pos;
    }
    result2 += source.substr(last_pos);
    pos = 0;
    while ((pos = result2.find("#link", pos)) != std::string::npos) {
        bool in_str = false;
        for (size_t check = 0; check < pos; check++) {
            if (result2[check] == '"' && (check == 0 || result2[check - 1] != '\\')) { in_str = !in_str; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (!in_str) {
            size_t line_end = result2.find('\n', pos);
            if (line_end == std::string::npos) line_end = result2.size();
            result2.erase(pos, line_end - pos + 1);
        } else {
            pos++;
        }
    }
    pos = 0;
    while ((pos = result2.find("#searchdir", pos)) != std::string::npos) {
        bool in_str = false;
        for (size_t check = 0; check < pos; check++) {
            if (result2[check] == '"' && (check == 0 || result2[check - 1] != '\\')) { in_str = !in_str; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (!in_str) {
            size_t line_end = result2.find('\n', pos);
            if (line_end == std::string::npos) line_end = result2.size();
            result2.erase(pos, line_end - pos + 1);
        } else {
            pos++;
        }
    }
    pos = 0;
    while ((pos = result2.find("#entrypoint", pos)) != std::string::npos) {
        bool in_str = false;
        for (size_t check = 0; check < pos; check++) {
            if (result2[check] == '"' && (check == 0 || result2[check - 1] != '\\')) { in_str = !in_str; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (!in_str) {
            size_t line_end = result2.find('\n', pos);
            if (line_end == std::string::npos) line_end = result2.size();
            result2.erase(pos, line_end - pos + 1);
        } else {
            pos++;
        }
    }
    pos = 0;
    while ((pos = result2.find("#depends", pos)) != std::string::npos) {
        bool in_str = false;
        for (size_t check = 0; check < pos; check++) {
            if (result2[check] == '"' && (check == 0 || result2[check - 1] != '\\')) { in_str = !in_str; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (!in_str) {
            size_t line_end = result2.find('\n', pos);
            if (line_end == std::string::npos) line_end = result2.size();
            result2.erase(pos, line_end - pos + 1);
        } else {
            pos++;
        }
    }
    pos = 0;
    while ((pos = result2.find("#nomain", pos)) != std::string::npos) {
        bool in_str = false;
        for (size_t check = 0; check < pos; check++) {
            if (result2[check] == '"' && (check == 0 || result2[check - 1] != '\\')) { in_str = !in_str; }
        }
        if (inside_comment(source, pos)) { in_string = true; }
        if (!in_str) {
            size_t line_end = result2.find('\n', pos);
            if (line_end == std::string::npos) line_end = result2.size();
            result2.erase(pos, line_end - pos + 1);
        } else {
            pos++;
        }
    }
    std::vector<std::string> translated;
    translated.reserve(to_c_include.size());
    for (std::string& c_header : to_c_include) {
        TranslateCtx context{.header = c_header, .args = c_args};
        Translator translator(&context);
        translator.fci = fci;
        int result = translator.translate_header();
        if (result) {
            for (auto& err : context.errors) { res.errors.push_back({new CTError(err->details, err->pos), "Error"}); }
            continue;
        }
        translated.push_back(std::move(translator.result_file));
    }
    size_t total_size = result2.size();
    for (const auto& s : translated) total_size += s.size();
    std::string output;
    output.reserve(total_size);
    for (auto it = translated.rbegin(); it != translated.rend(); ++it) output += *it;
    output += result2;
    if (!to_c_include.empty()) {
        auto res2 = preprocess_includes(output, current_file, c_args, fci);
        res.errors.insert(res.errors.end(), res2.errors.begin(), res2.errors.end());
        res.clean_source = std::move(res2.clean_source);
    } else
        res.clean_source = std::move(output);
    res.dependency_paths = dependencies;
    res.included_namespaces = from_where;
    res.namespace_depends = namespace_depends;
    return res;
}
