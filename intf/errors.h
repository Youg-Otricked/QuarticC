#pragma once
#include "token.h"
#include <string>
#include <optional>
class Error {
  public:
    Position pos;
    std::string error_name;
    std::string details;
    Error(std::string err, std::string details, Position pos);
    virtual std::string as_string();
    virtual ~Error() = default;
};

class IllegalCharError : public Error {
  public:
    IllegalCharError(std::string details, Position pos) : Error("illegal character", details, pos) {}
};

class InvalidSyntaxError : public Error {
  public:
    InvalidSyntaxError(std::string details, Position pos) : Error("invalid syntax: ", details, pos) {}
};

class MissingSemicolonError : public Error {
  public:
    MissingSemicolonError(Position pos) : Error("QC-MS99", "expected semicolon", pos) {}
};
class Note {
  public:
    Position pos;
    std::string message;
    bool context = false;
    Note(const Position& pos, const std::string& message, bool context = false) : pos(pos), message(message), context(context) {}
    std::string as_string() const;
};
class Help {
  public:
    Position pos;
    std::string message;
    std::optional<std::string> replacement;
    Help(const Position& pos, const std::string& message, std::optional<std::string> replacement = std::nullopt)
        : pos(pos), message(message), replacement(std::move(replacement)) {}
    std::string as_string() const;
};
class Insight {
  public:
    std::string message;
    Insight(const std::string& message) : message(message) {}
    std::string as_string() const;
};

class CTError : public Error {
  public:
    CTError(std::string d, Position pos, bool is_warning = false, std::string code = "", std::vector<Note> notes = {}, std::vector<Help> helps = {},
            std::vector<Insight> insights = {})
        : Error(code, d, pos) {
        this->is_warning = is_warning;
        this->notes = notes;
        this->helps = helps;
        this->insights = insights;
    }
    bool is_warning = false;
    std::vector<Note> notes;
    std::vector<Help> helps;
    std::vector<Insight> insights;
    std::string as_string() override;
};
struct Ler {
    std::vector<Token> Tkns;
    Error* error;
};
