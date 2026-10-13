#ifndef TRANSLATE_H
#define TRANSLATE_H
#include "errors.h"
#include <clang-c/Index.h>
#include <string>
#include <vector>
#include <unordered_set>
struct TranslateCtx {
    std::string header;
    std::vector<std::string> args;
    std::vector<Error*> errors;
};
class Translator {
  public:
    explicit Translator(TranslateCtx* ctx) : ctx(ctx) {}
    int translate_header();
    std::string result_file;

  private:
    TranslateCtx* ctx;
    static CXChildVisitResult visit_trampoline(CXCursor c, CXCursor parent, CXClientData data);
    CXChildVisitResult visit(CXCursor c);
    void onFunction(CXCursor c);
    void onRecord(CXCursor c, const std::string& name_override = "");
    void onEnum(CXCursor c, const std::string& name_override = "");
    void onTypedef(CXCursor c);
    void onVar(CXCursor c);
    void onMacro(CXCursor c);
    std::string translateType(CXType t);
    void error(const std::string& msg);
    std::unordered_set<std::string> emitted_types;
    bool mark_type_emitted(const std::string& name) { return emitted_types.insert(name).second; }
};

#endif
