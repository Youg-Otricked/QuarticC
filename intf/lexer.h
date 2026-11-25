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
inline bool is_keyword(const std::string& id) {
    return (
        /* primitives */ id == "int" || id == "float" || id == "double" || id == "bool" || id == "qbool" || id == "char" || id == "ai64u_t" ||
        id == "ai64_t" || id == "i32u_t" || id == "i16u_t" ||
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
        /* namespace */ id == "namespace" || // i̶m̶ ̶t̶o̶o̶ ̶l̶a̶z̶y̶ ̶t̶o̶ ̶d̶o̶ ̶t̶h̶e̶ ̶r̶e̶s̶t̶ ̶o̶f̶ ̶t̶h̶i̶s̶ ̶r̶e̶o̶r̶g̶i̶n̶i̶z̶a̶t̶i̶o̶n̶ nevermind i did it. why did i do this. Also yes
                                             // i am using neovim and did just manually type
        // <C-v>u0336 1... 2... 51 times. And yes you did just count them. And yes, there are probably 50 or 52. It should be a unordered set. Im
        // not making it a unordered_set. 64 or's are faster. Yes i did count them. yes i am probably off.
        /* operator */ id == "roperator" || id == "operator" ||
        /* try/catch */ id == "try" || id == "catch" ||
        /* nullptr */ id == "nullptr" ||
        /* storage modifiers */ id == "out" || id == "inout" || id == "volatile" || id == "restrict" ||
        /* more types */ id == "byte" || id == "nibble" || id == "addr_t" ||
        /* class stuff */ id == "friend" || id == "friendly" || id == "static" || id == "abstract" || id == "final" ||
        /* more  class stuff */ id == "public" || id == "protected" || id == "private" || id == "virtual" || id == "override" ||
        /* defer */ id == "defer" || id == "errdefer" ||
        /* concepts */ id == "concept" || id == "proves" || id == "with_proof" || id == "_of" || id == "at_least" || id == "all_of" ||
        id == "proved_by" ||
        /* modifiers */ id == "modifier" || id == "on_call" || id == "on_return" || id == "on_use" ||
        /* comptime */ id == "comptime" ||
        /* raw unions */ id == "union" ||
        /* goto */ id == "goto" || id == "jmp" ||
        /* patterns */ id == "pattern" || id == "fits" ||
        /* panic */ id == "panic" ||
        /* stmt values */ id == "returning");
}
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
    Token make_raw_string();
};
#endif
