#ifndef PARSER_H
#define PARSER_H
#include "nodes.h"
#include <vector>
#include <unordered_set>
#include <functional>
enum class MainType { NA, RT_ARRAY, C_STYLE };
inline MainType main_type = MainType::NA;

class Parser {
  public:
    [[gnu::noinline]]
    void parseGenerics(std::vector<GenericType>& genericsM, ParseResult& res);
    [[gnu::noinline]]
    void fn(std::vector<ConceptInfo::Block>& blockList, ParseResult& res, std::vector<ConceptInfo::Block>& blocks,
            std::vector<std::pair<int, ConceptInfo::DefaultBlock>>& defaultBlocks); 
    std::vector<GenericType> current_generics;
    std::string base_type_name(std::string full_type) {
        size_t open_bracket = full_type.find('<');
        if (open_bracket == std::string::npos) { return full_type; }
        std::string base = full_type.substr(0, open_bracket);
        size_t last_valid_char = base.find_last_not_of(" \t\r\n");
        if (last_valid_char != std::string::npos) { return base.substr(0, last_valid_char + 1); }
        return base;
    }
    AnyNode default_value_for_type(const Token& type_tok, const Position& pos);
    std::vector<std::string> namespaceStack;
    size_t index = 0;
    int tmp_counter = 0;
    std::unordered_map<std::string, UserTypeInfo> user_types;
    Token current_tok;
    std::vector<Token> tokens;
    std::string currentNamespace;
    Parser(std::vector<Token> tokens, std::unordered_map<std::string, UserTypeInfo> user_types = {});
    std::string qualify_name(const std::string& name);
    bool is_primint_type(const Token& name) {
        return std::unordered_set<std::string>({"int", "byte", "nibble", "addr_t", "short int", "long int"}).contains(name.value);
    }
    bool is_primitive_type(std::string& name) {
        return std::unordered_set<std::string>({"void", "int", "double", "float", "byte", "nibble", "addr_t", "string", "char", "bool", "qbool"})
            .contains(name);
    }
    bool is_known_type(std::string name) {
        std::string base = base_type_name(name);
        if (base.ends_with("&")) { base.pop_back(); }
        while (base.ends_with("*")) base.pop_back();
        while (base.ends_with("[]")) {
            base.pop_back();
            base.pop_back();
        }
        for (GenericType ty : current_generics) {
            if (ty.name == base && !ty.isNonType) { return true; }
        }
        if (user_types.count(base_type_name(base))) return true;
        std::string ns;
        for (int i = (int)namespaceStack.size() - 1; i >= 0; --i) {
            ns = ns.empty() ? namespaceStack[i] : namespaceStack[i] + "::" + ns;
            std::string candidate = ns + "::" + base;
            if (user_types.count(base_type_name(candidate))) return true;
        }
        return false;
    }
    std::string get_known_type(std::string name) {
        std::string base = base_type_name(name);
        if (base.ends_with("&")) { base.pop_back(); }
        while (base.ends_with("*")) base.pop_back();
        while (base.ends_with("[]")) {
            name.pop_back();
            base.pop_back();
        }
        if (user_types.count(base)) return base;
        for (GenericType ty : current_generics) {
            if (ty.name == base) { return base; }
        }
        std::string ns;
        for (int i = (int)namespaceStack.size() - 1; i >= 0; --i) {
            ns = ns.empty() ? namespaceStack[i] : namespaceStack[i] + "::" + ns;
            std::string candidate = ns + "::" + base;
            if (user_types.count(candidate)) return candidate;
        }
        return base;
    }
    Token peek(int offset = 1) {
        size_t peek_idx = this->index + offset;
        if (peek_idx < this->tokens.size()) { return this->tokens[peek_idx]; }
        return this->current_tok;
    }
    std::optional<std::string> try_parse_qualified_name() {
        if (current_tok.type != TokenType::IDENTIFIER) return std::nullopt;
        return parseTypeString();
    }
    Token consume_qualified_name() {
        if (current_tok.type != TokenType::IDENTIFIER) { throw InvalidSyntaxError("QC-S279: Expected identifier", current_tok.pos); }

        Position start_pos = current_tok.pos;
        std::string qualified = current_tok.value;
        this->advance();
        while (current_tok.type == TokenType::SCOPE) {
            this->advance();

            if (current_tok.type != TokenType::IDENTIFIER) { throw InvalidSyntaxError("QC-S280: Expected identifier after '::'", current_tok.pos); }

            qualified += "::" + current_tok.value;
            this->advance();
        }

        return Token(TokenType::IDENTIFIER, qualified, start_pos);
    }
    UserTypeInfo* find_type(std::string name) {
        if (is_known_type(base_type_name(name))) {
            for (GenericType ty : current_generics) {
                if (ty.name == base_type_name(get_known_type(name))) { return nullptr; }
            }
            return &user_types[base_type_name(get_known_type(name))];
        }
        name = base_type_name(name);
        if (name.find("::") != std::string::npos) {
            if (user_types.count(base_type_name(name))) { return &user_types[base_type_name(name)]; }
            return nullptr;
        }
        if (!currentNamespace.empty()) {
            std::string key = currentNamespace + "::" + name;
            if (user_types.count(base_type_name(key))) { return &user_types[base_type_name(key)]; }
        }
        for (int i = namespaceStack.size() - 1; i >= 0; --i) {
            std::string ns;
            for (int j = 0; j <= i; ++j) {
                if (j > 0) ns += "::";
                ns += namespaceStack[j];
            }
            std::string key = ns + "::" + name;
            if (user_types.count(base_type_name(key))) { return &user_types[base_type_name(key)]; }
        }
        if (user_types.count(base_type_name(name))) { return &user_types[base_type_name(name)]; }

        return nullptr;
    }
    bool is_known_qualified_type(const std::string& full) const {
        auto pos = full.rfind("::");
        if (pos == std::string::npos) return false;

        std::string ns = full.substr(0, pos);
        std::string name = full.substr(pos + 2);

        for (auto& [k, info] : user_types) {
            if (k == name && info.namespace_path == ns) { return true; }
        }

        return false;
    }

    Token advance();
    Prs factor();
    Prs term();
    Prs comparison();
    Prs expr();
    Prs atom();
    Prs power();
    Prs if_expr(bool is_comptime = false);
    Prs defer_expr();
    Prs return_stmt();
    Prs bin_op(std::function<Prs()> func, std::initializer_list<TokenType> ops);
    Prs logical_and();
    Prs qif_expr();
    Prs bitwise();
    Prs qout_expr();
    Prs qin_expr();
    Prs logical_or();
    Prs match_stmt();
    Prs switch_stmt();
    Prs qswitch_stmt();
    Aer parse();
    Prs statement();
    Prs loop_stmt();
    Prs dowhile_stmt();
    Prs while_stmt();
    Prs for_stmt();
    Prs try_catch_expr();
    Prs array_literal();
    Prs ternary();
    Prs text_unops();
    Prs call(AnyNode node_to_call);
    Prs func_def_multi(std::vector<Token> return_type, std::optional<Token> func_name, std::vector<GenericType> generics, bool keep = false,
                       bool is_volatile = false, std::vector<Token> modifiers = {});
    Parameter parse_parameter(bool type_only);
    bool in_extern = false;
    bool in_foreign = false;
    std::string parseNoGenericString() {
        std::string type = "";
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "volatile") {
            type += "volatile ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "atomic") {
            type += "atomic ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "fn") {
            type += "fn";
            this->advance();
            if (this->current_tok.type != TokenType::LPAREN) {
                throw InvalidSyntaxError("QC-T058: `fn` is not a standalone type and must also have its () and return type", this->current_tok.pos);
            }
            this->advance();
            type += "(";
            type += parseTypeString();
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                type += ", " + parseTypeString();
            }
            if (this->current_tok.type != TokenType::RPAREN) {
                throw InvalidSyntaxError("QC-T058: `fn` is not a standalone type and must also have its () and return type", this->current_tok.pos);
            }
            this->advance();
            type += ")";
            if (this->current_tok.type != TokenType::ARROW) {
                throw InvalidSyntaxError("QC-T059: `fn` must have a `->` before the return type.", this->current_tok.pos);
            }
            type += " -> ";
            this->advance();
            type += parseTypeString();
            return type;
        }
        if (this->current_tok.type == TokenType::KEYWORD && (this->current_tok.value == "short" || this->current_tok.value == "long")) {
            type += this->current_tok.value + " ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD || this->current_tok.type == TokenType::IDENTIFIER) {
            type += this->current_tok.value;
            this->advance();
        } else {
            return "";
        }
        while (this->current_tok.type == TokenType::SCOPE) {
            this->advance();
            type += "::" + this->current_tok.value;
            this->advance();
        }
        while (this->current_tok.type == TokenType::LBRACKET) {
            this->advance();
            if (this->current_tok.type == TokenType::INT) {
                type += "[" + this->current_tok.value + "]";
                this->advance();
            } else {
                type += "[]";
            }
            this->advance();
        }
        while (this->current_tok.type == TokenType::MUL) {
            type += "*";
            this->advance();
        }
        if (this->current_tok.type == TokenType::AMPERSAND) {
            type += "&";
            this->advance();
        }

        return type;
    }
    std::string parseTypeString() {
        std::string type = "";
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "proves") {
            this->advance();
            auto parse_constraint_type = [&]() -> std::string {
                if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER) { return ""; }
                std::string result = this->current_tok.value;
                this->advance();
                while (this->current_tok.type == TokenType::SCOPE) {
                    this->advance();
                    if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER) { return ""; }
                    result += "::" + this->current_tok.value;
                    this->advance();
                }
                if (this->current_tok.type == TokenType::LESS) {
                    this->advance();
                    result += "<";
                    int depth = 1;
                    while (depth > 0) {
                        if (this->current_tok.type == TokenType::EOFT) { return ""; }
                        if (this->current_tok.type == TokenType::LESS) {
                            depth++;
                        } else if (this->current_tok.type == TokenType::MORE) {
                            depth--;
                            if (depth == 0) {
                                this->advance();
                                break;
                            }
                        }
                        result += this->current_tok.value;
                        this->advance();
                    }
                    result += ">";
                }
                return result;
            };
            auto parse_not = [&]() -> std::string {
                std::string result;
                if (this->current_tok.type == TokenType::NOT) {
                    result += "!";
                    this->advance();
                }
                std::string type = parse_constraint_type();
                if (type.empty()) { return ""; }
                result += type;
                return result;
            };
            auto parse_and = [&]() -> std::string {
                std::string result = parse_not();
                if (result.empty()) { return ""; }
                while (this->current_tok.type == TokenType::AND) {
                    result += " && ";
                    this->advance();
                    std::string rhs = parse_not();
                    if (rhs.empty()) { return ""; }
                    result += rhs;
                }
                return result;
            };
            std::string constraint = parse_and();
            if (constraint.empty()) { return ""; }
            while (this->current_tok.type == TokenType::OR) {
                constraint += " || ";
                this->advance();
                std::string rhs = parse_and();
                if (rhs.empty()) { return ""; }
                constraint += rhs;
            }
            type += constraint;
            return type;
        }
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "volatile") {
            type += "volatile ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "atomic") {
            type += "atomic ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "fn") {
            type += "fn";
            this->advance();
            if (this->current_tok.type != TokenType::LPAREN) {
                throw InvalidSyntaxError("QC-T058: `fn` is not a standalone type and must also have its () and return type", this->current_tok.pos);
            }
            this->advance();
            type += "(";
            type += parseTypeString();
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                type += ", " + parseTypeString();
            }
            if (this->current_tok.type != TokenType::RPAREN) {
                throw InvalidSyntaxError("QC-T058: `fn` is not a standalone type and must also have its () and return type", this->current_tok.pos);
            }
            this->advance();
            type += ")";
            if (this->current_tok.type != TokenType::ARROW) {
                throw InvalidSyntaxError("QC-T059: `fn` must have a `->` before the return type.", this->current_tok.pos);
            }
            type += " -> ";
            this->advance();
            type += parseTypeString();
            return type;
        }
        if (this->current_tok.type == TokenType::KEYWORD && (this->current_tok.value == "short" || this->current_tok.value == "long")) {
            type += this->current_tok.value + " ";
            this->advance();
        }
        if (this->current_tok.type == TokenType::KEYWORD || this->current_tok.type == TokenType::IDENTIFIER) {
            type += this->current_tok.value;
            this->advance();
        } else {
            return "";
        }
        while (this->current_tok.type == TokenType::SCOPE) {
            this->advance();
            type += "::" + this->current_tok.value;
            this->advance();
        }
        if (this->current_tok.type == TokenType::LESS) {
            this->advance();
            type += "<";
            int depth = 1;
            while (depth > 0) {
                if (this->current_tok.type == TokenType::EOFT) break;
                if (this->current_tok.type == TokenType::LESS) {
                    depth++;
                } else if (this->current_tok.type == TokenType::MORE) {
                    depth--;
                    if (depth == 0) {
                        this->advance();
                        break;
                    }
                }
                type += this->current_tok.value;
                this->advance();
            }
            type += ">";
        }
        while (this->current_tok.type == TokenType::LBRACKET) {
            this->advance();
            if (this->current_tok.type == TokenType::INT) {
                type += "[" + this->current_tok.value + "]";
                this->advance();
            } else {
                type += "[]";
            }
            this->advance();
        }
        while (this->current_tok.type == TokenType::MUL) {
            type += "*";
            this->advance();
        }
        if (this->current_tok.type == TokenType::AMPERSAND) {
            type += "&";
            this->advance();
        }

        return type;
    }
    inline AnyNode prs_to_anynode(Prs st) {
        return std::visit(
            [](auto arg) -> AnyNode {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_constructible_v<AnyNode, T>) { return AnyNode(arg); }
                return std::monostate{};
            },
            st);
    }

    bool parse_block_into(StatementsNode*& out_block, ParseResult& res) {
        if (this->current_tok.type == TokenType::LBRACE) {
            this->advance();
            std::vector<AnyNode> stmts;
            while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
                Prs st = this->statement();
                if (std::holds_alternative<Error*>(st)) {
                    res.failure(std::get<Error*>(st));
                    return false;
                }
                AnyNode any_stmt = prs_to_anynode(st);
                stmts.push_back(any_stmt);
            }
            if (this->current_tok.type != TokenType::RBRACE) {
                res.failure(new InvalidSyntaxError("QC-S281: Expected '}' after block", this->current_tok.pos));
                return false;
            }
            this->advance();
            out_block = new StatementsNode(stmts, true);
            return true;
        } else {
            Prs st = this->statement();
            if (std::holds_alternative<Error*>(st)) {
                res.failure(std::get<Error*>(st));
                return false;
            }
            AnyNode any_stmt = prs_to_anynode(st);
            std::vector<AnyNode> stmts;
            stmts.push_back(any_stmt);
            out_block = new StatementsNode(stmts, false);
            return true;
        }
    }
    Prs assignment_expr();
};
#endif
