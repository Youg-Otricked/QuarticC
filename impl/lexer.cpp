#include "lexer.h"
#include <algorithm>
bool isCharInSet(char c, const std::string& charSet) {
    return charSet.find(c) != std::string::npos;
}
Lexer::Lexer(std::string text, std::string filename) {
    this->Filename = filename;
    this->pos = Position(SourceManager::instance().add_file(filename, text), -1, 0, -1);
    this->text = text;
    this->current_char = '\0';
    this->advance();
}

void Lexer::advance() {
    this->pos.advance(this->current_char);
    if (this->pos.index < this->text.length()) {
        this->current_char = this->text[this->pos.index];
    } else {
        this->current_char = '\0';
    }
}
Token Lexer::make_number() {
    Position start_pos = this->pos.copy();
    std::string num = "";
    int dot_count = 0;
    bool is_float = false;
    bool is_octal = false;
    bool is_binary = false;
    bool is_long = false;
    bool is_short = false;
    bool is_hex = false;
    bool is_addrt = false;
    bool is_byte = false;
    bool is_nibble = false;
    if (this->current_char == '0') {
        switch (this->text[this->pos.index + 1]) {
        case 'x':
        case 'X':
            is_hex = true;
            this->advance();
            this->advance();
            break;
        case 'o':
        case 'O':
            is_octal = true;
            this->advance();
            this->advance();
            break;
        case 'b':
        case 'B':
            is_binary = true;
            this->advance();
            this->advance();
            break;
        }
    }
    if (is_hex) {
        while (this->current_char != '\0' && isCharInSet(this->current_char, DIGITS + "abcdefABCDEF'")) {
            if (this->current_char == '\'') { this->advance(); }
            num += this->current_char;
            this->advance();
        }
        size_t val = std::stoull(num, nullptr, 16);
        if (this->current_char == 'l') {
            this->advance();
            return Token(TokenType::LONG_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'i') {
            this->advance();
            return Token(TokenType::INT, std::to_string(val), start_pos);
        } else if (this->current_char == 's') {
            this->advance();
            return Token(TokenType::SHORT_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'y') {
            this->advance();
            return Token(TokenType::BYTE, std::to_string(val), start_pos);
        } else if (this->current_char == 'n') {
            this->advance();
            return Token(TokenType::NIBBLE, std::to_string(val), start_pos);
        }
        return Token(TokenType::ADDR_T, std::to_string(val), start_pos);
    } else if (is_octal) {
        while (this->current_char != '\0' && ((std::isdigit(this->current_char) && this->current_char - '0' < 8) || this->current_char == '\'')) {
            if (this->current_char == '\'') { this->advance(); }
            num += this->current_char;
            this->advance();
        }
        size_t val = std::stoull(num, nullptr, 8);
        if (this->current_char == 'l') {
            this->advance();
            return Token(TokenType::LONG_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'i') {
            this->advance();
            return Token(TokenType::INT, std::to_string(val), start_pos);
        } else if (this->current_char == 's') {
            this->advance();
            return Token(TokenType::SHORT_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'y') {
            this->advance();
            return Token(TokenType::BYTE, std::to_string(val), start_pos);
        } else if (this->current_char == 'n') {
            this->advance();
            return Token(TokenType::NIBBLE, std::to_string(val), start_pos);
        }
        return Token(TokenType::ADDR_T, std::to_string(val), start_pos);
    } else if (is_binary) {
        while (this->current_char != '\0' && ((std::isdigit(this->current_char) && this->current_char - '0' < 2) || this->current_char == '\'')) {
            if (this->current_char == '\'') { this->advance(); }
            num += this->current_char;
            this->advance();
        }
        size_t val = std::stoull(num, nullptr, 2);
        if (this->current_char == 'l') {
            this->advance();
            return Token(TokenType::LONG_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'i') {
            this->advance();
            return Token(TokenType::INT, std::to_string(val), start_pos);
        } else if (this->current_char == 's') {
            this->advance();
            return Token(TokenType::SHORT_INT, std::to_string(val), start_pos);
        } else if (this->current_char == 'y') {
            this->advance();
            return Token(TokenType::BYTE, std::to_string(val), start_pos);
        } else if (this->current_char == 'n') {
            this->advance();
            return Token(TokenType::NIBBLE, std::to_string(val), start_pos);
        }
        return Token(TokenType::ADDR_T, std::to_string(val), start_pos);
    } else {
        while (this->current_char != '\0' && isCharInSet(this->current_char, DIGITS + ".flsayn'")) {
            if (this->current_char == '.') {
                if (dot_count == 1 || is_hex || is_binary || is_octal) {
                    this->advance();
                    break;
                }
                dot_count++;
                num += ".";
                this->advance();
            } else if (this->current_char == 'f') {
                is_float = true;
                this->advance();
                break;
            } else if (this->current_char == 'l') {
                is_long = true;
                this->advance();
                break;
            } else if (this->current_char == 's') {
                is_short = true;
                this->advance();
                break;
            } else if (this->current_char == 'a') {
                is_addrt = true;
                this->advance();
                break;
            } else if (this->current_char == 'y') {
                this->advance();
                is_byte = true;
                break;
            } else if (this->current_char == 'n') {
                this->advance();
                is_nibble = true;
                break;
            } else if (this->current_char == '\'') {
                if (num.empty() || !std::isdigit(static_cast<unsigned char>(this->text[this->pos.index + 1]))) {
                    throw new InvalidSyntaxError("QC-IC03: ' delimiter must appear between digits", start_pos);
                }
                this->advance();
            } else {
                num += this->current_char;
                this->advance();
            }
        }
    }
    if (dot_count == 1) {
        if (is_float) { return Token(TokenType::FLOAT, num, start_pos); }
        if (is_long) return Token(TokenType::LONG_DOUBLE, num, start_pos);
        return Token(TokenType::DOUBLE, num, start_pos);
    }
    if (is_byte) return Token(TokenType::BYTE, num, start_pos);
    if (is_nibble) return Token(TokenType::NIBBLE, num, start_pos);
    if (is_addrt) return Token(TokenType::ADDR_T, num, start_pos);
    if (is_long) return Token(TokenType::LONG_INT, num, start_pos);
    if (is_short) return Token(TokenType::SHORT_INT, num, start_pos);
    return Token(TokenType::INT, num, start_pos);
}
Token Lexer::make_identifier() {
    std::string id = "";
    Position start_pos = this->pos.copy();
    while (this->current_char != '\0' && (isalnum(this->current_char) || this->current_char == '_' || this->current_char == '`')) {
        id += this->current_char;
        this->advance();
    }
    if (
        /* primitives */ id == "int" || id == "float" || id == "double" || id == "bool" || id == "string" || id == "qbool" || id == "char" ||
        /* storage modifiers */ id == "long" || id == "short" || id == "const" || id == "atomic" ||
        /* switch */ id == "case" || id == "switch" || id == "default" || id == "match" ||
        /* if else */ id == "if" || id == "else" ||
        /* loops */ id == "break" || id == "while" || id == "loop" || id == "do" || id == "for" || id == "continue" || id == "foreach" ||
        id == "in" || id == "unreachable" ||
        /* special types */ id == "void" || id == "auto" ||
        /* functions / lambdas */ id == "return" || id == "fn" ||
        /* q stuff */ id == "qif" || id == "qelse" || id == "qelif" || id == "qswitch" ||
        /* usertypes */ id == "class" || id == "struct" || id == "enum" || id == "type" ||
        /* extern */ id == "foreign" || id == "extern" ||
        /* namespace */ id == "namespace" || // i̶m̶ ̶t̶o̶o̶ ̶l̶a̶z̶y̶ ̶t̶o̶ ̶d̶o̶ ̶t̶h̶e̶ ̶r̶e̶s̶t̶ ̶o̶f̶ ̶t̶h̶i̶s̶ ̶r̶e̶o̶r̶g̶i̶n̶i̶z̶a̶t̶i̶o̶n̶ nevermind i did it. why did i do this. Also yes i am
                                             // using neovim and did just manually type
        // <C-v>u0336 1... 2... 51 times. And yes you did just count them. And yes, there are probably 50 or 52. It should be a unordered set. Im not
        // making it a unordered_set. 64 or's are faster. Yes i did count them. yes i am probably off.
        /* operator */ id == "roperator" || id == "operator" ||
        /* try/catch */ id == "try" || id == "catch" ||
        /* nullptr */ id == "nullptr" ||
        /* storage modifiers */ id == "out" || id == "inout" || id == "volatile" || id == "restrict" ||
        /* more types */ id == "byte" || id == "nibble" || id == "addr_t" ||
        /* class stuff */ id == "friend" || id == "friendly" || id == "static" || id == "abstract" || id == "final" ||
        /* more  class stuff */ id == "public" || id == "protected" || id == "private" || id == "virtual" || id == "override" ||
        /* defer */ id == "defer" ||
        /* concepts */ id == "concept" || id == "proves" || id == "with_proof" || id == "_of" || id == "at_least" || id == "all_of" ||
        id == "proved_by" ||
        /* modifiers */ id == "modifier" || id == "on_call" || id == "on_return" || id == "on_use" ||
        /* comptime */ id == "comptime") {
        return Token(TokenType::KEYWORD, id, start_pos);
    }
    if (id == "true" || id == "false") { return Token(TokenType::BOOL, id, start_pos); }
    if (id == "qtrue" || id == "qfalse" || id == "both" || id == "none") { return Token(TokenType::QBOOL, id, start_pos); }
    if (id == "sizeof") { return Token(TokenType::SIZEOF, id, start_pos); }
    if (id == "throw") { return Token(TokenType::THROW, id, start_pos); }
    if (id == "as") { return Token(TokenType::AS, id, start_pos); }
    if (id == "typeof") { return Token(TokenType::TYPEOF, id, start_pos); }
    return Token(TokenType::IDENTIFIER, id, start_pos);
}
Token Lexer::make_string() {
    std::string str = "";
    Position start_pos = this->pos.copy();
    bool escape_character = false;
    this->advance();
    while (this->current_char != '\0' && (this->current_char != '"' || escape_character)) {
        if (escape_character) {
            switch (this->current_char) {
            case 'a': str += '\a'; break;
            case 'b': str += '\b'; break;
            case 't': str += '\t'; break;
            case 'n': str += '\n'; break;
            case 'v': str += '\v'; break;
            case 'f': str += '\f'; break;
            case 'r': str += '\r'; break;
            case 'e': str += '\x1B'; break;
            case '\\': str += '\\'; break;
            case '"': str += '"'; break;
            case 'x': {
                escape_character = false;
                this->advance();
                if (!std::isxdigit(static_cast<unsigned char>(this->current_char))) {
                    throw IllegalCharError("QC-IC02: Expected hex digit after \\x", this->pos);
                }
                std::string hex = "";
                while (std::isxdigit(this->current_char) && hex.size() < 2) {
                    hex += this->current_char;
                    this->advance();
                }
                if (hex.size() != 2) { throw IllegalCharError("QC-IC02: Expected two hex digits after \\x", this->pos); }

                str += static_cast<char>(std::stoi(hex, nullptr, 16));
                continue;
            }
            default:
                if (this->current_char >= '0' && this->current_char <= '7') {
                    escape_character = false;
                    std::string octal;
                    for (int i = 0; i < 3 && this->current_char >= '0' && this->current_char <= '7'; ++i) {
                        octal += this->current_char;
                        this->advance();
                    }
                    str += static_cast<char>(std::stoi(octal, nullptr, 8));
                    continue;
                }
                str += this->current_char;
                break;
            }
            escape_character = false;
        } else {
            if (this->current_char == '\\') {
                escape_character = true;
            } else {
                str += this->current_char;
            }
        }
        this->advance();
    }

    if (this->current_char != '"') {
        this->advance();
        throw IllegalCharError("QC-IC01: Expected \"", this->pos);
    }

    this->advance();
    return Token(TokenType::STRING, str, start_pos);
}
Token Lexer::make_char() {
    Position start_pos = this->pos.copy();
    this->advance();
    bool already_advanced = false;
    std::string val = "";
    if (this->current_char == '\\') {
        this->advance();
        switch (this->current_char) {
        case 'e': val = std::string(1, '\x1B'); break;
        case 'a': val = std::string(1, '\a'); break;
        case 'b': val = std::string(1, '\b'); break;
        case 't': val = std::string(1, '\t'); break;
        case 'n': val = std::string(1, '\n'); break;
        case 'v': val = std::string(1, '\v'); break;
        case 'f': val = std::string(1, '\f'); break;
        case 'r': val = std::string(1, '\r'); break;
        case '\\': val = std::string(1, '\\'); break;
        case '\'': val = std::string(1, '\''); break;
        case '"': val = std::string(1, '"'); break;
        case 'x': {
            this->advance();
            std::string hex;

            for (int i = 0; i < 2; ++i) {
                if (!std::isxdigit(static_cast<unsigned char>(this->current_char))) {
                    throw IllegalCharError("QC-IC01: Invalid hex escape", this->pos);
                }

                hex += this->current_char;
                this->advance();
            }

            val = std::string(1, static_cast<char>(std::stoi(hex, nullptr, 16)));
            already_advanced = true;
            break;
        }
        default:
            if (this->current_char >= '0' && this->current_char <= '7') {
                std::string octal;
                for (int i = 0; i < 3 && this->current_char >= '0' && this->current_char <= '7'; ++i) {
                    octal += this->current_char;
                    this->advance();
                }
                already_advanced = true;
                val = std::string(1, static_cast<char>(std::stoi(octal, nullptr, 8)));
            } else {
                val = std::string(1, this->current_char);
                break;
            }
        }
        if (!already_advanced) this->advance();
    } else {
        val = std::string(1, this->current_char);
        this->advance();
    }
    if (this->current_char != '\'') { throw IllegalCharError("QC-IC01: Expected closing single quote", this->pos); }
    this->advance();

    return Token(TokenType::CHAR, val, start_pos);
}
Token Lexer::make_fstring() {
    Position start_pos = this->pos.copy();
    this->advance();

    std::vector<std::string> parts;
    std::vector<std::string> exprs;

    std::string current = "";
    bool escape = false;

    while (this->current_char != '\0' && (this->current_char != '"' || escape)) {
        if (escape) {
            switch (this->current_char) {
            case 'e': current += '\x1B'; break;
            case 'a': current += '\a'; break;
            case 'b': current += '\b'; break;
            case 't': current += '\t'; break;
            case 'n': current += '\n'; break;
            case 'v': current += '\v'; break;
            case 'f': current += '\f'; break;
            case 'r': current += '\r'; break;
            case '\\': current += '\\'; break;
            case '"': current += '"'; break;
            case 'x': {
                escape = false;
                this->advance();
                if (!std::isxdigit(this->current_char)) { throw IllegalCharError("QC-IC02: Expected hex digit after \\x", this->pos); }
                std::string hex = "";
                while (std::isxdigit(this->current_char) && hex.size() < 2) {
                    hex += this->current_char;
                    this->advance();
                }
                if (hex.size() != 2) { throw IllegalCharError("QC-IC02: Expected two hex digits after \\x", this->pos); }
                current += static_cast<char>(std::stoi(hex, nullptr, 16));
                continue;
            }
            default:
                escape = false;
                if (this->current_char >= '0' && this->current_char <= '7') {
                    std::string octal;
                    for (int i = 0; i < 3 && this->current_char >= '0' && this->current_char <= '7'; ++i) {
                        octal += this->current_char;
                        this->advance();
                    }
                    current += static_cast<char>(std::stoi(octal, nullptr, 8));
                    continue;
                }
                current += this->current_char;
                break;
            }
            escape = false;
            this->advance();
            continue;
        }

        if (this->current_char == '\\') {
            escape = true;
            this->advance();
            continue;
        }

        if (this->current_char == '{' && this->text[this->pos.index + 1] == '{') {
            current += '{';
            this->advance();
            this->advance();
            continue;
        }

        if (this->current_char == '}' && this->text[this->pos.index + 1] == '}') {
            current += '}';
            this->advance();
            this->advance();
            continue;
        }

        if (this->current_char == '{') {
            parts.push_back(current);
            current = "";
            this->advance();

            std::string expr = "";
            int brace_depth = 1;

            while (this->current_char != '\0' && brace_depth > 0) {
                if (this->current_char == '{')
                    brace_depth++;
                else if (this->current_char == '}')
                    brace_depth--;

                if (brace_depth > 0) expr += this->current_char;

                this->advance();
            }

            if (brace_depth != 0) throw IllegalCharError("QC-IC02: Unclosed brace in f-string", this->pos);

            exprs.push_back(expr);
        } else {
            current += this->current_char;
            this->advance();
        }
    }

    parts.push_back(current);

    if (this->current_char != '"') throw IllegalCharError("QC-IC02: Unterminated f-string", this->pos);
    this->advance();
    std::string encoded = "";
    for (size_t i = 0; i < parts.size(); i++) {
        encoded += parts[i];
        if (i < exprs.size()) encoded += "\x01" + exprs[i] + "\x01";
    }
    return Token(TokenType::FSTRING, encoded, start_pos);
}
Token Lexer::make_raw_string() {
    Position start_pos = this->pos.copy();
    std::string start_delim = "\"";
    this->advance();
    while (this->current_char != '\0' && this->current_char != '(' && this->current_char != '\\' && this->current_char != ' ') {
        start_delim += this->current_char;
        this->advance();
    }
    start_delim += '(';
    this->advance();
    std::string end_marker = start_delim;
    std::reverse(end_marker.begin(), end_marker.end());
    for (char& c : end_marker) {
        if (c == '(')
            c = ')';
        else if (c == ')')
            c = '(';
        else if (c == '[')
            c = ']';
        else if (c == ']')
            c = '[';
        else if (c == '{')
            c = '}';
        else if (c == '}')
            c = '{';
        else if (c == '<')
            c = '>';
        else if (c == '>')
            c = '<';
    }
    std::string value = "";
    while (this->current_char != '\0') {
        value += this->current_char;
        this->advance();
        if (value.ends_with(end_marker)) {
            value = value.substr(0, value.length() - end_marker.length());
            break;
        }
    }
    return Token(TokenType::STRING, value, start_pos);
}
Ler Lexer::make_tokens() {
    std::vector<Token> tokens;
    tokens.reserve(256);
    while (this->current_char != '\0') {
        Position start_pos = this->pos.copy();
        if (isCharInSet(this->current_char, bad_chars)) {
            this->advance();
            continue;
        } else if (isCharInSet(this->current_char, DIGITS)) {
            tokens.push_back(this->make_number());
            continue;
        } else if (this->current_char == 'f' && this->text[this->pos.index + 1] == '"') {
            this->advance();
            tokens.push_back(this->make_fstring());
            continue;
        } else if (this->current_char == 'R' && this->text[this->pos.index + 1] == '"') {
            this->advance();
            tokens.push_back(this->make_raw_string());
            continue;
        } else if (isCharInSet(this->current_char, LETTERS + "_`")) {
            tokens.push_back(this->make_identifier());
        } else if (this->current_char == '"') {
            tokens.push_back(this->make_string());
            continue;
        } else if (this->current_char == '\'') {
            tokens.push_back(this->make_char());
        } else {
            switch (this->current_char) {
            case '+':
                this->advance();
                if (current_char == '+') {
                    this->advance();
                    tokens.push_back(Token(TokenType::INCREMENT, "++", start_pos));
                } else if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::PLUS_EQ, "+=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::PLUS, "+", start_pos));
                }
                break;

            case '-':
                this->advance();
                if (current_char == '-') {
                    this->advance();
                    tokens.push_back(Token(TokenType::DECREMENT, "--", start_pos));
                } else if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::MINUS_EQ, "-=", start_pos));
                } else if (current_char == '>') {
                    this->advance();
                    tokens.push_back(Token(TokenType::ARROW, "->", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::MINUS, "-", start_pos));
                }
                break;
            case '*':
                this->advance();
                if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::MUL_EQ, "*=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::MUL, "*", start_pos));
                    break;
                }
                break;
            case '/':
                this->advance();
                if (this->current_char == '/') {
                    while (this->current_char != '\0' && this->current_char != '\n') { this->advance(); }
                    continue;
                } else if (this->current_char == '*') {
                    this->advance();
                    while (this->current_char != '\0') {
                        if (this->current_char == '*') {
                            this->advance();
                            if (this->current_char == '/') {
                                this->advance();
                                break;
                            }
                        } else {
                            this->advance();
                        }
                    }
                    continue;
                } else if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::DIV_EQ, "/=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::DIV, "/", start_pos));
                }
                break;
            case '=':
                this->advance();
                if (current_char == '=') {
                    this->advance();
                    if (current_char == '=') {
                        this->advance();
                        tokens.push_back(Token(TokenType::QEQEQ, "===", start_pos));
                    } else {
                        tokens.push_back(Token(TokenType::EQ_TO, "==", start_pos));
                    }
                } else if (current_char == '>') {
                    this->advance();
                    tokens.push_back(Token(TokenType::BIG_ARROW, "=>", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::EQ, "=", start_pos));
                    break;
                }
                break;
            case '!':
                this->advance();
                if (current_char == '=') {
                    this->advance();
                    if (current_char == '=') {
                        this->advance();
                        tokens.push_back(Token(TokenType::QNEQ, "!==", start_pos));
                    } else {
                        tokens.push_back(Token(TokenType::NOT_EQ, "!=", start_pos));
                    }
                } else if (current_char == '!') {
                    this->advance();
                    tokens.push_back(Token(TokenType::QNOT, "!!", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::NOT, "!", start_pos));
                    break;
                }
                break;
            case '>':
                this->advance();
                if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::MORE_EQ, ">=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::MORE, ">", start_pos));
                    break;
                }
                break;
            case '<':
                this->advance();
                if (current_char == '<') {
                    this->advance();
                    if (current_char == '<') {
                        this->advance();
                        if (this->current_char == '=') {
                            this->advance();
                            tokens.push_back(Token(TokenType::LROT_EQ, "<<<=", start_pos));
                            break;
                        }
                        tokens.push_back(Token(TokenType::L_ROT, "<<<", start_pos));
                    } else {
                        if (this->current_char == '=') {
                            this->advance();
                            tokens.push_back(Token(TokenType::LSH_EQ, "<<=", start_pos));
                            break;
                        }
                        tokens.push_back(Token(TokenType::LSHIFT, "<<", start_pos));
                    }
                } else if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::LESS_EQ, "<=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::LESS, "<", start_pos));
                    break;
                }
                break;
            case '(':
                tokens.push_back(Token(TokenType::LPAREN, "(", start_pos));
                this->advance();
                break;
            case ')':
                tokens.push_back(Token(TokenType::RPAREN, ")", start_pos));
                this->advance();
                break;
            case '{':
                tokens.push_back(Token(TokenType::LBRACE, "{", start_pos));
                this->advance();
                break;
            case '}':
                tokens.push_back(Token(TokenType::RBRACE, "}", start_pos));
                this->advance();
                break;
            case '[':
                tokens.push_back(Token(TokenType::LBRACKET, "[", start_pos));
                this->advance();
                break;
            case ']':
                tokens.push_back(Token(TokenType::RBRACKET, "]", start_pos));
                this->advance();
                break;
            case '%':
                this->advance();
                if (current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::MOD_EQ, "%=", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::MOD, "%", start_pos));
                    break;
                }
                break;
            case '&':
                this->advance();
                if (current_char == '&') {
                    this->advance();
                    if (current_char == '&') {
                        this->advance();
                        tokens.push_back(Token(TokenType::QAND, "&&&", start_pos));
                    } else {
                        tokens.push_back(Token(TokenType::AND, "&&", start_pos));
                    }
                } else if (this->current_char == '|') {
                    this->advance();
                    if (this->current_char == '&') {
                        this->advance();
                        tokens.push_back(Token(TokenType::COLLAPSE_AND, "&|&", start_pos));
                    }
                } else {
                    if (this->current_char == '=') {
                        this->advance();
                        tokens.push_back(Token(TokenType::BIT_A_EQ, "&=", start_pos));
                        break;
                    }
                    tokens.push_back(Token(TokenType::AMPERSAND, "&", start_pos));
                    break;
                }
                break;
            case '|':
                this->advance();
                if (current_char == '|') {
                    this->advance();
                    if (current_char == '|') {
                        this->advance();
                        tokens.push_back(Token(TokenType::QOR, "|||", start_pos));
                    } else {
                        tokens.push_back(Token(TokenType::OR, "||", start_pos));
                    }
                } else if (this->current_char == '&') {
                    this->advance();
                    if (this->current_char == '|') {
                        this->advance();
                        tokens.push_back(Token(TokenType::COLLAPSE_OR, "|&|", start_pos));
                    }
                } else if (this->current_char == '>') {
                    this->advance();
                    if (this->current_char == '>') {
                        this->advance();
                        if (this->current_char == '=') {
                            this->advance();
                            tokens.push_back(Token(TokenType::RROT_EQ, "|>>=", start_pos));
                            break;
                        }
                        tokens.push_back(Token(TokenType::R_ROT, "|>>", start_pos));
                    } else {
                        if (this->current_char == '=') {
                            this->advance();
                            tokens.push_back(Token(TokenType::RSH_EQ, "|>=", start_pos));
                            break;
                        }
                        tokens.push_back(Token(TokenType::RSHIFT, "|>", start_pos));
                    }
                } else {
                    if (this->current_char == '=') {
                        this->advance();
                        tokens.push_back(Token(TokenType::BIT_O_EQ, "|=", start_pos));
                        break;
                    }
                    tokens.push_back(Token(TokenType::PIPE, "|", start_pos));
                    break;
                }
                break;
            case '@':
                this->advance();
                tokens.push_back(Token(TokenType::AT, "@", start_pos));
                break;
            case '^':
                this->advance();
                if (current_char == '^') {
                    this->advance();
                    tokens.push_back(Token(TokenType::QXOR, "^^", start_pos));
                } else {
                    tokens.push_back(Token(TokenType::XOR, "^", start_pos));
                }
                break;
            case '#':
                this->advance();
                if (current_char == '^') {
                    this->advance();
                    tokens.push_back(Token(TokenType::POWER, "#^", start_pos));
                    break;
                }
                break;
            case ',':
                this->advance();
                tokens.push_back(Token(TokenType::COMMA, ",", start_pos));
                break;
            case ':':
                this->advance();
                if (current_char == ':') {
                    this->advance();
                    tokens.push_back(Token(TokenType::SCOPE, "::", start_pos));
                    break;
                } else if (current_char == '>') {
                    this->advance();
                    if (this->current_char == '=') {
                        this->advance();
                        tokens.push_back(Token(TokenType::LRSH_EQ, ":>=", start_pos));
                        break;
                    }
                    tokens.push_back(Token(TokenType::LOGICAL_RSHIFT, ":>", start_pos));
                    break;
                } else {
                    tokens.push_back(Token(TokenType::COLON, ":", start_pos));
                }
                break;
            case ';':
                tokens.push_back(Token(TokenType::SEMICOLON, ";", start_pos));
                this->advance();
                break;
            case '.':
                this->advance();
                if (this->current_char == '.') {
                    this->advance();
                    if (this->current_char == '.') {
                        this->advance();
                        tokens.push_back(Token(TokenType::VARADIC, "...", start_pos));
                        break;
                    }
                }
                tokens.push_back(Token(TokenType::DOT, ".", start_pos));
                break;
            case '$':
                this->advance();
                if (this->current_char == '=') {
                    this->advance();
                    tokens.push_back(Token(TokenType::BIT_X_EQ, "$=", start_pos));
                    break;
                }
                tokens.push_back(Token(TokenType::BITWISE_XOR, "$", start_pos));
                break;
            case '~':
                tokens.push_back(Token(TokenType::BITWISE_NOT, "~", start_pos));
                this->advance();
                break;
            case '?':
                tokens.push_back(Token(TokenType::QUESTION, "?", start_pos));
                this->advance();
                break;
            default:
                std::string unknown = std::string(1, this->current_char);
                return Ler{std::vector<Token>(), new IllegalCharError("QC-IC03:" + unknown, this->pos)};
            }
        }
    }
    tokens.push_back(Token(TokenType::EOFT, "<eof>", this->pos));
    return Ler{tokens, NULL};
}
