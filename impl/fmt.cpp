#include "lexer.h"
#include "yaml.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>
const std::string VERSION = "v0.0.1";
bool isLiteralTok(TokenType t) {
    return (t >= TokenType::INT && t <= TokenType::CHAR) || t == TokenType::BOOL || t == TokenType::QBOOL || t == TokenType::FSTRING;
}
bool isWholeNumberLiteral(TokenType t) {
    return (t >= TokenType::INT && t < TokenType::STRING) || (t >= TokenType::BYTE && t <= TokenType::ADDR_T);
}
bool isAssignmentTok(TokenType t) {
    switch (t) {
    case TokenType::EQ:
    case TokenType::PLUS_EQ:
    case TokenType::MINUS_EQ:
    case TokenType::MUL_EQ:
    case TokenType::DIV_EQ:
    case TokenType::MOD_EQ:
    case TokenType::LSH_EQ:
    case TokenType::RSH_EQ:
    case TokenType::BIT_X_EQ:
    case TokenType::RROT_EQ:
    case TokenType::LROT_EQ:
    case TokenType::LRSH_EQ:
    case TokenType::BIT_A_EQ:
    case TokenType::BIT_O_EQ: return true;
    default: return false;
    }
}
bool isDelimiterTok(TokenType t) {
    switch (t) {
    case TokenType::LPAREN:
    case TokenType::RPAREN:
    case TokenType::LBRACE:
    case TokenType::RBRACE:
    case TokenType::LBRACKET:
    case TokenType::RBRACKET: return true;
    default: return false;
    }
}
bool isOpeningDelimiter(TokenType t) {
    return t == TokenType::LPAREN || t == TokenType::LBRACE || t == TokenType::LBRACKET;
}
bool isClosingDelimiter(TokenType t) {
    return t == TokenType::RPAREN || t == TokenType::RBRACE || t == TokenType::RBRACKET;
}
bool isOperatorTok(TokenType t) {
    switch (t) {
    case TokenType::MOD:
    case TokenType::EQ_TO:
    case TokenType::NOT_EQ:
    case TokenType::MORE:
    case TokenType::LESS:
    case TokenType::MORE_EQ:
    case TokenType::LESS_EQ:
    case TokenType::AND:
    case TokenType::OR:
    case TokenType::XOR:
    case TokenType::BITWISE_NOT:
    case TokenType::BITWISE_XOR:
    case TokenType::ARROW:
    case TokenType::R_ROT:
    case TokenType::L_ROT:
    case TokenType::LOGICAL_RSHIFT:
    case TokenType::QAND:
    case TokenType::QOR:
    case TokenType::COLLAPSE_AND:
    case TokenType::COLLAPSE_OR:
    case TokenType::QEQEQ:
    case TokenType::QNEQ:
    case TokenType::QNOT:
    case TokenType::QXOR:
    case TokenType::PIPE:
    case TokenType::QUESTION:
    case TokenType::AS:
    case TokenType::TYPEOF:
    case TokenType::BIG_ARROW:
    case TokenType::AMPERSAND:
    case TokenType::STAR:
    case TokenType::PLUS:
    case TokenType::MINUS:
    case TokenType::MUL:
    case TokenType::DIV:
    case TokenType::POWER:
    case TokenType::LSHIFT:
    case TokenType::RSHIFT:
    case TokenType::SCOPE:
    case TokenType::INCREMENT:
    case TokenType::DECREMENT:
    case TokenType::SIZEOF:
    case TokenType::VARADIC:
    case TokenType::NOT: return true;
    default: return false;
    }
}
bool isUnaryOpTok(TokenType t) {
    switch (t) {
    case TokenType::BITWISE_NOT:
    case TokenType::QNOT:
    case TokenType::AS:
    case TokenType::TYPEOF:
    case TokenType::MINUS:
    case TokenType::INCREMENT:
    case TokenType::DECREMENT:
    case TokenType::SIZEOF:
    case TokenType::VARADIC:
    case TokenType::AMPERSAND:
    case TokenType::MUL:
    case TokenType::NOT: return true;
    default: return false;
    }
}
#include <unordered_set>

bool isUnary(TokenType type, bool previous_context) {
    switch (type) {
    case TokenType::MUL:
    case TokenType::AMPERSAND:
    case TokenType::MINUS: return isUnaryOpTok(type) && !previous_context;
    default: return isUnaryOpTok(type);
    }
}
bool isOpWord(const std::string& s) {
    return s == "as" || s == "throw" || s == "typeof" || s == "sizeof";
}
bool isControlFlowKeyword(const std::string& s) {
    return s == "if" || s == "else" || s == "for" || s == "foreach" || s == "while" || s == "do" || s == "loop" || s == "switch" || s == "match" ||
           s == "case" || s == "default" || s == "qif" || s == "qelse" || s == "qelif" || s == "qswitch" || s == "try" || s == "catch";
}
bool isJumpKeyword(const std::string& s) {
    return s == "goto" || s == "jmp";
}
bool isStatementKeyword(const std::string& s) {
    return s == "return" || s == "break" || s == "continue" || s == "unreachable" || s == "defer";
}
bool containsNewline(const Token& t) {
    return t.type == TokenType::WHITESPACE && t.value.find('\n') != std::string::npos;
}
bool isHorizontalWhitespace(const Token& t) {
    return t.type == TokenType::WHITESPACE && t.value.find('\n') == std::string::npos && !t.value.empty();
}
Token remove_newline(Token t) {
    if (!t.value.contains("\n")) return t;
    t.value = t.value.substr(t.value.find("\n"), t.value.length() - t.value.find("\n"));
    return t;
}
std::string get_casing_scheme(std::string name) {
    std::unordered_map<std::string, bool> options = {{"snake_case", true},
                                                     {"camelCase", true},
                                                     {"PascalCase", true},
                                                     {"camel_Snake_Case", true},
                                                     {"Pascal_Snake_Case", true}};
    bool allcaps = std::all_of(name.begin(), name.end(), [](unsigned char ch) { return std::isupper(ch) || !std::isalpha(ch); });
    if (name.length() == 1) {
        if (std::isupper(name[0])) return "PascalCase";
        return "camelCase";
    }
    for (int i = 0; i < name.length(); i++) {
        char c = name[i];
        if (allcaps) { return "SCREAMING_SNAKE_CASE"; }

        if (std::isupper(c) && i == 0) {
            options["snake_case"] = false;
            options["camelCase"] = false;
            options["camel_Snake_Case"] = false;
        } else if (i == 0) {
            options["PascalCase"] = false;
            options["Pascal_Snake_Case"] = false;
        } else if (c == '_') {
            options["camelCase"] = false;
            options["PascalCase"] = false;
        } else if (std::isupper(c)) {
            if (name[i - 1] != '_') {
                options["Pascal_Snake_Case"] = false;
                options["camel_Snake_Case"] = false;
            }
            options["snake_case"] = false;
            if (name[i - 1] == '_') {
                options["PascalCase"] = false;
                options["camelCase"] = false;
            }
        }
    }
    for (auto [key, value] : options) {
        if (value == true) { return key; }
    }
    return "unknown";
}
bool is_primitive(const std::string& value) {
    return std::unordered_set<std::string>({"int", "float", "double", "bool", "string", "qbool", "char", "ai64u_t", "ai64_t", "i32u_t", "i16u_t",
                                            "long int", "short int", "void", "auto", "byte", "nibble", "addr_t"})
        .contains(value);
}
struct FmtError {
    std::string name;
    std::string msg;
    size_t index;
};
class Fmt {
  public:
    std::vector<Token> tokens;
    std::vector<FmtError> errors;
    Token current_tok;
    fkyaml::node rules;
    size_t idx = 0;
    void advance() {
        if (tokens.size() <= idx) return;
        this->current_tok = tokens[idx++];
        this->previous_tok = this->current_tok;
    }
    size_t current_indentation = 0;
    size_t tab_size = 4;
    bool use_tabs = false;
    std::string after_unop;
    std::string before_binop;
    std::string after_binop;
    Token previous_tok = Token(TokenType::EOFT, "<eof>", Position());
    Fmt(std::vector<Token> tokens, std::vector<FmtError> errors, fkyaml::node rules)
        : rules(rules), tokens(tokens), errors(errors), current_tok(tokens[0]) {}
    void check_indentation(Token tok) {
        if (use_tabs) {
            std::string tabs = std::string(current_indentation, '\t');
            if (tok.value != tabs) error("incorrect indentation level", "error");
        } else {
            std::string spaces = std::string(current_indentation * tab_size, ' ');
            if (tok.value != spaces) error("incorrect indentation level", "error");
        }
    }
    void error(std::string message, std::string level) { errors.push_back({level, message, this->idx}); }
    Token peek(long long by) {
        if (tokens.size() <= idx + by || idx + by < 1) return Token();
        return tokens[idx + by - 1];
    }
    Token previous_non_whitespace() {
        for (long long i = idx - 1; i >= 1; i--) {
            Token res = peek(i - idx);
            if (res.type != TokenType::WHITESPACE) return res;
        }
        return Token();
    }
    bool previous_token_is_value_context() {
        TokenType type = previous_non_whitespace().type;
        return type == TokenType::IDENTIFIER || isLiteralTok(type) || isClosingDelimiter(type);
    }
    void format_operator() {
        if (previous_tok.type == TokenType::EOFT) {
            if (!isUnaryOpTok(this->current_tok.type)) error("syntax error: binary operator used with no previous tok", "error");
            this->advance();
            this->advance();
            return;
        }
        if (isUnary(this->current_tok.type, previous_token_is_value_context())) {
            if (isOpWord(this->current_tok.value)) {
                if (Token next = peek(1); next.type != TokenType::WHITESPACE || next.value != " ")
                    error("expected space after word unary op", "error");
                else
                    this->advance();
                this->advance();
            } else if (previous_token_is_value_context()) {
                if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
                this->advance();
            } else {
                this->advance();
                if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
                format_expr();
            }
        } else {
            if (this->current_tok.type == TokenType::ARROW || this->current_tok.type == TokenType::DOT) {
                if (peek(1).value == " ") error("expected no space after ->/. operator.", "error");
                if (previous_tok.value == " ") error("expected no space before ->/. operator.", "error");
                this->advance();
                this->advance();
            } else if (this->current_tok.type == TokenType::QUESTION) {
                if (!after_binop.empty() && peek(1).value != after_binop) error("expected " + after_binop + " after ? in ternary operator.", "error");
                if (!before_binop.empty() && previous_tok.value != before_binop)
                    error("expected " + before_binop + " before ? in ternary operator.", "error");
                this->advance();
                this->advance();
                format_expr();
                if (!after_binop.empty() && peek(1).value != after_binop) error("expected " + after_binop + " after : in ternary operator.", "error");
                if (!before_binop.empty() && previous_tok.value != before_binop)
                    error("expected " + before_binop + " before : in ternary operator.", "error");
                this->advance();
                this->advance();
            } else {
                if (!after_binop.empty() && peek(1).value != after_binop) error("expected " + after_binop + " after binary operator.", "error");
                if (!before_binop.empty() && previous_tok.value != before_binop)
                    error("expected " + before_binop + " before binary operator.", "error");
                this->advance();
                this->advance();
            }
        }
    }
    void format_primary_expr() {
        if (isLiteralTok(this->current_tok.type)) {
            this->advance();
            return;
        }
        if (this->current_tok.type == TokenType::LPAREN || this->current_tok.type == TokenType::LBRACKET) {
            this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE) {
                error("expected no space between ( and expr", "error");
                this->advance();
            }
            format_expr();
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                if (this->current_tok.type != TokenType::WHITESPACE) {
                    error("expected a space between , and expr", "error");
                } else {
                    this->advance();
                }
                if (this->current_tok.type == TokenType::RPAREN || this->current_tok.type == TokenType::RBRACKET) {
                    error("trailing comma", "warning");
                    break;
                }
                format_expr();
            }
            if (this->current_tok.type == TokenType::RPAREN || this->current_tok.type == TokenType::RBRACKET) this->advance();
            return;
        }
        if (this->current_tok.type == TokenType::IDENTIFIER || (!isOpWord(this->current_tok.value) && this->current_tok.type == TokenType::KEYWORD)) {
            format_ident();
        }
    }
    void format_expr() {
        format_primary_expr();
        if (this->current_tok.type == TokenType::WHITESPACE && isAssignmentTok(peek(1).type) || isOperatorTok(peek(1).type)) { this->advance(); }
        if (isAssignmentTok(this->current_tok.type) || isOperatorTok(this->current_tok.type)) { format_operator(); }
        if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::LBRACKET) {
            error("expected no whitespace between expr and [", "error");
            this->advance();
        }
        while (this->current_tok.type == TokenType::LBRACKET) {
            this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE) {
                error("expected no whitespace after [", "error");
                this->advance();
            }
            if (isWholeNumberLiteral(this->current_tok.type)) this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE) {
                error("expected no whitespace before ]", "error");
                this->advance();
            }
            if (this->current_tok.type == TokenType::RBRACKET) this->advance();
        }
    }
    std::string format_ident() {
        std::string result;
        result += this->current_tok.value;
        this->advance();
        while (this->current_tok.type == TokenType::WHITESPACE || this->current_tok.type == TokenType::SCOPE) {
            if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::SCOPE) {
                error("expected no space between identifier and ::", "error");
                this->advance();
            } else if (this->current_tok.type == TokenType::WHITESPACE)
                break;
            if (this->current_tok.type == TokenType::SCOPE) {
                result += this->current_tok.value;
                this->advance();
                if (this->current_tok.type == TokenType::WHITESPACE) {
                    error("expected no space between :: and identifier", "error");
                    this->advance();
                }
                if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                    result += this->current_tok.value;
                    this->advance();
                }
            }
        }
        return result;
    }
    std::unordered_set<std::string> known_types;
    bool is_known_type() {
        size_t saved_idx = idx - 1;
        std::string result;
        if (this->current_tok.type == TokenType::VARADIC) return true;
        if (this->current_tok.type == TokenType::LPAREN) {
            result += "(" + format_ident();
            if (this->current_tok.type == TokenType::COMMA) {
                idx = saved_idx;
                return true;
            } else {
                idx = saved_idx;
                return false;
            }
        } else
            result = format_ident();
        idx = saved_idx;
        this->advance();
        return known_types.contains(result) || is_primitive(result);
    }
    void consume_type() {
        if (this->current_tok.type == TokenType::LPAREN) {
            this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE) {
                error("expected no space between ( and expr", "error");
                this->advance();
            }
            consume_type();
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                if (this->current_tok.type != TokenType::WHITESPACE) {
                    error("expected a space between , and expr", "error");
                } else {
                    this->advance();
                }
                if (this->current_tok.type == TokenType::RPAREN) {
                    error("trailing comma", "warning");
                    break;
                }
                consume_type();
            }
            if (this->current_tok.type == TokenType::RPAREN) this->advance();
            return;
        }
        while (this->current_tok.value == "const" || this->current_tok.value == "atomic" || this->current_tok.value == "short" ||
               this->current_tok.value == "long") {
            this->advance();
            if (this->current_tok.type != TokenType::WHITESPACE) {
                error("expected whitespace after const/atomic/short/long", "error");
            } else
                this->advance();
        }
        format_expr();
        while (this->current_tok.type == TokenType::MUL) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::MUL) {
            if (peek(2).type == TokenType::MUL) {
                error("2 stars after whitespace. int ** is invalid, prefer int* *", "warning");
                this->advance();
            }
            this->advance();
            this->advance();
        }
        if (this->current_tok.value == "const") this->advance();
        while (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::MUL || this->current_tok.type == TokenType::MUL ||
               this->current_tok.value == "const")
            this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::AMPERSAND) this->advance();
        if (this->current_tok.type == TokenType::AMPERSAND) this->advance();
        return;
    }
    void format_func_def() {
        this->advance();
        while (this->current_tok.type != TokenType::RPAREN && this->current_tok.type != TokenType::EOFT) {
            if (!is_known_type()) {
                error("expected type before param name in func def", "error");
                return;
            } else {
                consume_type();
            }
            if (this->current_tok.type != TokenType::WHITESPACE) {
                error("expected space after type", "error");
            } else
                this->advance();
            if (this->current_tok.type == TokenType::IDENTIFIER) {
                format_expr();
                if (this->current_tok.type == TokenType::EQ) {
                    error("expected space before =", "error");
                } else if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::EQ)
                    this->advance();
                if (this->current_tok.type == TokenType::EQ) format_expr();
            }
            if (this->current_tok.type == TokenType::WHITESPACE) {
                error("trailing space after parameter", "error");
                this->advance();
            }
            if (this->current_tok.type == TokenType::RPAREN) break;
            if (this->current_tok.type != TokenType::COMMA) {
                error("expected comma after parameter", "error");
            } else {
                this->advance();
                if (this->current_tok.type != TokenType::WHITESPACE) {
                    error("expected space after comma", "error");
                    this->advance();
                }
            }
        }
        this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::SEMICOLON) {
            error("expected no space before semicolon in func def", "error");
            this->advance();
        }
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            return;
        } else {
            if (this->current_tok.type != TokenType::WHITESPACE) {
                error("expected space before { in func def", "error");
            } else
                this->advance();
            if (this->current_tok.type != TokenType::RBRACE) {
                error("expected { in func def", "error");
            } else
                format_stmt();
        }
    }
    void format_type_begin_stmt() {
        consume_type();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space after type", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::IDENTIFIER) {
            format_expr();
            if (this->current_tok.type != TokenType::WHITESPACE && peek(1).type == TokenType::EQ) {
                error("expected space before =", "error");
            } else if (this->current_tok.type == TokenType::WHITESPACE)
                this->advance();
            if (this->current_tok.type == TokenType::EQ)
                format_expr();
            else if (this->current_tok.type == TokenType::LPAREN) {
                this->format_func_def();
                return;
            }
            if (this->current_tok.type != TokenType::SEMICOLON) {
                error("expected ; after var decl", "error");
            } else
                this->advance();
        }
    }
    void format_for_stmt() {
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before (", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::LPAREN) this->advance();
        if (this->current_tok.type != TokenType::SEMICOLON)
            format_stmt();
        else
            this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
        if (this->current_tok.type != TokenType::SEMICOLON) format_expr();
        if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
        if (this->current_tok.type != TokenType::RPAREN) format_expr();
        if (this->current_tok.type == TokenType::RPAREN) this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected whitespace before stmt", "error");
        } else
            this->advance();
        format_stmt();
    }
    bool is_semicolon_before_rparen() {
        size_t ind = idx - 1;
        while (this->current_tok.type != TokenType::RPAREN) {
            if (this->current_tok.type == TokenType::SEMICOLON) {
                idx = ind;
                this->advance();
                return true;
            }
        }
        idx = ind;
        this->advance();
        return false;
    }
    void format_if_stmt() {
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before (", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::LPAREN) this->advance();
        if (is_semicolon_before_rparen()) { this->format_stmt(); }
        if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
        this->format_expr();
        if (this->current_tok.type == TokenType::RPAREN) this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected whitespace before stmt", "error");
        } else
            this->advance();
        format_stmt();
        if (this->current_tok.value == "else") {
            this->advance();
            if (this->current_tok.type != TokenType::WHITESPACE) {
                error("expected whitespace before stmt", "error");
            } else
                this->advance();
            format_stmt();
        }
    }
    void format_while_stmt() {
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before (", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::LPAREN) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) {
            error("expected no space before condition in while", "error");
            this->advance();
        }
        this->format_expr();
        if (this->current_tok.type == TokenType::RPAREN) this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected whitespace before stmt", "error");
        } else
            this->advance();
        format_stmt();
    }
    void format_loop_do_or_defer_stmt() {
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected whitespace before stmt", "error");
        } else
            this->advance();
        format_stmt();
    }
    void format_dowhile_stmt() {
        format_loop_do_or_defer_stmt();
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before (", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::LPAREN) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) {
            error("expected no space before condition in while", "error");
            this->advance();
        }
        this->format_expr();
        if (this->current_tok.type == TokenType::RPAREN) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) {
            error("expected no whitespace before ;", "error");
            this->advance();
        }
        if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
    }
    void format_foreach_stmt() {
        this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before (", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::LPAREN) this->advance();
        if (this->current_tok.type == TokenType::WHITESPACE) {
            error("expected no space before condition in foreach", "error");
            this->advance();
        }
        consume_type();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space after type", "error");
        } else
            this->advance();
        if (this->current_tok.type == TokenType::IDENTIFIER) { format_expr(); }
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before in", "error");
        } else
            this->advance();
        if (this->current_tok.value == "in") this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected space before collection", "error");
        } else
            this->advance();
        this->format_expr();
        if (this->current_tok.type == TokenType::RPAREN) this->advance();
        if (this->current_tok.type != TokenType::WHITESPACE) {
            error("expected whitespace before stmt", "error");
        } else
            this->advance();
        format_stmt();
    }
    void format_stmt() {
        if (this->current_tok.value == "if") {
            this->format_if_stmt();
            return;
        } else if (this->current_tok.value == "while") {
            this->format_while_stmt();
            return;
        } else if (this->current_tok.value == "do") {
            this->format_dowhile_stmt();
            return;
        } else if (this->current_tok.value == "for") {
            this->format_for_stmt();
            return;
        } else if (this->current_tok.value == "loop" || this->current_tok.value == "defer") {
            this->format_loop_do_or_defer_stmt();
            return;
        } else if (this->current_tok.value == "foreach") {
            this->format_foreach_stmt();
            return;
        }
        if (this->current_tok.type == TokenType::LBRACE) {
            this->advance();
            this->current_indentation++;
            while (this->current_tok.type != TokenType::RBRACE) {
                if (this->current_tok.type != TokenType::WHITESPACE || this->current_tok.value[0] != '\n') {
                    error("expected newline after lbrace", "error");
                }
                if (this->current_tok.type == TokenType::WHITESPACE) {
                    if (peek(1).type == TokenType::RBRACE) {
                        current_indentation--;
                        check_indentation(remove_newline(this->current_tok));
                        this->advance();
                        continue;
                    }
                    check_indentation(remove_newline(this->current_tok));
                }
                this->advance();
                if (this->current_tok.type == TokenType::RBRACE) {
                    current_indentation--;
                    continue;
                }
                this->format_stmt();
                if (this->current_tok.type == TokenType::RBRACE) {
                    current_indentation--;
                    continue;
                }
            }
            if (previous_tok.type == TokenType::WHITESPACE) check_indentation(remove_newline(previous_tok));
            return;
        }
        if (is_known_type()) {
            format_type_begin_stmt();
            return;
        }
        if (this->current_tok.value == "return") {
            this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE && peek(1).type == TokenType::SEMICOLON) {
                this->advance();
                error("expected no whitespace in void return", "error");
            } else if (this->current_tok.type == TokenType::WHITESPACE) {
                do {
                    this->advance();
                    if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
                    format_expr();
                } while (this->current_tok.type == TokenType::COMMA);
            }
            if (this->current_tok.type != TokenType::SEMICOLON) {
                error("expected space after return", "error");
                do {
                    this->advance();
                    if (this->current_tok.type == TokenType::WHITESPACE) this->advance();
                    format_expr();
                } while (this->current_tok.type == TokenType::COMMA);
            }
            if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
            return;
        }
        if (this->current_tok.value == "break" || this->current_tok.value == "continue" || this->current_tok.value == "unreachable") {
            this->advance();
            if (this->current_tok.type == TokenType::WHITESPACE) {
                this->advance();
                error("expected no whitespace after break/continue", "error");
            }
            if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
            return;
        }
        this->format_expr();
    }
    void format() {
        after_unop = rules["formatting"]["operators"]["unops"]["after"].get_value<std::string>();
        before_binop = rules["formatting"]["operators"]["binops"]["before"].get_value<std::string>();
        after_binop = rules["formatting"]["operators"]["binops"]["after"].get_value<std::string>();
        while (this->current_tok.type != TokenType::EOFT) {
            format_stmt();
            /*
    case switch default match
    if else
     while loop do for foreach
    in
    fn
    qif qelse qelif qswitch
    class struct enum type
    foreign extern
    namespace"
    roperator operator
    try catch
    nullptr
    out inout volatile restrict
    friend friendly static abstract final
    public protected private virtual override
    defer
    concept proves with_proof _of at_least all_of
    proved_by
    modifier on_call on_return on_use
    comptime
    union
    goto jmp
                */
        }
    }
};
std::unordered_map<std::string, std::variant<std::vector<FmtError>, std::string>>
format_many(std::string rule_alias, std::unordered_map<std::string, std::vector<Token>> files) {
    char* chome = std::getenv("HOME");
    if (chome == nullptr) { throw "HOME not set"; }
    std::string home(chome);
    if (!std::filesystem::exists(home + "/.qc-fmt/config.yaml")) { throw "Please run `qc fmt setup`"; }
    FILE* config_file = fopen((home + "/.qc-fmt/config.yaml").c_str(), "r");
    auto config_node = fkyaml::node::deserialize(config_file);
    std::fclose(config_file);
    if (!config_node["aliases"].as_map().contains(rule_alias)) { throw "Invalid rule alias"; }
    FILE* rules_file = std::fopen(config_node["aliases"][rule_alias].get_value<std::string>().c_str(), "r");
    fkyaml::node rules = fkyaml::node::deserialize(rules_file);
    std::fclose(rules_file);
    std::unordered_map<std::string, std::variant<std::vector<FmtError>, std::string>> result;
    std::string file_comment = "";
    std::string namespace_comment = "";
    std::string function_comment = "";
    std::string internal_comment = "";
    if (rules.as_map().contains("comments")) {
        if (rules["comments"].as_map().contains("rules")) {
            function_comment = rules["comments"]["specifiers"][rules["comments"]["rules"]["functions"]].get_value<std::string>();
            file_comment = rules["comments"]["specifiers"][rules["comments"]["rules"]["file"]].get_value<std::string>();
            namespace_comment = rules["comments"]["specifiers"][rules["comments"]["rules"]["namespaces"]].get_value<std::string>();
            internal_comment = rules["comments"]["rules"]["internal"].get_value<std::string>();
        }
    }
    size_t tab_size = 4;
    bool use_tabs = false;
    if (rules.as_map().contains("formatting")) {
        if (rules["formatting"].as_map().contains("tab_size")) {
            tab_size = rules["formatting"]["tab_size"].get_value<size_t>();
            use_tabs = rules["foramtting"]["use_tabs"].get_value<size_t>();
        }
    }
    for (auto [filename, tokens] : files) {
        Fmt formatter = Fmt(tokens, std::vector<FmtError>(), rules);
        formatter.tab_size = tab_size;
        formatter.use_tabs = use_tabs;
        formatter.format();
        result[filename] = formatter.errors;
    }
    return result;
}

/*
void notImplemented(int count, char** args) {
    std::cout << "Not implemented" << '\n';
}
void help(int count, char** args) {
    std::cout << "QuantumC Formatter version " << VERSION << R"(
qc fmt help - This command.
qc fmt list <config file alias | default> - Lists formatting conventions for that alias.
qc fmt check [config file alias | default] <filename> - Checks formatting against formatting conventions. Current saved default unless specified file
path / default qc fmt apply [config file alias | default] <filename> - Same as above but also applys. qc fmt setup - Creates all the folders and files
for the cli to work, and adds it to your PATH. qc fmt create - Adds formatting rules, creates the rules file, and adds it, with an alias. qc fmt alias
{ <name> <path> | -l|--list | -d|--delete <name> } - Creates a quick alias for a rule file and stores it in the saved directorys in the .qconform
folder. -d/--delete is the vis-versa, and -l/--list lists all aliases
)";
}
void setup(int count, char** args) {
    char* chome = std::getenv("HOME");
    if (chome == nullptr) {
        throw "HOME not set.";
    }
    std::string home(chome);
    std::filesystem::create_directories(home + "/.qc-fmt/bin");
    std::filesystem::create_directories(home + "/.qc-fmt/formats");
    if (std::filesystem::exists(home + "/.qc-fmt/config.yaml")) {
        std::cout << "Config file already exists." << '\n';
    } else {
        auto file = std::ofstream(home + "/.qc-fmt/config.yaml");
        file << "aliases:\n  default: " << home << "/.qc-fmt/default.fmt.yaml";
        file.close();
    }
    if (std::filesystem::exists(home + "/.qc-fmt/default.fmt.yaml")) {
        std::cout << "Default format already exists." << '\n';
    } else {
        auto file = std::ofstream(home + "/.qc-fmt/default.fmt.yaml");
        file << R"(
formatting:
  indent_size: 4
  use_tabs: false
  operators:
    unops:
      after: ""
    binops:
      before: " "
      after: " "
  # params_per_line: null
  # max_width: null
naming:
  specifiers:
    priv_member:
      position: prefix
      value: __
      style: snake_case
  rules:
    enforce:
      level: warning
      autocorrect: false
    vars: snake_case
    functions: camelCase
    user_types: PascalCase
    constants: SCREAMING_SNAKE_CASE
    private_members: priv_member
    private_methods: camel_Snake_Case
    namespaces: PascalCase
    internal_namespaces: Pascal_Snake_Case
    internal_functions: camel_Snake_Case
comments:
  specifiers:
    normal: //
    doc: ///
    top_level_doc: //!
  rules:
    enforce:
      level: error
    functions: doc
    file: top_level_doc
    namespaces: doc
    internal: No Include
newlines:
  style: lf
  final_newline: true
  collapse_blanks: true
includes:
  filepath:
    quotes: false
namespaces:
  types:
    amount: 1
    enforce: warning
)";
        file.close();
    }
    std::string shell;
    const char* shell_raw = getenv("SHELL");
    if (shell_raw) shell = shell_raw;
    std::string rcFile;
    if (shell.ends_with("zsh")) rcFile = home + "/.zshrc";
    else if (shell.ends_with("fish")) rcFile = home + "/.config/fish/config.fish";
    else rcFile = home + "/.bashrc";
    std::string exportLine = "export PATH=\"$HOME/.qc-fmt/bin:$PATH\"";
    if (shell.ends_with("fish")) {
        exportLine = "fish_add_path $HOME/.qc-fmt/bin";
    }
    std::ifstream rcIn(rcFile);
    std::string rcContent((std::istreambuf_iterator<char>(rcIn)), std::istreambuf_iterator<char>());
    rcIn.close();
    char result[PATH_MAX];
    ssize_t cnt = readlink("/proc/self/exe", result, PATH_MAX);
    std::filesystem::copy_file(std::string(result, cnt), home + "/.qc-fmt/bin/qconform", std::filesystem::copy_options::overwrite_existing);
    std::cout << "Run: source " << rcFile << "\n";
}
std::string strip(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}
void create(int count, char** args) {
    char* chome = std::getenv("HOME");
    if (chome == nullptr) {
        throw "HOME not set";
    }
    std::string home(chome);
    if (!std::filesystem::exists(home + "/.qc-fmt/formats")) {
        throw "Please run qc fmt setup";
    }
    if (!std::filesystem::exists(home + "/.qc-fmt/config.yaml")) {
        throw "Please run qc fmt setup";
    }
    std::cout << "\033[?1049h\033[2J\033[H";
    std::cout << "This Wizard will create you your .fmt.yaml." << '\n';
    std::cout << "\n\n";
    std::cout << "rules name: (" << std::filesystem::current_path().filename() << ") ";
    std::string rules_name = "";
    std::getline(std::cin, rules_name);
    if (rules_name.empty() || std::all_of(rules_name.begin(), rules_name.end(), isspace)) {
        rules_name = std::filesystem::current_path().filename();
    }
    std::cout << '\n';
    std::cout << "\n\nFormatting rules:\n\n";
    std::cout << "  Enter spacing amount or tab for tabs: (4) ";
    std::string tabs;
    std::getline(std::cin, tabs);
    int tabc = 4;
    std::string use_tabs = "false";
    try {
        if (strip(tabs) != "tab") {
            std::stoi(tabs);
        } else {
            tabs = "4";
            use_tabs = "true";
        }
    } catch (...) {
        tabs = "4";
    }
    std::cout << '\n';
    std::cout << "  Enter max number of parameters per line (or null for none): (null) ";
    std::string param_max;
    std::getline(std::cin, param_max);
    try {
        std::stoi(param_max);
    } catch (...) {
        param_max = "null";
    }
    std::cout << '\n';
    std::cout << "  Enter max number of chars per line (or null for none): (null) ";
    std::string char_max;
    std::getline(std::cin, char_max);
    try {
        std::stoi(char_max);
    } catch (...) {
        char_max = "null";
    }
    std::cout << '\n' << '\n' << '\n';
    std::cout << "  Brace rules:\n\n";
    std::cout << "    What whitespace character should be before open braces (' ' or default): (\\n) ";
    std::string before_braces;
    std::getline(std::cin, before_braces);
    if (before_braces != " ") {
        before_braces = "\\n";
    }
    std::cout << "\n";
    std::cout << "    What level should brace rules be enforced (warning or error or not): (warning) ";
    std::string brace_enforcement;
    std::getline(std::cin, brace_enforcement);
    if (brace_enforcement != "error" && brace_enforcement != "warning" && brace_enforcement != "not") {
        brace_enforcement = "warning";
    }
    std::cout << "\n";
    std::cout << "    Should the apply command automatically fix brace spacing? (Y/n) ";
    std::string autocomplete_brace_fixes;
    std::getline(std::cin, autocomplete_brace_fixes);
    if (autocomplete_brace_fixes == "Y" || autocomplete_brace_fixes == "y") {
        autocomplete_brace_fixes = "true";
    } else {
        autocomplete_brace_fixes = "false";
    }
    std::cout << '\n' << '\n' << '\n';
    std::cout << "  Operator rules:\n\n";
    std::cout << "    Should unops have a space after them: (Y/n)";
    std::string unop_space_after;
    std::getline(std::cin, unop_space_after);
    if (unop_space_after == "Y" || unop_space_after == "y") {
        unop_space_after = " ";
    } else {
        unop_space_after = "";
    }
    std::cout << "    Should binops have a space before them: (Y/n)";
    std::string binop_space_before;
    std::getline(std::cin, binop_space_before);
    if (binop_space_before == "Y" || binop_space_before == "y") {
        binop_space_before = " ";
    } else {
        binop_space_before = "";
    }
    std::cout << "    Should binops have a space after them: (Y/n)";
    std::string binop_space_after;
    std::getline(std::cin, binop_space_after);
    if (binop_space_after == "Y" || binop_space_after == "y") {
        binop_space_after = " ";
    } else {
        binop_space_after = "";
    }
    std::cout << '\n';
    std::cout << "\n\nNaming rules:\n\n";
        std::vector<std::string> casing_styles = {
        "snake_case", "camelCase", "PascalCase",
        "SCREAMING_SNAKE_CASE", "camel_Snake_Case", "Pascal_Snake_Case"
    };
    struct Specifier {
        std::string name;
        std::string position;
        std::string value;
        std::string style;
    };
    std::vector<Specifier> specifiers;
    std::cout << "  Naming Specifiers (e.g., private_member prefix __):\n";
    while (true) {
        std::cout << "    Enter specifier name (or 'done'): ";
        std::string specName;
        std::getline(std::cin, specName);
        if (specName == "done" || specName.empty()) break;
        Specifier s;
        s.name = specName;
        std::cout << "      Position (prefix/suffix): ";
        std::getline(std::cin, s.position);
        std::cout << "      Value (e.g., __): ";
        std::getline(std::cin, s.value);
        std::cout << "      Internal Style (snake_case, etc): ";
        std::getline(std::cin, s.style);
        specifiers.push_back(s);
    }
    std::vector<std::string> rule_types = {
        "vars", "functions", "user_types", "constants",
        "private_members", "private_methods", "namespaces"
    };
    struct RuleMapping {
        std::string rule_name;
        std::string target;
    };
    std::vector<RuleMapping> rule_mappings;

    std::cout << "\n  Mapping Rules:\n";
    for (const auto& rule : rule_types) {
        std::cout << "    Style for " << rule << ": (";
        for (std::string style : casing_styles) {
            if (style != "snake_case") {
                std::cout << " | ";
            }
            std::cout << style;
        }
        std::cout << ") ";
        std::string choice;
        std::getline(std::cin, choice);
        if (std::find(casing_styles.begin(), casing_styles.end(), strip(choice)) == casing_styles.end()) {
            bool is_speci = false;
            for (Specifier sp : specifiers) {
                if (strip(sp.name) == strip(choice)) {
                    is_speci = true;
                }
            }
            if (!is_speci) {
                throw "Invalid casing style or specifier";
            }
        }
        rule_mappings.push_back({rule, strip(choice)});
    }
    std::cout << "\n  Naming Enforcement Level (warning/error/not): (warning) ";
    std::string name_enforce_level;
    std::getline(std::cin, name_enforce_level);
    if (name_enforce_level != "error" && name_enforce_level != "warning" && name_enforce_level != "not") {
        name_enforce_level = "warning";
    }
    std::cout << "  Autocorrect naming? (Y/n): ";
    std::string name_auto;
    std::getline(std::cin, name_auto);
    std::string name_autocorrect = (name_auto == "Y" || name_auto == "y") ? "true" : "false";
    std::cout << "\n\nComment Rules:\n\n";
    std::cout << "  Enter character sequence for normal comments: (//) ";
    std::string comment_normal;
    std::getline(std::cin, comment_normal);
    if (comment_normal.empty()) comment_normal = "//";
    std::cout << "  Enter character sequence for doc comments: (///) ";
    std::string comment_doc;
    std::getline(std::cin, comment_doc);
    if (comment_doc.empty()) comment_doc = "///";
    std::cout << "  Enter character sequence for top-level doc comments: (//!) ";
    std::string top_doc;
    std::getline(std::cin, top_doc);
    if (top_doc.empty()) top_doc = "///";
    std::cout << "  What level should comment rules be enforced (error/warning/not): (warning) ";
    std::string comment_level;
    std::getline(std::cin, comment_level);
    if (comment_level.empty()) {
        comment_level = "warning";
    }
    std::cout << "  What comment style should doc functions (doc, top_level_doc, or normal): (doc) ";
    std::string function_comment;
    std::getline(std::cin, function_comment);
    if (function_comment.empty()) function_comment = "doc";
    std::cout << "  What comment style should doc the top of files (doc, top_level_doc, or normal): (top_level_doc) ";
    std::string file_comment;
    std::getline(std::cin, file_comment);
    if (file_comment.empty()) file_comment = "top_level_doc";
    std::cout << "  What comment style should doc namespaces: (doc) ";
    std::string namespace_comment;
    std::getline(std::cin, namespace_comment);
    if (namespace_comment.empty()) namespace_comment = "doc";
    std::cout << "  What string or special sequence that starts a namespace comment should be used to mark a namespace as 'internal' (no include): (No
Include) "; std::string internal_comment; std::getline(std::cin, internal_comment); if (internal_comment.empty()) internal_comment = "No Include";
    std::cout << "\n\nFile & Include Rules:\n\n";
    std::cout << "  Line ending style (lf/crlf): (lf) ";
    std::string nl_style;
    std::getline(std::cin, nl_style);
    if (nl_style.empty()) nl_style = "lf";
    std::cout << "  Force final newline at end of file? (Y/n) ";
    std::string final_nl_input;
    std::getline(std::cin, final_nl_input);
    std::string final_nl = (final_nl_input == "n" || final_nl_input == "N") ? "false" : "true";
    std::cout << "  Collapse blank lines in apply mode? (Y/n) ";
    std::string collapse_blanks_input;
    std::getline(std::cin, collapse_blanks_input);
    std::string collapse_blanks = (collapse_blanks_input == "n" || collapse_blanks_input == "N") ? "false" : "true";
    std::cout << "  Use quotes for includes? (y/N) ";
    std::string inc_quotes_input;
    std::getline(std::cin, inc_quotes_input);
    std::string inc_quotes = (inc_quotes_input == "y" || inc_quotes_input == "Y") ? "true" : "false";
    std::cout << "\n\nNamespace Rules:\n\n";
    std::cout << "  How many types per namespace: (1) ";
    std::string ns_amount;
    std::getline(std::cin, ns_amount);
    if (ns_amount.empty()) ns_amount = "1";
    std::cout << "\033[?1049l";
    std::ofstream rule_file(std::string(std::filesystem::current_path().c_str()) + "/" + rules_name + ".fmt.yaml");
    rule_file << "formatting:\n";
    rule_file << "  indent_size: " << tabc << "\n";
    rule_file << "  use_tabs: " << use_tabs << '\n';
    rule_file << "  params_per_line: " << param_max << '\n';
    rule_file << "  max_width: " << char_max << '\n';
    rule_file << "  braces:\n";
    rule_file << "    enforce:\n";
    rule_file << "      level: " << brace_enforcement << '\n';
    rule_file << "      autocomplete: " << autocomplete_brace_fixes << '\n';
    rule_file << "    open_brace:\n";
    rule_file << "      before: \"" << before_braces << '"' << '\n';
    rule_file << "  operators:\n    unops:\n";
    rule_file << "      after: \"" << unop_space_after << '"' << '\n';
    rule_file << "    binops:\n";
    rule_file << "      before: \"" << binop_space_before << '"' << '\n';
    rule_file << "      after: \"" << binop_space_after << '"' << '\n';
    rule_file << "naming:\n  specifiers:\n" << '\n';
    for (Specifier spec : specifiers) {
        rule_file << "    " << spec.name << ": \n";
        rule_file << "      position: " << spec.position << '\n';
        rule_file << "      value: " << spec.value << '\n';
        rule_file << "      style: " << spec.style << '\n';
    }
    rule_file << "  rules:\n    enforce:\n";
    rule_file << "      level: " << name_enforce_level << '\n';
    rule_file << "      autocorrect: " << name_autocorrect << '\n';
    for (RuleMapping rule : rule_mappings) {
        rule_file << "    " << rule.rule_name << ": " << rule.target << '\n';
    }
    rule_file << "comments:\n";
    rule_file << "  specifiers:\n";
    rule_file << "    normal: " << comment_normal << "\n";
    rule_file << "    doc: " << comment_doc << "\n";
    rule_file << "    top_level_doc: " << top_doc << "\n";
    rule_file << "  rules:\n";
    rule_file << "    enforce:\n";
    rule_file << "      level: " << comment_level << "\n";
    rule_file << "    functions: " << function_comment << "\n";
    rule_file << "    file: " << file_comment << "\n";
    rule_file << "    namespaces: " << namespace_comment << "\n";
    rule_file << "    internal: \"" << internal_comment << "\"\n";

    rule_file << "newlines:\n";
    rule_file << "  style: " << nl_style << "\n";
    rule_file << "  final_newline: " << final_nl << "\n";
    rule_file << "  collapse_blanks: " << collapse_blanks << "\n";

    rule_file << "includes:\n";
    rule_file << "  filepath:\n";
    rule_file << "    quotes: " << inc_quotes << "\n";
    rule_file << "namespaces:\n";
    rule_file << "  types:\n";
    rule_file << "    amount: " << ns_amount << "\n";
    rule_file << "    enforce: warning\n";
    rule_file.close();
    std::filesystem::rename(std::string(std::filesystem::current_path().c_str()) + "/" + rules_name + ".fmt.yaml", home + "/.qc-fmt/formats/" +
rules_name + ".fmt.yaml"); auto yaml = fkyaml::node::deserialize(fopen((home + std::string("/.qc-fmt/config.yaml")).c_str(), "r"));
    yaml["aliases"][rules_name] = home + "/.qc-fmt/formats/" + rules_name + ".fmt.yaml";
    auto config = std::ofstream(home + "/.qc-fmt/config.yaml");
    config << yaml;
    config.close();
    std::cout << "Rule .fmt.yaml file created successfuly!" << '\n';
}
void alias(int count, char** args) {
    if (count < 3) {
        throw "Too few args";
    }
    char* chome = std::getenv("HOME");
    if (chome == nullptr) {
        throw "HOME not set";
    }
    std::string home(chome);
    if (!std::filesystem::exists(home + "/.qc-fmt/config.yaml")) {
        throw "Please run `qc fmt setup`";
    }
    auto config_node = fkyaml::node::deserialize(fopen((home + "/.qc-fmt/config.yaml").c_str(), "r"));
    if (std::string(args[2]) == "-d" || std::string(args[2]) == "--delete") {
        if (count < 4) {
            throw "Too few args";
        }
        std::string target = args[3];
        if (config_node["aliases"].contains(target)) {
            if (target == "default") {
                std::cout << "Cannot delete the default alias." << std::endl;
            } else {
                std::filesystem::remove(config_node["aliases"][target].get_value<std::string>());
                config_node["aliases"].as_map().erase(target);
                auto config = std::ofstream(home + "/.qc-fmt/config.yaml");
                config << fkyaml::node::serialize(config_node);
                config.close();
                std::cout << "Alias '" << target << "' removed." << std::endl;
            }
        } else {
            throw "Alias " + target + " doesn't exist.";
        }
    } else if (std::string(args[2]) == "-l" || std::string(args[2]) == "--list") {
        auto aliases = config_node["aliases"];
        std::cout << "Current Formatting Aliases:\n";
        std::cout << "---------------------------\n";
        for (auto it = aliases.begin(); it != aliases.end(); ++it) {
            std::cout << "  " << it.key().get_value<std::string>() << " -> " << it.value().get_value<std::string>() << "\n";
        }
    } else {
        if (count < 4) {
            throw "Too few args";
        }
        std::string alias_name = args[2];
        std::filesystem::path alias_path(args[3]);
        if (!alias_path.empty() && alias_path.string()[0] == '~') {
            alias_path = std::filesystem::path(home) / alias_path.string().substr(2);
        }
        std::cout << "Importing " << alias_path.filename() << " into ~/.qc-fmt/formats/..." << '\n';
        std::filesystem::rename(alias_path, home + "/.qc-fmt/formats/" + std::string(alias_path.filename().c_str()));
        config_node["aliases"][alias_name] = home + "/.qc-fmt/formats/" + std::string(alias_path.filename().c_str());
        std::ofstream config_out(home + "/.qc-fmt/config.yaml");
        config_out << fkyaml::node::serialize(config_node);
        config_out.close();
    }
}
void list_conventions(int count, char** args) {
    char* chome = std::getenv("HOME");
    if (chome == nullptr) {
        throw "HOME not set";
    }
    if (count < 3) {
        throw "Too few args";
    }
    std::string home(chome);
    if (!std::filesystem::exists(home + "/.qc-fmt/config.yaml")) {
        throw "Please run `qc fmt setup`";
    }
    auto config = fkyaml::node::deserialize(fopen((home + "/.qc-fmt/config.yaml").c_str(), "r"));
    std::string active_path = config["aliases"][args[2]].get_value<std::string>();
    auto rules = fkyaml::node::deserialize(fopen(active_path.c_str(), "r"));
    std::cout << "Active Conventions (" << active_path << "):\n";
    std::cout << "------------------------------------------\n";
    std::cout << "Formatting:\n";
    std::cout << "  Indentation: " << rules["formatting"]["indent_size"]
              << (rules["formatting"]["use_tabs"].get_value<bool>() ? " (tabs)" : " (spaces)") << "\n";
    std::cout << "\nNaming Rules:\n";
    auto naming = rules["naming"]["rules"];
    for (auto it = naming.begin(); it != naming.end(); ++it) {
        if (it.key().get_value<std::string>() != "enforce") {
             std::cout << "  " << it.key().get_value<std::string>()
                       << ": " << it.value().get_value<std::string>() << "\n";
        }
    }
    std::cout << "--- Formatting Rules (as raw yaml) ---" << '\n';
    std::cout << fkyaml::node::serialize(rules) << '\n';
}
struct Command {
    std::string name;
    std::function<void(int count, char** args)> command;
};
std::vector<Command> commands = {{"setup", setup}, {"help", help}, {"create", create}, {"alias", alias}, {"list", list_conventions}};
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Too few args. Try qc fmt help" << '\n';
        return 1;
    }
    for (Command comm : commands) {
        if (argv[1] == comm.name) {
            try {
                comm.command(argc, argv);
                return 0;
            } catch (std::string e) {
                std::cout << "FmtError: " << e << '\n';
            } catch (const char* e) {
                std::cout << "FmtError: " << e << '\n';
            } catch (char* e) {
                std::cout << "FmtError: " << e << '\n';
            } catch (std::runtime_error e) {
                std::cout << "FmtError: " << e.what() << '\n';
            } catch (const std::exception& e) {
                std::cerr << "FmtError: " << e.what() << '\n';
            } catch (...) {
                std::cout << "Unknown error." << '\n';
            }
            return 1;
        }
    }
    std::cout << "Unknown command" << '\n';
    return 1;
}
*/
