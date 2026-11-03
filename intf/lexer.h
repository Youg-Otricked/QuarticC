#ifndef LEXER_H
#define LEXER_H
#include <string>
#include "errors.h"
#include "token.h"
inline std::string bad_chars = " \t\n\r";
inline std::string DIGITS = "0123456789";
inline std::string LETTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
inline std::string LETTERSDIGITS = LETTERS + DIGITS;
bool isCharInSet(char c, const std::string& charSet);
class Lexer {
  public:
    std::string Filename;
    std::string text;
    Position pos;
    char current_char;
    bool fmt = false;
    Lexer(std::string text, std::string filename, bool fmt = false);
    void advance();
    Token make_char();
    Token make_string();
    Ler make_tokens();
    Token make_identifier();
    Token make_number();
    Token make_fstring();
    Token make_raw_string();
};
#endif
