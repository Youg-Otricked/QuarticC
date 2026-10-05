#include "main.h"
#include "nodes.h"
#include "filesystem"
#ifndef __EMSCRIPTEN__
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#endif
#include <llvm/IR/LegacyPassManager.h>
#include <fstream>
#include <iostream>
#include "compiler.h"
#include "shared_globals.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "parser.h"
#include "lexer.h"
#include "errors.h"
#include "preproccesser.h"
#include <functional>
#include <llvm/Support/raw_ostream.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/IR/Verifier.h>
namespace run {
std::string collapse_inline_brackets(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '[') {
            size_t k = out.size();
            char prev = 0;
            while (k > 0) {
                char c = out[k - 1];
                if (!std::isspace((unsigned char)c)) {
                    prev = c;
                    break;
                }
                --k;
            }

            size_t j = i + 1;
            while (j < s.size() && std::isspace((unsigned char)s[j])) j++;

            bool prev_is_value_starter = (prev == '=' || prev == '(' || prev == ',' || prev == '{' || prev == '[');

            if (j < s.size() && s[j] == ']' && prev_is_value_starter) {
                out += "[]";
                i = j;
                continue;
            }
        }

        out += s[i];
    }
    return out;
}
std::string bst_diagram(const std::string& input) {
    std::string s = collapse_inline_brackets(input);

    struct Node {
        std::string val;
        std::vector<Node> children;
    };

    std::function<Node(const std::string&, size_t&)> parse_node;
    parse_node = [&](const std::string& str, size_t& pos) -> Node {
        Node n;
        std::string token;
        while (pos < str.size()) {
            char c = str[pos++];
            if (c == '(' || c == '[' || c == '{') {
                Node child = parse_node(str, pos);
                n.children.push_back(child);
            } else if (c == ')' || c == ']' || c == '}') {
                break;
            } else if (c == ',') {
                if (!token.empty()) {
                    n.children.push_back(Node{token});
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) n.val = token;
        return n;
    };

    size_t idx = 0;
    Node root = parse_node(s, idx);

    struct ASCIINode {
        std::vector<std::string> lines;
        int width = 0;
        int height = 0;
        int middle = 0;
    };

    std::function<ASCIINode(const Node&)> build;
    build = [&](const Node& n) -> ASCIINode {
        ASCIINode res;
        res.lines.push_back(n.val);
        res.width = n.val.size();
        res.height = 1;
        res.middle = res.width / 2;

        if (n.children.empty()) return res;

        std::vector<ASCIINode> child_nodes;
        int total_width = 0;
        for (const auto& c : n.children) {
            ASCIINode cn = build(c);
            child_nodes.push_back(cn);
            total_width += cn.width;
        }
        total_width += (int)(n.children.size() - 1) * 2;

        res.width = std::max(res.width, total_width);
        res.height = 1;
        std::string connector(res.width, ' ');
        int x = 0;
        for (size_t i = 0; i < child_nodes.size(); i++) {
            int cmid = x + child_nodes[i].middle;
            if (cmid < res.middle)
                connector[cmid] = '/';
            else if (cmid > res.middle)
                connector[cmid] = '\\';
            x += child_nodes[i].width + 2;
        }

        res.lines.push_back(connector);
        res.height++;

        int max_child_height = 0;
        for (auto& c : child_nodes) max_child_height = std::max(max_child_height, (int)c.lines.size());

        for (int i = 0; i < max_child_height; i++) {
            std::string line(res.width, ' ');
            int pos = 0;
            for (size_t j = 0; j < child_nodes.size(); j++) {
                ASCIINode& c = child_nodes[j];
                std::string part = (i < (int)c.lines.size()) ? c.lines[i] : std::string(c.width, ' ');
                for (int k = 0; k < (int)part.size(); k++) line[pos + k] = part[k];
                pos += c.width + 2;
            }
            res.lines.push_back(line);
            res.height++;
        }

        return res;
    };

    ASCIINode diagram = build(root);
    std::ostringstream out;
    for (auto& l : diagram.lines) out << l << "\n";
    return out.str();
}

std::string indent_ast(const std::string& input) {
    std::string s = collapse_inline_brackets(input);

    std::string out;
    int indent = 0;
    bool new_line = true;

    auto add_indent = [&]() {
        for (int i = 0; i < indent; ++i) out += "    ";
    };

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (new_line) {
            add_indent();
            new_line = false;
        }
        if (s[i] == '[' && i + 1 < s.size() && s[i + 1] == ']') {
            out += "[]";
            i++;
            continue;
        } else if (c == '{' || c == '[') {
            out += c;
            out += '\n';
            indent++;
            new_line = true;
        } else if (c == '}' || c == ']') {
            out += '\n';
            indent = std::max(0, indent - 1);
            new_line = true;
            add_indent();
            out += c;
        } else if (c == ',') {
            out += c;
            out += '\n';
            new_line = true;
        } else {
            out += c;
        }
    }

    out += '\n';
    return out;
}
std::string asciiTreeAST(const std::string& input) {
    std::string s = collapse_inline_brackets(input);

    std::string out;
    std::vector<bool> lastChildStack;
    int indent = 0;
    bool new_line = true;

    for (size_t i = 0; i < s.size(); ++i) {
        if (new_line) {
            while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
            if (i >= s.size()) break;

            for (int j = 0; j < indent; ++j) out += lastChildStack[j] ? "    " : "│   ";

            out += (lastChildStack.empty() || !lastChildStack.back()) ? "├─ " : "└─ ";

            new_line = false;
        }

        if (s[i] == '[' && i + 1 < s.size() && s[i + 1] == ']') {
            out += "[]";
            i++;
            continue;
        }

        char c = s[i];

        if (c == '{' || c == '[') {
            out += '\n';
            indent++;
            lastChildStack.push_back(false);
            new_line = true;
        } else if (c == '}' || c == ']') {
            out += '\n';
            indent = std::max(0, indent - 1);
            if (!lastChildStack.empty()) lastChildStack.pop_back();
            new_line = true;
        } else if (c == ',') {
            out += '\n';
            new_line = true;
        } else {
            out += c;
        }
    }

    out += '\n';
    return out;
}
std::string removeExtension(const std::string& filename) {
    size_t lastDot = filename.find_last_of('.');
    if (lastDot == std::string::npos) { return filename; }
    return filename.substr(0, lastDot);
}
int emitObjectFile(llvm::Module& M, const std::string& outputPath, bool debug, std::string tgt = "") {
#ifndef X86_ONLY
#ifndef __EMSCRIPTEN__
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();
    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    LLVMInitializeARMAsmPrinter();
    LLVMInitializeARMAsmParser();
#endif
    LLVMInitializeWebAssemblyTargetInfo();
    LLVMInitializeWebAssemblyTarget();
    LLVMInitializeWebAssemblyTargetMC();
    LLVMInitializeWebAssemblyAsmPrinter();
    LLVMInitializeWebAssemblyAsmParser();
#endif
#ifdef __EMSCRIPTEN__
    llvm::Triple triple("wasm32-unknown-unknown");
    M.setTargetTriple(triple);
    std::string err;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        llvm::errs() << "Target error: " << err << "\n";
        return 1;
    }
    llvm::TargetOptions opt;
    if (debug) opt.DebuggerTuning = llvm::DebuggerKind::GDB;
    auto RM = llvm::Reloc::PIC_;
    llvm::TargetMachine* TM = target->createTargetMachine(triple, "generic", "", opt, RM);
    M.setDataLayout(TM->createDataLayout());
    std::error_code EC;
    llvm::raw_fd_ostream dest(outputPath, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return 1;
    }
    llvm::legacy::PassManager PM;
    if (TM->addPassesToEmitFile(PM, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        llvm::errs() << "TargetMachine cannot emit file\n";
        return 1;
    }
    PM.run(M);
    delete TM;
    return 0;
#else
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();
    llvm::Triple triple(tgt.empty() ? llvm::sys::getDefaultTargetTriple() : tgt);
    M.setTargetTriple(triple);
    std::string err;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        llvm::errs() << "Target error: " << err << "\n";
        return 1;
    }
    llvm::TargetOptions opt;
    if (debug) opt.DebuggerTuning = llvm::DebuggerKind::GDB;
    auto RM = llvm::Reloc::PIC_;
    llvm::TargetMachine* TM = target->createTargetMachine(triple, "generic", "", opt, RM);
    M.setDataLayout(TM->createDataLayout());
    std::error_code EC;
    llvm::raw_fd_ostream dest(outputPath, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return 1;
    }
    llvm::legacy::PassManager PM;
    if (TM->addPassesToEmitFile(PM, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        llvm::errs() << "TargetMachine cannot emit file\n";
        return 1;
    }
    PM.run(M);
    delete TM;
    return 0;
#endif
}
Mer run(std::string file, std::string text, RunConfig config = {}) {
    // Check for inline directives
    definitions = config.definitions;
    definitions["__FILE__"] = "\"" + file + "\"";
    aliases = config.aliases;
    dir_aliases = config.dir_aliases;
    if (text.find("// @no-context") != std::string::npos) { config.use_context = false; }

    if (text.find("// @looser-types") != std::string::npos) { config.looser_types = true; }

    if (text.find("// @print-ast") != std::string::npos) { config.print_ast = true; }

    if (text.find("// @print-tokens") != std::string::npos) { config.print_tokens = true; }

    if (text.find("// @show-time") != std::string::npos) { config.show_time = true; }

    if (text.find("// @quiet") != std::string::npos) { config.quiet_mode = true; }
    if (text.find("// @raw-ast") != std::string::npos) {
        config.raw = true;
        config.print_ast = true;
    }
    if (text.find("// @bst-ast") != std::string::npos) {
        config.bst = true;
        config.print_ast = true;
    }
    loose = config.looser_types;
    std::unordered_map<std::string, std::string> cleaned_files;
    std::unordered_set<std::string> visited;
    std::vector<std::string> queue = {file};
    std::unordered_map<std::string, std::string> raw_file_contents;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> file_included_namespaces;
    raw_file_contents[file] = text;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> file_namespace_deps;
    std::unordered_map<std::string, std::unordered_set<std::string>> fileAccessibleNamespaces;
    auto start = std::chrono::high_resolution_clock::now();
    try {
        while (!queue.empty()) {
            std::string current_file = queue.back();
            queue.pop_back();
            if (visited.count(current_file)) { continue; }
            visited.insert(current_file);
            std::string current_text;
            if (raw_file_contents.count(current_file)) {
                current_text = raw_file_contents[current_file];
            } else {
                std::ifstream infile(current_file);
                if (!infile.is_open()) {
                    return Mer{
                        Aer{nullptr, nullptr},
                        Ler{std::vector<Token>{}, new InvalidSyntaxError("QC-IX01: Include Error: Could not open " + current_file, Position())}, ""};
                }
                std::stringstream buffer;
                buffer << infile.rdbuf();
                current_text = buffer.str();
            }
            bool saved_no_main = no_main;
            if (config.progress) {
                std::cout << "[PREPROCESSING] " << current_file << '\n';
                std::cout.flush();
            }
            auto outputofdeps = preprocess_includes(current_text, current_file);
            if (config.progress) {
                std::cout << "[DONE PREPROCESSING] " << current_file << '\n';
                std::cout.flush();
            }
            if (!(current_file == file)) { no_main = saved_no_main; }
            cleaned_files[current_file] = outputofdeps.clean_source;
            fileAccessibleNamespaces[current_file] = outputofdeps.accessible_namespaces;
            file_included_namespaces[current_file] = outputofdeps.included_namespaces;
            file_namespace_deps[current_file] = outputofdeps.namespace_depends;
            for (const std::string& dep_path : outputofdeps.dependency_paths) {
                if (!visited.count(dep_path)) { queue.push_back(dep_path); }
            }
        }
    } catch (std::runtime_error& e) {
        return Mer{Aer{nullptr, nullptr},
                   Ler{std::vector<Token>{}, new InvalidSyntaxError(std::string("QC-IX01: Include Error: ") + e.what(), Position())}, ""};
    }
    std::unordered_map<std::string, Aer> file_asts;
    std::unordered_map<std::string, std::unordered_map<std::string, UserTypeInfo>> type_registry;
    Ler resp;
    std::function<void(const std::string&)> process_file = [&](const std::string& path) {
        if (file_asts.count(path)) return;
        auto& deps = file_included_namespaces[path];
        for (auto const& [dep_path, ns_list] : deps) { process_file(dep_path); }
        std::unordered_map<std::string, UserTypeInfo> visible_types;
        for (auto const& [dep_path, ns_list] : deps) {
            auto& dep_types = type_registry[dep_path];
            std::unordered_set<std::string> widened_ns(ns_list.begin(), ns_list.end());
            std::vector<std::string> worklist(ns_list.begin(), ns_list.end());
            while (!worklist.empty()) {
                std::string ns = worklist.back();
                worklist.pop_back();
                if (file_namespace_deps.count(dep_path) && file_namespace_deps[dep_path].count(ns)) {
                    for (const std::string& needed_ns : file_namespace_deps[dep_path][ns]) {
                        if (widened_ns.insert(needed_ns).second) { worklist.push_back(needed_ns); }
                    }
                }
            }
            for (const std::string& ns : widened_ns) {
                for (auto& [type_name, info] : dep_types) {
                    if (type_name.rfind(ns + "::", 0) == 0) { visible_types[type_name] = info; }
                }
            }
        }
        Ler file_resp;
        try {
            if (config.progress) {
                std::cout << "[LEXING] " << path << '\n';
                std::cout.flush();
            }
            Lexer lexer(cleaned_files[path], path);
            file_resp = lexer.make_tokens();
            if (config.progress) {
                std::cout << "[DONE LEXING] " << path << '\n';
                std::cout.flush();
            }
            if (config.dump_tokens && file == path && file_resp.error != nullptr) {
                std::cout << "\n##DUMP##" << '\n'
                          << "ERROR: " << file_resp.error->pos.line << " " << file_resp.error->pos.column << " " << file_resp.error->pos.length << " "
                          << file_resp.error->details << '\n'
                          << "########" << '\n';
                return;
            }
        } catch (IllegalCharError e) {
            if (config.dump_tokens) {
                std::cout << "\n##DUMP##" << '\n'
                          << e.pos.line << " " << e.pos.column << " " << e.pos.length << " " << e.details << '\n'
                          << "########" << '\n';
                return;
            } else {
                throw e;
            }
        }
        if (config.dump_tokens && file == path) {
            std::cout << "\n##DUMP##" << '\n';
            for (const auto& tok : file_resp.Tkns) {
                std::cout << tok.pos.line << " " << tok.pos.column << " " << tok.pos.length << " " << get_token_name(tok.type) << '\n';
            }
            std::cout << "########" << '\n';
            return;
        }
        if (config.progress) {
            std::cout << "[PARSING] " << path << '\n';
            std::cout.flush();
        }
        Parser parser(file_resp.Tkns, visible_types);
        bool saved_no_main = no_main;
        no_main = ((path == file) ? no_main : true);
        Aer ast = parser.parse();
        if (config.progress) {
            std::cout << "[DONE PARSING] " << path << '\n';
            std::cout.flush();
        }
        no_main = saved_no_main;
        type_registry[path] = ast.user_types;
        file_asts[path] = ast;
        if (path == file) { resp = file_resp; }
    };
    try {
        process_file(file);
        if (config.dump_tokens) { return Mer{Aer{nullptr, nullptr}, Ler{std::vector<Token>(), nullptr}, ""}; }
    } catch (InvalidSyntaxError& e) {
        std::cout << '\n' << e.as_string() << '\n';
        return Mer{Aer{nullptr, nullptr}, resp, ""};
    } catch (IllegalCharError& e) {
        std::cout << '\n' << e.as_string() << '\n';
        return Mer{Aer{nullptr, nullptr}, Ler{std::vector<Token>(), nullptr}, ""};
    }
    Aer ast = file_asts[file];
    if (config.print_tokens) {
        std::cout << "=== TOKENS ===" << '\n';
        for (const auto& tok : resp.Tkns) { std::cout << "Type: " << get_token_name(tok.type) << " | Value: '" << tok.value << "'" << '\n'; }
        std::cout << "==============" << '\n' << '\n';
    }
    if (ast.error) { return Mer{ast, resp, ""}; }
    if (config.print_ast) {
        std::cout << "=== AST ===" << std::endl;
        for (const auto& [filepath, astd] : file_asts) {
            std::cout << "--- File: " << filepath << " ---" << std::endl;
            if (!astd.statements) {
                std::cout << "  (Empty AST)" << std::endl;
                continue;
            }
            for (const auto& stmt : astd.statements->statements) {
                if (config.raw) {
                    std::cout << indent_ast(printAny(stmt));
                } else if (config.bst) {
                    std::cout << bst_diagram(printAny(stmt));
                } else {
                    std::cout << asciiTreeAST(printAny(stmt));
                }
            }
        }
        std::cout << "===========" << std::endl << std::endl;
    }
    try {
        // compiler
#ifdef ENABLE_LLVM
        if (config.compile_mode) {
            llvm::LLVMContext context;
            auto master_module = new llvm::Module("master_module", context);
#ifdef __EMSCRIPTEN__
            LLVMInitializeWebAssemblyTargetInfo();
            LLVMInitializeWebAssemblyTarget();
            LLVMInitializeWebAssemblyTargetMC();

            llvm::Triple triple("wasm32-unknown-unknown");
            master_module->setTargetTriple(triple);

            std::string target_err;
            const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, target_err);
            if (target) {
                llvm::TargetOptions opt;
                auto RM = llvm::Reloc::PIC_;
                llvm::TargetMachine* TM = target->createTargetMachine(triple, "generic", "", opt, RM);
                master_module->setDataLayout(TM->createDataLayout());
                delete TM;
            }
#else
#ifndef X86_ONLY
            LLVMInitializeAArch64TargetInfo();
            LLVMInitializeAArch64Target();
            LLVMInitializeAArch64TargetMC();
            LLVMInitializeARMTargetInfo();
            LLVMInitializeARMTarget();
            LLVMInitializeARMTargetMC();
            LLVMInitializeWebAssemblyTargetInfo();
            LLVMInitializeWebAssemblyTarget();
            LLVMInitializeWebAssemblyTargetMC();
#endif
            LLVMInitializeX86TargetInfo();
            LLVMInitializeX86Target();
            LLVMInitializeX86TargetMC();

            llvm::Triple triple(config.target.empty() ? llvm::sys::getDefaultTargetTriple() : config.target);
            master_module->setTargetTriple(triple);

            std::string target_err;
            const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, target_err);
            if (target) {
                llvm::TargetOptions opt;
                auto RM = llvm::Reloc::PIC_;
                llvm::TargetMachine* TM = target->createTargetMachine(triple, "generic", "", opt, RM);
                master_module->setDataLayout(TM->createDataLayout());
                delete TM;
            }
#endif
            if (config.use_runtime) {
                llvm::StringRef irString(_binary_runtime_ll_start, _binary_runtime_ll_size);
                llvm::SMDiagnostic err;
                llvm::MemoryBufferRef bufRef(irString, "runtime.ll");
                auto modulePtr = llvm::parseIR(bufRef, err, context);
                if (!modulePtr) {
                    err.print("runtime.ll", llvm::errs());
                    throw "Failed to load runtime.ll";
                }
                if (llvm::Linker::linkModules(*master_module, std::move(modulePtr))) { throw "Failed to link runtime module"; }
            }
            std::unordered_map<std::string, std::unordered_map<std::string, FunctionSignature>> db_sigs;
            std::unordered_map<std::string, std::unordered_map<std::string, FuncDefNode*>> db_fDefs;
            std::unordered_map<std::string, std::unordered_map<std::string, std::string>> db_typeStrings;
            std::unordered_map<std::string, std::unordered_map<std::string, int>> db_lengths;
            std::unordered_map<std::string, std::unordered_map<std::string, std::string>> db_vars;
            std::unordered_map<std::string, std::unordered_map<std::string, llvm::AllocaInst*>> db_allocas;
            std::unordered_map<std::string, std::unordered_map<std::string, llvm::FunctionType*>> db_lambdas;
            std::unordered_map<std::string, std::unordered_map<std::string, llvm::GlobalVariable*>> db_globals;
            std::unordered_map<std::string, std::map<std::string, llvm::Function*>> db_specialized;
            std::vector<std::string> sorted_files;
            std::unordered_set<std::string> sort_visited;
            std::function<void(const std::string&)> sort_visit = [&](const std::string& p) {
                if (sort_visited.count(p)) return;
                for (auto const& [dep_p, ns_list] : file_included_namespaces[p]) { sort_visit(dep_p); }
                sort_visited.insert(p);
                sorted_files.push_back(p);
            };
            sort_visit(file);
            std::vector<Diagnostic> diagnostics;
            for (auto& filepath : sorted_files) {
                std::unordered_map<std::string, FunctionSignature> visSigs;
                std::unordered_map<std::string, FuncDefNode*> visFDefs;
                std::unordered_map<std::string, std::pair<int, int>> visJagged;
                std::unordered_map<std::string, std::string> visTypeStr;
                std::unordered_map<std::string, int> visLen;
                std::unordered_map<std::string, std::string> visVars;
                std::unordered_map<std::string, llvm::AllocaInst*> visAlloc;
                std::unordered_map<std::string, llvm::GlobalVariable*> visGlobals;
                std::unordered_map<std::string, llvm::FunctionType*> visLamb;
                std::map<std::string, llvm::Function*> visSpec;
                for (auto const& [dep_p, ns_list] : file_included_namespaces[filepath]) {
                    if (db_sigs.find(dep_p) == db_sigs.end()) continue;
                    for (const std::string& ns : ns_list) {
                        if (ns.empty()) continue;
                        auto pre = ns + "::";
                        auto& d_sigs = db_sigs.at(dep_p);
                        auto& d_fDefs = db_fDefs.at(dep_p);
                        auto& d_typeStr = db_typeStrings.at(dep_p);
                        auto& d_len = db_lengths.at(dep_p);
                        auto& d_vars = db_vars.at(dep_p);
                        auto& d_allocs = db_allocas.at(dep_p);
                        auto& d_lambs = db_lambdas.at(dep_p);
                        auto& d_spec = db_specialized.at(dep_p);
                        auto& d_globals = db_globals.at(dep_p);
                        for (auto const& [k, v] : d_sigs)
                            if (k.rfind(pre, 0) == 0) visSigs[k] = v;
                        for (auto const& [k, v] : d_fDefs)
                            if (k.rfind(pre, 0) == 0) visFDefs[k] = v;
                        for (auto const& [k, v] : d_typeStr)
                            if (k.rfind(pre, 0) == 0) visTypeStr[k] = v;
                        for (auto const& [k, v] : d_len)
                            if (k.rfind(pre, 0) == 0) visLen[k] = v;
                        for (auto const& [k, v] : d_vars)
                            if (k.rfind(pre, 0) == 0) visVars[k] = v;
                        for (auto const& [k, v] : d_allocs)
                            if (k.rfind(pre, 0) == 0) visAlloc[k] = v;
                        for (auto const& [k, v] : d_lambs)
                            if (k.rfind(pre, 0) == 0) visLamb[k] = v;
                        for (auto const& [k, v] : d_globals)
                            if (k.rfind(pre, 0) == 0) visGlobals[k] = v;
                        if (d_spec.count(ns)) { visSpec[ns] = d_spec.at(ns); }
                    }
                }
                isHeader = std::filesystem::path(filepath).extension() == ".hqc";
                if (config.progress) {
                    std::cout << "[COMPILING] " << filepath << '\n';
                    std::cout.flush();
                }
                LLVMCompiler comp(file_asts[filepath].user_types, master_module, context, filepath == file);
                comp.config = config;
                std::vector<CTError> errs = comp.compile(file_asts[filepath].statements, visSigs, visFDefs, visJagged, visTypeStr, visLen, visVars,
                                                         visAlloc, visLamb, visSpec, visGlobals);
                if (config.progress) {
                    std::cout << "[DONE COMPILING] " << filepath << '\n';
                    std::cout.flush();
                }
                if (!errs.empty()) {
                    for (auto& err : errs)
                        diagnostics.push_back(
                            {new CTError(err.details, err.pos, err.is_warning, err.error_name, err.notes, err.helps, err.insights), err.is_warning ? "Warning" : "Error"});
                    break;
                }
                db_sigs[filepath] = comp.functionSignatures;
                db_fDefs[filepath] = comp.functionDefs;
                db_typeStrings[filepath] = comp.arrayTypeStringsStack[0];
                db_lengths[filepath] = comp.arrayLengthsStack[0];
                db_vars[filepath] = comp.varTypesStack[0];
                db_allocas[filepath] = comp.runtimeArraySizes;
                db_lambdas[filepath] = comp.lambdaTypes;
                db_specialized[filepath] = comp.specializedFunctions;
                db_globals[filepath] = comp.globals;
            }
            for (Diagnostic& diagnostic : diagnostics) {
                if (diagnostic.level != "Warning") { return Mer{ast, resp, "Program exited with code: 1", diagnostics}; }
            }
            std::string base_name = config.output_file.empty() ? "out" : removeExtension(config.output_file);
            size_t last_slash = base_name.find_last_of("/\\");
            std::string dir = (last_slash == std::string::npos) ? "" : base_name.substr(0, last_slash + 1);
            std::string stem = (last_slash == std::string::npos) ? base_name : base_name.substr(last_slash + 1);

            std::string ll_file = config.compile_only ? base_name + ".ll" : dir + "temp_" + stem + ".ll";
            std::string obj_file = config.object_only ? base_name + ".o" : dir + "temp_" + stem + ".o";
            auto end = std::chrono::high_resolution_clock::now();
            bool error_found = false;
            if (!diagnostics.empty() &&
                std::ranges::any_of(diagnostics, [&](const Diagnostic& d) { return d.level != "Warning" && d.level != "Note"; })) {
                error_found = true;
            }
            std::string message = "Program exited with code: 0";
            if (error_found) { message = "Program exited with code: 1"; }
            if (!diagnostics.empty() && error_found) { return Mer{ast, resp, message, diagnostics}; }
            std::error_code EC;
            llvm::raw_fd_ostream out(ll_file, EC, llvm::sys::fs::OF_Text);
            if (EC) {
                llvm::errs() << "Failed to open output: " << EC.message() << "\n";
                message = "Program exited with code: 1";
                return Mer{ast, resp, message, diagnostics};
            }
            if (config.use_runtime) {
                for (llvm::Function& F : master_module->functions()) {
                    if (F.getName().starts_with("qc_") || F.getName().starts_with("__qc_") || F.getName().starts_with("_qc_")) {
                        F.setLinkage(llvm::GlobalValue::InternalLinkage);
                    }
                }
            }
#ifndef __EMSCRIPTEN__
            if (config.optimize) {
                if (config.progress) {
                    std::cout << "[OPTIMIZING] " << file << '\n';
                    std::cout.flush();
                }
                llvm::Triple triple(config.target.empty() ? llvm::sys::getDefaultTargetTriple() : config.target);
                std::string target_err;
                const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, target_err);
                llvm::TargetMachine* TM = nullptr;
                if (target) {
                    llvm::TargetOptions opt;
                    TM = target->createTargetMachine(triple, "generic", "", opt, llvm::Reloc::PIC_);
                }
                llvm::LoopAnalysisManager LAM;
                llvm::FunctionAnalysisManager FAM;
                llvm::CGSCCAnalysisManager CGAM;
                llvm::ModuleAnalysisManager MAM;
                llvm::PassInstrumentationCallbacks PIC;
                if (config.debug)
                    PIC.registerBeforeNonSkippedPassCallback([](llvm::StringRef PassID, llvm::Any IR) {
                        if (const auto** F = llvm::any_cast<const llvm::Function*>(&IR)) {
                            llvm::errs() << "[" << PassID << "] on: " << (*F)->getName() << "\n";
                        }
                    });

                llvm::PassBuilder PB(TM, llvm::PipelineTuningOptions(), std::nullopt, &PIC);
                if (TM) {
                    FAM.registerPass([&] { return TM->getTargetIRAnalysis(); });
                }
                PB.registerModuleAnalyses(MAM);
                PB.registerCGSCCAnalyses(CGAM);
                PB.registerFunctionAnalyses(FAM);
                PB.registerLoopAnalyses(LAM);
                PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
                llvm::ModulePassManager MPM;
                auto optimization_level = llvm::OptimizationLevel::O2;
                switch (config.opt_level[1]) {
                case '0': optimization_level = llvm::OptimizationLevel::O0; break;
                case '1': optimization_level = llvm::OptimizationLevel::O1; break;
                case '2': optimization_level = llvm::OptimizationLevel::O2; break;
                case '3': optimization_level = llvm::OptimizationLevel::O3; break;
                case 'z': optimization_level = llvm::OptimizationLevel::Oz; break;
                default: optimization_level = llvm::OptimizationLevel::O2; break;
                }
                std::string errStr;
                llvm::raw_string_ostream errStream(errStr);
                if (llvm::verifyModule(*master_module, &errStream)) {
                    llvm::errs() << "LLVM Error: Module verification failed: " << errStr << "\n";
                    message = "Program exited with code: 1";
                    return Mer{ast, resp, message, diagnostics};
                }
                MPM = PB.buildPerModuleDefaultPipeline(optimization_level);
                MPM.run(*master_module, MAM);
                if (config.progress) {
                    std::cout << "[DONE OPTIMIZING] " << file << '\n';
                    std::cout.flush();
                }
                delete TM;
            }
#endif
            master_module->print(out, nullptr);
            if (config.compile_only) {
                message += ". Compiled to " + ll_file;
                return Mer{ast, resp, message, diagnostics};
            }
            if (config.progress) {
                std::cout << "[EMITTING OBJECT] " << file << '\n';
                std::cout.flush();
            }
            int llc_result = emitObjectFile(*master_module, obj_file, config.debug, config.target);
            if (llc_result != 0) {
                diagnostics.push_back({new CTError("Failed to compile IR to object file", Position(Position::INVALID_FILE_ID, 0, 0, 0))});
                return Mer{ast, resp, message, diagnostics};
            }
            if (config.progress) {
                std::cout << "[DONE EMITTING OBJECT] " << file << '\n';
                std::cout.flush();
            }
            if (config.object_only) {
                std::remove(ll_file.c_str());
                message += ". Compiled to " + obj_file;
                return Mer{ast, resp, message, diagnostics};
            }
            if (config.progress) {
                std::cout << "[LINKING] " << file << '\n';
                std::cout.flush();
            }
            std::string final_exe = config.output_file.empty() ? "a.out" : config.output_file;
            std::string link_cmd = "gcc " + obj_file + " -o " + final_exe;
            for (const auto& path : to_link_dir) { link_cmd += " -L\"" + path + "\""; }
            for (const auto& lib : to_link) {
                if (lib.ends_with(".a") || lib.ends_with(".so") || lib.ends_with(".dylib")) {
                    link_cmd += " \"" + lib + "\"";
                } else {
                    link_cmd += " -l" + lib;
                }
            }
            for (const auto& path : config.library_search_paths) { link_cmd += " -L\"" + path + "\""; }
            for (const auto& lib : config.libraries) {
                if (lib.ends_with(".a") || lib.ends_with(".so") || lib.ends_with(".dylib")) {
                    link_cmd += " \"" + lib + "\"";
                } else {
                    link_cmd += " -l" + lib;
                }
            }
            for (const std::string& arg : config.link_with) { link_cmd += " " + arg; }
            link_cmd += " -lm";
            if (config.debug) link_cmd += " -g";
            if (!config.quiet_mode) std::cout << "Linking with command " + link_cmd << '\n';
            int link_result = system(link_cmd.c_str());
            if (link_result != 0) {
                diagnostics.push_back({new CTError("Failed to link object file", Position(Position::INVALID_FILE_ID, 0, 0, 0))});
                return Mer{ast, resp, message, diagnostics};
            }
            std::remove(ll_file.c_str());
            std::remove(obj_file.c_str());
            if (config.progress) {
                std::cout << "[DONE LINKING] " << file << '\n';
                std::cout.flush();
            }
            message += ". Built executable: " + final_exe;
            if (config.quiet_mode) message = "";
            return Mer{ast, resp, message, diagnostics};
        }
#endif
    } catch (const char* err) { std::cout << err << '\n'; } catch (const std::exception& e) {
        std::cout << "std::exception: " << e.what() << '\n';
    } catch (...) { std::cout << "unknown error\n"; }
    return Mer{ast, resp, "", std::vector<Diagnostic>{}};
}
}
