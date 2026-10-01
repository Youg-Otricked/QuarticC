#include "errors.h"
#include <iostream>
std::string Note::as_string() const {
    std::string result;
    result += "note: ";
    result += message;
    result += "\n";
    result += "   --> ";
    result += SourceManager::instance().get(pos.file_id).filename;
    result += ":";
    result += std::to_string(pos.line + 1);
    result += ":";
    result += std::to_string(pos.column + 1);
    result += "\n";
    result += pos.arrow_string(0);
    return result;
}
std::string Help::as_string() const {
    std::string result;
    result += "help: ";
    result += message;
    result += "\n";
    result += "   --> ";
    result += SourceManager::instance().get(pos.file_id).filename;
    result += ":";
    result += std::to_string(pos.line + 1);
    result += ":";
    result += std::to_string(pos.column + 1);
    result += "\n";
    result += pos.string(0);
    return result;
}
std::string Insight::as_string() const {
    std::string result;
    result += "= insight: ";
    result += message;
    result += "\n";
    return result;
}
std::string CTError::as_string() {
    std::string result;
    result += (is_warning ? "warning " : "error ");
    result += this->error_name;
    if (!this->details.empty()) { result += ": " + this->details; }
    result += "\n";
    result += " --> ";
    result += SourceManager::instance().get(this->pos.file_id).filename;
    result += ":";
    result += std::to_string(this->pos.line + 1);
    result += ":";
    result += std::to_string(this->pos.column + 1);
    result += "\n";
    result += this->pos.arrow_string();
    for (const auto& note : notes) {
        result += "\n\033[0m\033[36m  ";
        result += note.as_string();
        result += "\033[0m";
    }
    for (const auto& help : helps) {
        result += "\n\033[0m\033[34m  ";
        result += help.as_string();
        result += "\033[0m";
    }
    for (const auto& insight : insights) {
        result += "\n\033[0m\033[32m  ";
        result += insight.as_string();
        result += "\033[0m";
    }
    return result;
}
Error::Error(std::string err, std::string details, Position pos) {
    this->details = details;
    this->error_name = err;
    this->pos = pos;
}
std::string Error::as_string() {
    std::string result;
    result += "error";
    if (!this->error_name.empty()) { result += " " + this->error_name; }
    if (!this->details.empty()) { result += ": " + this->details; }
    result += "\n";
    result += " --> ";
    result += SourceManager::instance().get(this->pos.file_id).filename;
    result += ":";
    result += std::to_string(this->pos.line + 1);
    result += ":";
    result += std::to_string(this->pos.column + 1);
    result += "\n";
    result += this->pos.arrow_string();
    return result;
}

