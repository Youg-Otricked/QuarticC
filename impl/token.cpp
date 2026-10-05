#include "token.h"
#include <sstream>
SourceManager& SourceManager::instance() {
    static SourceManager sm;
    return sm;
}
std::optional<uint32_t> SourceManager::get_id(const std::string& filepath) const {
    if (filepath == "<scratch>") return Position::INVALID_FILE_ID;
    auto it = path_to_id.find(filepath);
    if (it != path_to_id.end()) { return it->second; }
    return std::nullopt;
}
uint32_t SourceManager::add_file(std::string filepath, std::string text) {
    if (filepath == "<scratch>") return Position::INVALID_FILE_ID;
    if (auto id = get_id(filepath)) { return *id; }
    uint32_t new_id = static_cast<uint32_t>(files.size());
    path_to_id[filepath] = new_id;
    files.push_back({std::move(filepath), std::move(text)});
    return new_id;
}
bool SourceManager::has_id(uint32_t id) const {
    return id < files.size();
}
const SourceFile& SourceManager::get(uint32_t id) const {
    if (id == Position::INVALID_FILE_ID) return INVALID;
    return files.at(id);
}
Position::Position() {
}
std::string Position::string(size_t context) const {
    const auto& file = SourceManager::instance().get(this->file_id);
    if (file.content.empty() || index > file.content.size()) { return "\n"; }
    std::vector<std::string> lines;
    std::stringstream ss(file.content);
    std::string temp;
    while (std::getline(ss, temp)) { lines.push_back(temp); }
    if (lines.empty()) return "\n";
    size_t current = std::min<size_t>(line, lines.size() - 1);
    size_t first = (current >= context) ? current - context : 0;
    size_t last = std::min(current + context, lines.size() - 1);
    std::string result;
    size_t width = std::to_string(last + 1).size();
    for (size_t i = first; i <= last; i++) {
        std::string num = std::to_string(i + 1);
        result += "  ";
        result += std::string(width - num.size(), ' ');
        result += num;
        result += " | ";
        result += lines[i];
        result += "\n";
    }
    return result;
}
std::string Position::arrow_string(size_t context) const {
    const auto& file = SourceManager::instance().get(this->file_id);
    if (file.content.empty() || index > file.content.size()) { return "\n"; }
    std::vector<std::string> lines;
    std::stringstream ss(file.content);
    std::string temp;
    while (std::getline(ss, temp)) { lines.push_back(temp); }
    if (lines.empty()) return "\n";
    size_t current = std::min<size_t>(line, lines.size() - 1);
    size_t first = (current >= context) ? current - context : 0;
    size_t last = std::min(current + context, lines.size() - 1);
    std::string result;
    size_t width = std::to_string(last + 1).size();
    for (size_t i = first; i <= last; i++) {
        std::string num = std::to_string(i + 1);
        result += "  ";
        result += std::string(width - num.size(), ' ');
        result += num;
        result += " | ";
        result += lines[i];
        result += "\n";
        if (i == current) {
            result += "  ";
            result += std::string(width, ' ');
            result += " | ";
            size_t col = column;
            size_t len = (length < 1) ? 1 : length;
            result += std::string(col, ' ');
            result += std::string(len, '^');
            result += "\n";
        }
    }
    return result;
}
Position::Position(uint32_t file_id, size_t index, size_t line, size_t column) {
    this->file_id = file_id;
    this->index = index;
    this->line = line;
    this->column = column;
}

void Position::advance(char current_char) {
    this->index++;
    this->column++;
    if (current_char == '\n') {
        this->line++;
        this->column = 0;
    }
}
TokenType stringToTokenType(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    static const std::unordered_map<std::string, TokenType> stringToEnum = {{"INT", TokenType::INT},
                                                                            {"STRING", TokenType::STRING},
                                                                            {"FLOAT", TokenType::FLOAT},
                                                                            {"DOUBLE", TokenType::DOUBLE},
                                                                            {"CHAR", TokenType::CHAR},
                                                                            {"BOOL", TokenType::BOOL},
                                                                            {"QBOOL", TokenType::QBOOL},
                                                                            {"PLUS", TokenType::PLUS},
                                                                            {"R_ROT", TokenType::R_ROT},
                                                                            {"L_ROT", TokenType::L_ROT},
                                                                            {"RROT_EQ", TokenType::RROT_EQ},
                                                                            {"LROT_EQ", TokenType::LROT_EQ},
                                                                            {"RSH_EQ", TokenType::RSH_EQ},
                                                                            {"LSH_EQ", TokenType::LSH_EQ},
                                                                            {"LRSH_EQ", TokenType::LRSH_EQ},
                                                                            {"BIT_X_EQ", TokenType::BIT_X_EQ},
                                                                            {"BIT_A_EQ", TokenType::BIT_A_EQ},
                                                                            {"BIT_O_EQ", TokenType::BIT_O_EQ},
                                                                            {"AMPERSAND", TokenType::AMPERSAND},
                                                                            {"PIPE", TokenType::PIPE},
                                                                            {"RSHIFT", TokenType::RSHIFT},
                                                                            {"LSHIFT", TokenType::LSHIFT},
                                                                            {"BITWISE_XOR", TokenType::BITWISE_XOR},
                                                                            {"BITWISE_NOT", TokenType::BITWISE_NOT},
                                                                            {"LOGICAL_RSHIFT", TokenType::LOGICAL_RSHIFT},
                                                                            {"MINUS", TokenType::MINUS},
                                                                            {"MUL", TokenType::MUL},
                                                                            {"DIV", TokenType::DIV},
                                                                            {"SIZEOF", TokenType::SIZEOF},
                                                                            {"THROW", TokenType::THROW},
                                                                            {"POWER", TokenType::POWER},
                                                                            {"LPAREN", TokenType::LPAREN},
                                                                            {"RPAREN", TokenType::RPAREN},
                                                                            {"SEMICOLON", TokenType::SEMICOLON},
                                                                            {"DEF", TokenType::DEF},
                                                                            {"INCREMENT", TokenType::INCREMENT},
                                                                            {"DECREMENT", TokenType::DECREMENT},
                                                                            {"IDENTIFIER", TokenType::IDENTIFIER},
                                                                            {"ADDR_T", TokenType::ADDR_T},
                                                                            {"BYTE", TokenType::BYTE},
                                                                            {"NIBBLE", TokenType::NIBBLE},
                                                                            {"LONG_DOUBLE", TokenType::LONG_DOUBLE},
                                                                            {"SHORT_INT", TokenType::SHORT_INT},
                                                                            {"LONG_INT", TokenType::LONG_INT},
                                                                            {"KEYWORD", TokenType::KEYWORD},
                                                                            {"EQ", TokenType::EQ},
                                                                            {"EOFT", TokenType::EOFT},
                                                                            {"ARROW", TokenType::ARROW}};

    auto it = stringToEnum.find(upperStr);
    if (it != stringToEnum.end()) return it->second;
    throw std::invalid_argument("Unknown TokenType string: " + str);
}
std::string get_token_name(TokenType tok) {
    switch (tok) {
    case TokenType::INT: return "int";
    case TokenType::STRING: return "string";
    case TokenType::ADDR_T: return "addr_t";
    case TokenType::BYTE: return "byte";
    case TokenType::NIBBLE: return "nibble";
    case TokenType::LONG_INT: return "long int";
    case TokenType::SHORT_INT: return "short int";
    case TokenType::LONG_DOUBLE: return "long double";
    case TokenType::FLOAT: return "float";
    case TokenType::DOUBLE: return "double";
    case TokenType::CHAR: return "char";
    case TokenType::BOOL: return "bool";
    case TokenType::QBOOL: return "qbool";
    case TokenType::IF: return "if";
    case TokenType::ELSE: return "else";
    case TokenType::SWITCH: return "switch";
    case TokenType::CASE: return "case";
    case TokenType::DEFAULT: return "default";
    case TokenType::BREAK: return "break";

    case TokenType::IDENTIFIER: return "identifier";
    case TokenType::KEYWORD: return "keyword";
    case TokenType::FSTRING: return "fstring";

    case TokenType::PLUS: return "+";
    case TokenType::MINUS: return "-";
    case TokenType::MUL: return "*";
    case TokenType::DIV: return "/";
    case TokenType::MOD: return "%";
    case TokenType::POWER: return "#^";

    case TokenType::PLUS_EQ: return "+=";
    case TokenType::MINUS_EQ: return "-=";
    case TokenType::MUL_EQ: return "*=";
    case TokenType::DIV_EQ: return "/=";
    case TokenType::MOD_EQ: return "%=";

    case TokenType::INCREMENT: return "++";
    case TokenType::DECREMENT: return "--";

    case TokenType::EQ: return "=";
    case TokenType::EQ_TO: return "==";
    case TokenType::NOT_EQ: return "!=";
    case TokenType::MORE: return ">";
    case TokenType::LESS: return "<";
    case TokenType::MORE_EQ: return ">=";
    case TokenType::LESS_EQ: return "<=";

    case TokenType::AND: return "&&&";
    case TokenType::OR: return "||";
    case TokenType::XOR: return "^";
    case TokenType::NOT: return "!";

    case TokenType::QAND: return "&&&";
    case TokenType::QOR: return "|||";
    case TokenType::QXOR: return "^^";
    case TokenType::QNOT: return "!!";
    case TokenType::QEQEQ: return "===";
    case TokenType::QNEQ: return "!==";
    case TokenType::COLLAPSE_AND: return "&|&";
    case TokenType::COLLAPSE_OR: return "|&|";

    case TokenType::LPAREN: return "(";
    case TokenType::RPAREN: return ")";
    case TokenType::LBRACE: return "{";
    case TokenType::RBRACE: return "}";
    case TokenType::LBRACKET: return "[";
    case TokenType::RBRACKET: return "]";
    case TokenType::COMMA: return ",";
    case TokenType::DOT: return ".";
    case TokenType::COLON: return ":";
    case TokenType::SEMICOLON: return ";";
    case TokenType::ARROW: return "->";
    case TokenType::AMPERSAND: return "&";
    case TokenType::STAR: return "*";
    case TokenType::SCOPE: return "::";
    case TokenType::LSHIFT: return "<<";
    case TokenType::RSHIFT: return "|>";
    case TokenType::R_ROT: return "|>>";
    case TokenType::L_ROT: return "<<<";
    case TokenType::BITWISE_XOR: return "$";
    case TokenType::BITWISE_NOT: return "~";
    case TokenType::LOGICAL_RSHIFT: return ":>";
    case TokenType::AT: return "@";
    case TokenType::PIPE: return "|";
    case TokenType::SIZEOF: return "sizeof";
    case TokenType::THROW: return "throw";
    case TokenType::EOFT: return "<eof>";
    case TokenType::VARADIC: return "...";
    case TokenType::RROT_EQ: return "|>>=";
    case TokenType::LROT_EQ: return "<<<=";
    case TokenType::LSH_EQ: return "<<=";
    case TokenType::RSH_EQ: return "|>=";
    case TokenType::LRSH_EQ: return ":>=";
    case TokenType::BIT_X_EQ: return "$=";
    case TokenType::BIT_A_EQ: return "&=";
    case TokenType::BIT_O_EQ: return "|=";
    default: return "<unknown token>";
    }

    return "<unknown token>";
}
Position Position::copy() {
    return Position(this->file_id, this->index, this->line, this->column);
}
Token::Token() {
}
Token::Token(TokenType t, std::string val, Position p) : type(t), value(val), pos(p) {
    this->pos.length = val.size();
}
std::string Token::print() const {
    if (this->value.empty()) {
        return get_token_name(this->type);
    } else {
        return get_token_name(this->type) + ":" + this->value;
    }
}
