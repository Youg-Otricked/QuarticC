#ifndef TRANSLATE_H
#define TRANSLATE_H
#include "errors.h"
#include <clang-c/Index.h>
#include <string>
#include <unordered_set>
#include <vector>
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
    int indent_level = 0;
    bool fci = false;
    bool inside_loop_header = false;
  private:
    TranslateCtx* ctx;
    static CXChildVisitResult visit_trampoline(CXCursor c, CXCursor parent, CXClientData data);
    CXChildVisitResult visit(CXCursor c, bool indent_braces = true);
    CXChildVisitResult visit_expr(CXCursor c);
    void append_indentation() {
        for (int i = 0; !inside_loop_header && i < indent_level; ++i) {
            result_file += "    ";
        }
    }
    void onCall(CXCursor c);
    void onCast(CXCursor c);
    void onCompoundLiteral(CXCursor c);
    void onInitList(CXCursor c);
    void onTernary(CXCursor c);
    void onUnop(CXCursor c);
    void onBinop(CXCursor c);
    void onSubscript(CXCursor c);
    void onLiteral(CXCursor c);
    void onDeclRef(CXCursor c);
    void onMemberRef(CXCursor c);
    void onDecl(CXCursor c);
    void onCompound(CXCursor c, bool indent);
    void onFunction(CXCursor c);
    void onRecord(CXCursor c, const std::string& name_override = "");
    void onEnum(CXCursor c, const std::string& name_override = "");
    void onTypedef(CXCursor c);
    void onVar(CXCursor c);
    void onMacro(CXCursor c);
    void onIf(CXCursor c);
    void onSwitch(CXCursor c);
    void onLabel(CXCursor c);
    void onTerminator(CXCursor c);
    void onWhile(CXCursor c);
    void onDo(CXCursor c);
    void onFor(CXCursor c);
    std::string translateType(CXType t);
    void error(const std::string& msg);
    std::unordered_set<std::string> emitted_types;
    bool mark_type_emitted(const std::string& name) { return emitted_types.insert(name).second; }
};

#endif
