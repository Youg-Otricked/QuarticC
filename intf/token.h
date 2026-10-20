#ifndef TOKEN_H
#define TOKEN_H
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
struct SourceFile {
    std::string filename;
    std::string content;
};
class Position {
  public:
    static constexpr uint32_t INVALID_FILE_ID = ~uint32_t(0);
    uint32_t file_id;
    size_t index;
    size_t line;
    size_t column;
    size_t length = 1;
    Position();
    bool is_valid() const;
    std::string string(size_t context = 2) const;
    std::string arrow_string(size_t context = 2) const;
    Position(uint32_t, size_t, size_t, size_t);
    void advance(char current_char);
    Position copy();
    bool operator==(const Position&) const = default;
};
class SourceManager {
  private:
    const SourceFile INVALID = SourceFile{
        "<scratch>",
        "scratchscratchscratchscratchscratchscratchscratchscratchscratchscratch...",
    };
    std::vector<SourceFile> files;
    std::unordered_map<uint32_t, uint64_t> line_counts;
    std::unordered_map<std::string, uint32_t> path_to_id;
    std::unordered_map<uint32_t, std::string> id_rev_map;

  public:
    std::string get_file_name(uint32_t id) const;
    static SourceManager& instance();
    uint64_t get_file_line_count(uint32_t id) const;
    std::optional<uint32_t> get_id(const std::string& filepath) const;
    uint32_t add_file(std::string filepath, std::string text);
    bool has_id(uint32_t id) const;
    const SourceFile& get(uint32_t id) const;
};
inline std::unordered_map<std::string, uint64_t> file_og_line_counts;
static inline std::optional<uint64_t> get_original_line_no(std::string file_name, uint32_t file_id, uint64_t line) {
    uint64_t res = line - (SourceManager::instance().get_file_line_count(file_id) - file_og_line_counts[file_name]);
    return res > line ? std::nullopt : std::make_optional(res);
}
static inline uint64_t getLineNoFromPos(const Position& pos) {
    auto opt = get_original_line_no(SourceManager::instance().get_file_name(pos.file_id), pos.file_id, pos.line);
    return opt.has_value() ? opt.value() + 1 : 0;
}
enum class TokenType {
    INT,
    LONG_DOUBLE,
    LONG_INT,
    SHORT_INT,
    STRING,
    FLOAT,
    BYTE,
    NIBBLE,
    ADDR_T,
    DOUBLE,
    CHAR,
    ARROW,
    AMPERSAND,
    STAR,
    BOOL,
    QBOOL,
    PLUS,
    MINUS,
    MUL,
    DIV,
    POWER,
    LPAREN,
    RPAREN,
    LSHIFT,
    RSHIFT,
    SCOPE,
    LSH_EQ,
    RSH_EQ,
    SEMICOLON,
    DEF,
    INCREMENT,
    DECREMENT,
    IDENTIFIER,
    KEYWORD,
    PLUS_EQ,
    MINUS_EQ,
    SIZEOF,
    THROW,
    MUL_EQ,
    DIV_EQ,
    MOD,
    MOD_EQ,
    EQ_TO,
    NOT_EQ,
    MORE,
    LESS,
    MORE_EQ,
    LESS_EQ,
    AND,
    OR,
    XOR,
    BITWISE_NOT,
    BITWISE_XOR,
    BIT_X_EQ,
    R_ROT,
    L_ROT,
    RROT_EQ,
    LROT_EQ,
    LOGICAL_RSHIFT,
    LRSH_EQ,
    BIT_A_EQ,
    BIT_O_EQ,
    VARADIC,
    NOT,
    EQ,
    FSTRING,
    SWITCH,
    CASE,
    DEFAULT,
    IF,
    ELSE,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COLON,
    BREAK,
    FUNC,
    COMMA,
    DOT,
    AT,
    QAND,
    QOR,
    COLLAPSE_AND,
    COLLAPSE_OR,
    QEQEQ,
    QNEQ,
    QNOT,
    QXOR,
    PIPE,
    QUESTION,
    AS,
    TYPEOF,
    BIG_ARROW,
    EOFT
};
std::string get_token_name(TokenType tok);
TokenType stringToTokenType(const std::string& str);
class Token {
  public:
    TokenType type;
    std::string value;
    Position pos;
    Token();
    Token(TokenType t, std::string val, Position p);
    bool operator==(const Token&) const = default;
    std::string print() const;
};
#endif
