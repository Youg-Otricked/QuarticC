#include "translate.h"
#include "compiler.h"
static std::string clang_resource_dir() {
    static std::string dir;
    static bool loaded = false;
    if (loaded) return dir;
    loaded = true;
    if (FILE* p = popen("clang -print-resource-dir 2>/dev/null", "r")) {
        char buf[512];
        if (fgets(buf, sizeof buf, p)) dir = trim(buf);
        pclose(p);
    }
    return dir;
}
namespace {
std::string str(CXString s) {
    const char* p = clang_getCString(s);
    std::string out = p ? p : "";
    clang_disposeString(s);
    return out;
}
void getChildren(CXCursor c, std::vector<CXCursor>* components) {
    clang_visitChildren(
        c,
        [](CXCursor child, CXCursor parent, CXClientData client_data) {
            auto* list = static_cast<std::vector<CXCursor>*>(client_data);
            list->push_back(child);
            return CXChildVisit_Continue;
        },
        components);
}
bool isUnsignedInteger(CXType t) {
    t = clang_getCanonicalType(t);
    switch (t.kind) {
    case CXType_Char_U:
    case CXType_UChar:
    case CXType_UShort:
    case CXType_UInt:
    case CXType_ULong:
    case CXType_ULongLong:
    case CXType_UInt128: return true;
    default: return false;
    }
}

} // namespace
bool isAnon(CXCursor decl) {
    if (clang_Cursor_isNull(decl)) return false;
    if (clang_Cursor_isAnonymous(decl)) return true;
    std::string s = str(clang_getCursorSpelling(decl));
    return s.empty() || s.find("(unnamed") != std::string::npos || s.find("(anonymous") != std::string::npos;
}
std::string anonName(CXCursor decl) {
    static std::map<std::string, std::string> names;
    static int counter = 0;
    std::string key = str(clang_getCursorUSR(decl));
    if (key.empty()) {
        CXFile f;
        unsigned line, col, off;
        clang_getSpellingLocation(clang_getCursorLocation(decl), &f, &line, &col, &off);
        key = str(clang_getFileName(f)) + ":" + std::to_string(line) + ":" + std::to_string(col);
    }
    auto it = names.find(key);
    if (it != names.end()) return it->second;
    return names[key] = "__anonymous_" + std::to_string(counter++);
}
CXType baseType(CXType t) {
    for (;;) {
        if (t.kind == CXType_Elaborated)
            t = clang_Type_getNamedType(t);
        else if (t.kind == CXType_Pointer)
            t = clang_getPointeeType(t);
        else if (t.kind == CXType_ConstantArray || t.kind == CXType_IncompleteArray)
            t = clang_getArrayElementType(t);
        else
            return t;
    }
}
void Translator::error(const std::string& msg) {
    ctx->errors.push_back(new CTranslateError(msg));
}

int Translator::translate_header() {
    CXIndex idx = clang_createIndex(0, 0);
    std::vector<const char*> argv = {"-x", "c"};
    std::string rd = clang_resource_dir();
    if (!rd.empty()) { ctx->args.push_back("-resource-dir=" + rd); }
    for (const std::string& a : ctx->args) argv.push_back(a.c_str());
    for (const std::string& a : ctx->args) argv.push_back(a.c_str());
    CXTranslationUnit tu = nullptr;
    CXErrorCode err = clang_parseTranslationUnit2(idx, ctx->header.c_str(), argv.data(), (int)argv.size(), nullptr, 0,
                                                  CXTranslationUnit_DetailedPreprocessingRecord, &tu);
    if (err != CXError_Success || !tu) {
        error("failed to parse '" + ctx->header + "' (code " + std::to_string((int)err) + ")");
        clang_disposeIndex(idx);
        return 1;
    }
    unsigned n = clang_getNumDiagnostics(tu);
    bool had_error = false;
    for (unsigned i = 0; i < n; i++) {
        CXDiagnostic d = clang_getDiagnostic(tu, i);
        std::string text = str(clang_formatDiagnostic(d, clang_defaultDiagnosticDisplayOptions()));
        if (clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error) {
            had_error = true;
            error(text);
        } else {
            fprintf(stderr, "%s\n", text.c_str());
        }
        clang_disposeDiagnostic(d);
    }
    if (!had_error) clang_visitChildren(clang_getTranslationUnitCursor(tu), visit_trampoline, this);
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(idx);
    return ctx->errors.empty() ? 0 : 1;
}
CXChildVisitResult Translator::visit_trampoline(CXCursor c, CXCursor, CXClientData data) {
    return static_cast<Translator*>(data)->visit(c);
}

CXChildVisitResult Translator::visit_expr(CXCursor c) {
    switch (clang_getCursorKind(c)) {
    case CXCursor_UnexposedExpr: {
        bool child_visited = false;
        clang_visitChildren(
            c,
            [](CXCursor child, CXCursor parent, CXClientData client_data) {
                auto* visited_flag = static_cast<bool*>(client_data);
                *visited_flag = true;
                return CXChildVisit_Break;
            },
            &child_visited);
        struct UnexposedTracker {
            Translator* self;
            bool visited;
        } tracker = {this, false};
        clang_visitChildren(
            c,
            [](CXCursor child, CXCursor parent, CXClientData client_data) {
                auto* data = static_cast<UnexposedTracker*>(client_data);
                data->self->visit_expr(child);
                data->visited = true;
                return CXChildVisit_Break;
            },
            &tracker);
        if (!tracker.visited) {
            CXString spellingStr = clang_getCursorSpelling(c);
            std::string spelling = clang_getCString(spellingStr);
            if (!spelling.empty()) { result_file += spelling; }
            clang_disposeString(spellingStr);
        }
        break;
    }
    case CXCursor_IntegerLiteral:
    case CXCursor_FloatingLiteral:
    case CXCursor_StringLiteral:
    case CXCursor_CharacterLiteral: onLiteral(c); break;
    case CXCursor_DeclRefExpr: onDeclRef(c); break;
    case CXCursor_MemberRefExpr: onMemberRef(c); break;
    case CXCursor_ArraySubscriptExpr: onSubscript(c); break;
    case CXCursor_UnaryExpr:
    case CXCursor_UnaryOperator: onUnop(c); break;
    case CXCursor_CompoundAssignOperator:
    case CXCursor_BinaryOperator: onBinop(c); break;
    case CXCursor_ConditionalOperator: onTernary(c); break;
    case CXCursor_ParenExpr: {
        std::vector<CXCursor> children;
        getChildren(c, &children);
        if (children.empty()) break;

        result_file += "(";
        visit_expr(children[0]);
        result_file += ")";
        break;
    }
    case CXCursor_CallExpr: onCall(c); break;
    case CXCursor_CStyleCastExpr: onCast(c); break;
    case CXCursor_InitListExpr: onInitList(c); break;
    case CXCursor_CompoundLiteralExpr: onCompoundLiteral(c); break;
    default: break;
    }
    return CXChildVisit_Continue;
}
void Translator::onCompoundLiteral(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    CXType type = clang_getCursorType(c);
    if (type.kind == CXType_Record) { result_file += translateType(type); }
    for (CXCursor child : children) {
        if (clang_getCursorKind(child) == CXCursor_InitListExpr) {
            onInitList(child);
            return;
        }
    }
    visit_expr(children[0]);
}
void Translator::onInitList(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    CXType raw_type = clang_getCursorType(c);
    CXType type = clang_getCanonicalType(raw_type);
    bool isArray = (type.kind == CXType_ConstantArray || type.kind == CXType_IncompleteArray);
    bool isRecord = (type.kind == CXType_Record);
    if (isRecord) { result_file += translateType(raw_type); }
    result_file += isArray ? "[" : "{";
    for (size_t i = 0; i < children.size(); ++i) {
        CXCursor child = children[i];
        CXCursorKind childKind = clang_getCursorKind(child);
        std::vector<CXCursor> sub_children;
        getChildren(child, &sub_children);
        if (childKind == CXCursor_UnexposedExpr && sub_children.size() >= 2) {
            CXString fieldNameStr = clang_getCursorSpelling(sub_children[0]);
            std::string fieldName = clang_getCString(fieldNameStr);
            clang_disposeString(fieldNameStr);
            if (!fieldName.empty() && fieldName[0] == '.') { fieldName = fieldName.substr(1); }
            result_file += fieldName + ": ";
            visit_expr(sub_children[1]);
        } else {
            if (childKind == CXCursor_UnexposedExpr && !sub_children.empty()) {
                visit_expr(sub_children[0]);
            } else {
                visit_expr(child);
            }
        }
        if (i + 1 < children.size()) { result_file += ", "; }
    }
    result_file += isArray ? "]" : "}";
}
void Translator::onCast(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    visit_expr(children[1]);
    result_file += " as " + translateType(clang_getCursorType(children[0]));
}
void Translator::onCall(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    visit_expr(children[0]);
    result_file += "(";
    for (int i = 1; i < children.size(); i++) {
        if (result_file.back() != '(') result_file += ", ";
        visit_expr(children[i]);
    }
    result_file += ")";
}
void Translator::onTernary(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    visit_expr(children[0]);
    result_file += " ? ";
    visit_expr(children[1]);
    result_file += " : ";
    visit_expr(children[2]);
}
void Translator::onUnop(CXCursor c) {
    if (clang_getCursorKind(c) == CXCursor_UnaryExpr) {
        CXString keywordStr = clang_getCursorSpelling(c);
        result_file += "(";
        result_file += clang_getCString(keywordStr);
        result_file += " ";
        clang_disposeString(keywordStr);
        std::vector<CXCursor> children;
        getChildren(c, &children);
        if (!children.empty()) {
            visit_expr(children[0]);
        } else {
            CXTranslationUnit tu = clang_Cursor_getTranslationUnit(c);
            CXSourceRange range = clang_getCursorExtent(c);
            CXToken* tokens = nullptr;
            unsigned numTokens = 0;
            clang_tokenize(tu, range, &tokens, &numTokens);
            bool insideParens = false;
            std::string cleanedType = "";
            for (unsigned i = 0; i < numTokens; ++i) {
                CXString tokenStr = clang_getTokenSpelling(tu, tokens[i]);
                std::string tokenText = clang_getCString(tokenStr);
                if (tokenText == ")") { insideParens = false; }
                if (insideParens) {
                    if (tokenText != "struct" && tokenText != "union" && tokenText != "enum") {
                        if (tokenText == "*") {
                            if (!cleanedType.empty() && cleanedType.back() == ' ') cleanedType.pop_back();
                            cleanedType += "*";
                        } else {
                            cleanedType += tokenText + " ";
                        }
                    }
                }
                if (tokenText == "(") { insideParens = true; }
                clang_disposeString(tokenStr);
            }
            while (!cleanedType.empty() && cleanedType.back() == ' ') cleanedType.pop_back();
            result_file += cleanedType;
            clang_disposeTokens(tu, tokens, numTokens);
        }
        result_file += ")";
        return;
    }
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    enum CXUnaryOperatorKind opKind = clang_getCursorUnaryOperatorKind(c);
    CXString opStr = clang_getUnaryOperatorKindSpelling(opKind);
    std::string op = clang_getCString(opStr);
    clang_disposeString(opStr);
    bool isPostfix = (opKind == CXUnaryOperator_PostInc || opKind == CXUnaryOperator_PostDec);
    if (isPostfix) {
        visit_expr(children[0]);
        result_file += op;
    } else {
        result_file += op;
        visit_expr(children[0]);
    }
}
void Translator::onBinop(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.size() < 2) return;
    visit_expr(children[0]);
    enum CXBinaryOperatorKind opKind = clang_getCursorBinaryOperatorKind(c);
    CXString opStr = clang_getBinaryOperatorKindSpelling(opKind);
    std::string txt(clang_getCString(opStr));
    if (txt == "^") txt = "$";
    if (txt == ">>") {
        if (isUnsignedInteger(clang_getCanonicalType(clang_getCursorType(children[0])))) {
            txt = ":>";
        } else {
            txt = "|>";
        }
    }

    result_file += " " + txt + " ";
    clang_disposeString(opStr);
    visit_expr(children[1]);
}
void Translator::onSubscript(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.size() < 2) return;
    visit_expr(children[0]);
    result_file += "[";
    visit_expr(children[1]);
    result_file += "]";
}
void Translator::onMemberRef(CXCursor c) {
    std::vector<CXCursor> children;
    getChildren(c, &children);
    if (children.empty()) return;
    visit_expr(children[0]);
    result_file += clang_getCursorType(children[0]).kind == CXType_Pointer ? "->" : ".";
    CXString fieldStr = clang_getCursorSpelling(c);
    result_file += clang_getCString(fieldStr);
    clang_disposeString(fieldStr);
}
void Translator::onDeclRef(CXCursor c) {
    CXCursor decl = clang_getCursorReferenced(c);
    if (clang_getCursorKind(decl) == CXCursor_EnumConstantDecl) {
        CXCursor enumParent = clang_getCursorSemanticParent(decl);
        CXString enumNameStr = clang_getCursorSpelling(enumParent);
        result_file += clang_getCString(enumNameStr);
        result_file += ".";
        clang_disposeString(enumNameStr);
    }
    CXString nameStr = clang_getCursorSpelling(c);
    result_file += clang_getCString(nameStr);
    clang_disposeString(nameStr);
}
void Translator::onLiteral(CXCursor c) {
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(c);
    CXSourceRange range = clang_getCursorExtent(c);
    CXToken* tokens = nullptr;
    unsigned numTokens = 0;
    clang_tokenize(tu, range, &tokens, &numTokens);
    if (numTokens > 0) {
        CXString tokenSpelling = clang_getTokenSpelling(tu, tokens[0]);
        result_file += clang_getCString(tokenSpelling);
        clang_disposeString(tokenSpelling);
    }
    clang_disposeTokens(tu, tokens, numTokens);
}
CXChildVisitResult Translator::visit(CXCursor c, bool indent_braces) {
    if (!fci)
        if (!clang_Location_isFromMainFile(clang_getCursorLocation(c))) return CXChildVisit_Continue;
    switch (clang_getCursorKind(c)) {
    case CXCursor_FunctionDecl: onFunction(c); break;
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
        if (isAnon(c)) break;
        onRecord(c);
        break;
    case CXCursor_EnumDecl:
        if (isAnon(c)) break;
        onEnum(c);
        break;
    case CXCursor_TypedefDecl: onTypedef(c); break;
    case CXCursor_VarDecl: onVar(c); break;
    case CXCursor_MacroDefinition: onMacro(c); break;
    case CXCursor_CompoundStmt: onCompound(c, indent_braces); break;
    case CXCursor_DeclStmt: onDecl(c); break;
    case CXCursor_NullStmt: {
        break;
    }
    case CXCursor_IfStmt: onIf(c); break;
    case CXCursor_SwitchStmt: onSwitch(c); break;
    case CXCursor_DefaultStmt:
    case CXCursor_CaseStmt:
    case CXCursor_LabelStmt: onLabel(c); break;
    case CXCursor_ContinueStmt:
    case CXCursor_BreakStmt:
    case CXCursor_ReturnStmt:
    case CXCursor_GotoStmt: onTerminator(c); break;
    case CXCursor_WhileStmt: onWhile(c); break;
    case CXCursor_DoStmt: onDo(c); break;
    case CXCursor_ForStmt: onFor(c); break;
    case CXCursor_UnexposedExpr:
    case CXCursor_IntegerLiteral:
    case CXCursor_FloatingLiteral:
    case CXCursor_StringLiteral:
    case CXCursor_CharacterLiteral:
    case CXCursor_DeclRefExpr:
    case CXCursor_MemberRefExpr:
    case CXCursor_ArraySubscriptExpr:
    case CXCursor_UnaryOperator:
    case CXCursor_BinaryOperator:
    case CXCursor_CompoundAssignOperator:
    case CXCursor_ConditionalOperator:
    case CXCursor_ParenExpr:
    case CXCursor_CallExpr:
    case CXCursor_CStyleCastExpr:
    case CXCursor_UnaryExpr:
    case CXCursor_InitListExpr:
    case CXCursor_CompoundLiteralExpr: {
        append_indentation();
        visit_expr(c);
        result_file += ";\n";
        break;
    }
    default: break;
    }
    return CXChildVisit_Continue;
}
void Translator::onFor(CXCursor c) {
    append_indentation();
    std::vector<CXCursor> components;
    getChildren(c, &components);
    if (components.empty()) return;
    CXCursor body = components.back();
    components.pop_back();
    result_file += "for (";
    inside_loop_header = true;
    if (!components.empty() && (clang_getCursorKind(components[0]) == CXCursor_DeclStmt || clang_getCursorKind(components[0]) == CXCursor_VarDecl)) {
        visit(components[0]);
        while (!result_file.empty() && result_file.back() == '\n') result_file.pop_back();
        if (result_file.back() != ';') result_file += ";";
    } else if (!components.empty()) {
        visit(components[0]);
        while (!result_file.empty() && result_file.back() == '\n') result_file.pop_back();
        result_file += ";";
    } else {
        result_file += ";";
    }
    result_file += " ";
    inside_loop_header = false;
    if (components.size() >= 2) {
        visit_expr(components[1]);
        while (!result_file.empty() && result_file.back() == '\n') result_file.pop_back();
        if (result_file.back() != ';') result_file += ";";
    } else {
        result_file += ";";
    }
    result_file += " ";
    if (components.size() == 3 || (components.size() == 2 && clang_getCursorKind(components[0]) != CXCursor_DeclStmt)) {
        visit_expr(components.back());
        while (!result_file.empty() && result_file.back() == '\n') result_file.pop_back();
        if (result_file.back() == ';') { result_file.pop_back(); }
    }
    result_file += ") ";
    visit(body, false);
}

void Translator::onDo(CXCursor c) {
    append_indentation();
    std::vector<CXCursor> components;
    getChildren(c, &components);
    if (components.size() < 2) return;
    result_file += "do {\n";
    indent_level++;
    clang_visitChildren(components[0], visit_trampoline, this);
    indent_level--;
    if (result_file.back() != '\n') result_file += '\n';
    append_indentation();
    result_file += "} while (";
    visit_expr(components[1]);
    result_file += ");\n";
}
void Translator::onWhile(CXCursor c) {
    std::vector<CXCursor> components;
    getChildren(c, &components);
    if (components.size() < 2) return;
    append_indentation();
    result_file += "while (";
    visit_expr(components[0]);
    result_file += ") ";
    visit(components[1], false);
}
void Translator::onTerminator(CXCursor c) {
    append_indentation();
    CXCursorKind kind = clang_getCursorKind(c);
    if (kind == CXCursor_BreakStmt) {
        result_file += "break;\n";
    } else if (kind == CXCursor_ContinueStmt) {
        result_file += "continue;\n";
    } else if (kind == CXCursor_ReturnStmt) {
        result_file += "return";
        std::vector<CXCursor> components;
        getChildren(c, &components);
        if (!components.empty()) {
            result_file += " ";
            visit_expr(components[0]);
        }
        result_file += ";\n";
        return;
    } else if (kind == CXCursor_GotoStmt) {
        result_file += "jmp ";
        CXCursor label = clang_getCursorReferenced(c);
        CXString nameStr = clang_getCursorSpelling(label);
        result_file += clang_getCString(nameStr);
        result_file += ";\n";
        clang_disposeString(nameStr);
    }
}
void Translator::onLabel(CXCursor c) {
    CXCursorKind kind = clang_getCursorKind(c);
    if (kind == CXCursor_CaseStmt) {
        append_indentation();
        std::vector<CXCursor> components = {};
        getChildren(c, &components);
        if (components.size() < 2) return;
        result_file += "case ";
        visit_expr(components[0]);
        result_file += ":\n";
        CXCursorKind bodyKind = clang_getCursorKind(components[1]);
        if (bodyKind == CXCursor_CaseStmt || bodyKind == CXCursor_DefaultStmt || bodyKind == CXCursor_CompoundStmt) {
            visit(components[1]);
        } else {
            indent_level++;
            struct Data {
                Translator* self;
                int num;
            };
            Data data = {this, 0};
            clang_visitChildren(
                c,
                [](CXCursor child, CXCursor parent, CXClientData client_data) {
                    auto* data = static_cast<Data*>(client_data);
                    data->num++;
                    if (data->num == 1) return CXChildVisit_Continue;
                    data->self->visit(child);
                    return CXChildVisit_Continue;
                },
                &data);
            indent_level--;
        }
    } else if (kind == CXCursor_DefaultStmt) {
        append_indentation();
        result_file += "default:\n";
        indent_level++;
        clang_visitChildren(c, visit_trampoline, this);
        indent_level--;
    } else if (kind == CXCursor_LabelStmt) {
        CXString nameStr = clang_getCursorSpelling(c);
        result_file += "\n";
        result_file += clang_getCString(nameStr);
        result_file += ":\n";
        clang_disposeString(nameStr);
        clang_visitChildren(c, visit_trampoline, this);
    }
}
void Translator::onSwitch(CXCursor c) {
    std::vector<CXCursor> components;
    getChildren(c, &components);
    if (components.size() < 2) return;
    append_indentation();
    result_file += "switch (";
    visit_expr(components[0]);
    result_file += ") {\n";
    indent_level++;
    clang_visitChildren(components[1], visit_trampoline, this);
    indent_level--;
    append_indentation();
    result_file += "}\n";
}
void Translator::onIf(CXCursor c) {
    std::vector<CXCursor> components;
    getChildren(c, &components);
    if (components.empty()) return;
    CXCursor condition = components[0];
    append_indentation();
    result_file += "if (";
    visit_expr(condition);
    result_file += ") ";
    if (components.size() > 1) { visit(components[1], false); }
    if (components.size() > 2) {
        if (result_file.ends_with("\n")) result_file.pop_back();
        result_file += " else ";
        if (clang_getCursorKind(components[2]) == CXCursor_IfStmt) {
            size_t marker = result_file.length();
            visit(components[2]);
            while (result_file.substr(marker, 4) == "    ") { result_file.erase(marker, 4); }
        } else {
            visit(components[2], false);
        }
    }
}
void Translator::onDecl(CXCursor c) {
    clang_visitChildren(c, visit_trampoline, this);
}
void Translator::onCompound(CXCursor c, bool indent = true) {
    if (indent) append_indentation();
    result_file += "{\n";
    indent_level++;
    clang_visitChildren(c, visit_trampoline, this);
    indent_level--;
    append_indentation();
    result_file += "}\n";
}
void Translator::onFunction(CXCursor c) {
    CXString nameStr = clang_getCursorSpelling(c);
    const char* name = clang_getCString(nameStr);
    std::string fname = name ? name : "";
    clang_disposeString(nameStr);
    if (!mark_decl_emitted("fn:" + fname)) return;
    CXType fn_type = clang_getCursorType(c);
    CXType ret_type = clang_getResultType(fn_type);
    speculative = true;
    poisoned = false;
    std::string sig = translateType(ret_type) + " " + fname + "(";
    int n = clang_Cursor_getNumArguments(c);
    for (int i = 0; i < n; ++i) {
        CXCursor arg = clang_Cursor_getArgument(c, i);
        CXString argNameStr = clang_getCursorSpelling(arg);
        const char* argName = clang_getCString(argNameStr);
        std::string argNameCopy = (argName && *argName) ? argName : "arg" + std::to_string(i);
        clang_disposeString(argNameStr);
        CXType argTy = clang_getArgType(fn_type, i);
        if (argTy.kind == CXType_Invalid) argTy = clang_getCursorType(arg);
        sig += translateType(argTy);
        sig += " " + argNameCopy;
        if (i + 1 < n) sig += ", ";
    }
    if (clang_isFunctionTypeVariadic(fn_type) && fname != "main") {
        if (n > 0) sig += ", ";
        sig += "...";
    }
    sig += ")";
    speculative = false;
    bool is_def = clang_isCursorDefinition(c);
    if (poisoned) {
        poisoned = false;
        if (is_def) { error("function '" + fname + "' uses an unsupported type"); }
        (*ctx->skipped).insert(fname);
        return;
    }
    append_indentation();
    result_file += sig;
    if (is_def) {
        result_file += " ";
        clang_visitChildren(c, visit_trampoline, this);
    } else {
        result_file += ";\n";
        append_indentation();
    }
}

void Translator::onRecord(CXCursor c, const std::string& name_override) {
    CXCursor def = clang_getCursorDefinition(c);
    if (!clang_Cursor_isNull(def)) c = def;
    CXCursorKind kind = clang_getCursorKind(c);
    bool is_union = kind == CXCursor_UnionDecl;
    std::string name = name_override;
    if (name.empty()) name = isAnon(c) ? anonName(c) : str(clang_getCursorSpelling(c));
    if (!mark_decl_emitted("struct:" + name)) return;
    if (!mark_type_emitted(name)) return;
    CXType type = clang_getCursorType(c);
    long long size = clang_Type_getSizeOf(type);
    long long align = clang_Type_getAlignOf(type);
    if (clang_Cursor_isNull(def) && !clang_isCursorDefinition(c)) {
        result_file += "struct " + name + ";\n";
        append_indentation();
        return;
    }
    std::vector<CXCursor> kids;
    getChildren(c, &kids);
    for (CXCursor k : kids) {
        if (clang_getCursorKind(k) != CXCursor_FieldDecl) continue;
        CXType bt = baseType(clang_getCursorType(k));
        CXCursor d = clang_getTypeDeclaration(bt);
        if (!isAnon(d)) continue;
        if (bt.kind == CXType_Record)
            onRecord(d, anonName(d));
        else if (bt.kind == CXType_Enum)
            onEnum(d, anonName(d));
    }
    struct Context {
        Translator* self;
        std::vector<std::string>* lines;
        bool is_union;
        int anon_fields = 0;
        int pad_count = 0;
        long long cursor = 0;
        bool skipped_bits = false;
    };
    std::vector<std::string> lines;
    Context ctx{this, &lines, is_union};
    bool prevSpec = speculative, prevPoison = poisoned;
    speculative = true;
    poisoned = false;
    clang_visitChildren(
        c,
        [](CXCursor child, CXCursor, CXClientData data) {
            if (clang_getCursorKind(child) != CXCursor_FieldDecl) return CXChildVisit_Continue;
            auto* ctx = static_cast<Context*>(data);
            if (clang_Cursor_isBitField(child)) {
                ctx->skipped_bits = true;
                return CXChildVisit_Continue;
            }
            CXType field_type = clang_getCursorType(child);
            long long off = clang_Cursor_getOffsetOfField(child);
            if (!ctx->is_union && ctx->skipped_bits && off > ctx->cursor) {
                ctx->lines->push_back("char[" + std::to_string((off - ctx->cursor) / 8) + "] __pad_" + std::to_string(ctx->pad_count++) + ";");
            }
            ctx->skipped_bits = false;
            CXString field_spelling = clang_getCursorSpelling(child);
            const char* field_name = clang_getCString(field_spelling);
            std::string fname = field_name ? field_name : "";
            clang_disposeString(field_spelling);
            if (fname.empty()) fname = "__anonymous_field_" + std::to_string(ctx->anon_fields++);
            ctx->lines->push_back(ctx->self->translateType(field_type) + " " + fname + ";");
            long long fsz = clang_Type_getSizeOf(field_type);
            if (!ctx->is_union && off >= 0 && fsz >= 0) ctx->cursor = off + fsz * 8;
            return CXChildVisit_Continue;
        },
        &ctx);
    if (!is_union && ctx.skipped_bits && size * 8 > ctx.cursor) {
        lines.push_back("char[" + std::to_string((size * 8 - ctx.cursor) / 8) + "] __pad_" + std::to_string(ctx.pad_count++) + ";");
    }
    bool bad = poisoned;
    speculative = prevSpec;
    poisoned = prevPoison;
    if ((bad || lines.empty()) && size > 0) {
        lines.clear();
        if (align == 8 && size % 8 == 0)
            lines.push_back("long int[" + std::to_string(size / 8) + "] __opaque;");
        else
            lines.push_back("char[" + std::to_string(size) + "] __opaque;");
    }
    result_file += is_union ? "union " : "struct ";
    result_file += name + " {\n";
    indent_level++;
    for (const std::string& l : lines) {
        append_indentation();
        result_file += l + "\n";
    }
    indent_level--;
    append_indentation();
    result_file += "};\n";
}

void Translator::onEnum(CXCursor c, const std::string& name_override) {
    CXString spelling = clang_getCursorSpelling(c);
    std::string name = name_override;
    if (name.empty()) name = isAnon(c) ? anonName(c) : str(clang_getCursorSpelling(c));
    clang_disposeString(spelling);
    CXType underlying = clang_getEnumDeclIntegerType(c);
    if (!mark_decl_emitted("enum:" + name)) return;
    if (!mark_type_emitted(name)) return;
    result_file += "enum " + name + " {\n";
    clang_visitChildren(
        c,
        [](CXCursor child, CXCursor, CXClientData data) {
            if (clang_getCursorKind(child) != CXCursor_EnumConstantDecl) return CXChildVisit_Continue;
            auto* out = static_cast<std::string*>(data);
            CXString s = clang_getCursorSpelling(child);
            const char* raw_name = clang_getCString(s);
            *out += "    ";
            *out += raw_name ? raw_name : "";
            long long value = clang_getEnumConstantDeclValue(child);
            *out += " = " + std::to_string(value) + ";\n";
            clang_disposeString(s);
            return CXChildVisit_Continue;
        },
        &result_file);
    result_file += "}\n";
}
void Translator::onTypedef(CXCursor c) {
    CXString name_str = clang_getCursorSpelling(c);
    const char* cn = clang_getCString(name_str);
    std::string name = cn ? cn : "";
    clang_disposeString(name_str);
    const std::string key = "td:" + name;
    if ((*ctx->emitted).count(key)) return;
    CXType underlying = clang_getTypedefDeclUnderlyingType(c);
    CXCursor underlyingDecl = clang_getTypeDeclaration(underlying);
    CXString un = clang_getCursorSpelling(underlyingDecl);
    const char* ucn = clang_getCString(un);
    std::string underlyingName = ucn ? ucn : "";
    clang_disposeString(un);
    if (name == underlyingName) return;
    switch (underlying.kind) {
    case CXType_Enum:
        (*ctx->emitted).insert(key);
        onEnum(underlyingDecl, name);
        return;
    case CXType_Record:
        (*ctx->emitted).insert(key);
        if (!underlyingName.empty()) {
            onRecord(underlyingDecl);
            result_file += "type " + name + " = " + underlyingName + ";\n";
        } else {
            onRecord(underlyingDecl, name);
        }
        return;
    default: {
        bool prevSpec = speculative, prevPoison = poisoned;
        speculative = true;
        poisoned = false;
        std::string ty = translateType(underlying);
        bool bad = poisoned;
        speculative = prevSpec;
        poisoned = prevPoison;
        if (bad) {
            (*ctx->skipped).insert(name);
            return;
        }
        if (!(*ctx->emitted).insert(key).second) return;
        result_file += "type " + name + " = " + ty + ";\n";
        return;
    }
    }
}
void Translator::onVar(CXCursor c) {
    append_indentation();
    CXType type = clang_getCursorType(c);
    CXString name = clang_getCursorSpelling(c);
    if (!mark_decl_emitted(std::string("var:") + clang_getCString(name))) {
        clang_disposeString(name);
        return;
    }
    bool is_extern = clang_Cursor_getStorageClass(c) == CX_SC_Extern && !clang_isCursorDefinition(c);
    if (is_extern) result_file += "foreign:\n";
    result_file += translateType(type) + " " + clang_getCString(name);
    CXCursor init = clang_Cursor_getVarDeclInitializer(c);
    if (!clang_Cursor_isNull(init)) {
        result_file += " = ";
        visit_expr(init);
    }
    result_file += ";\n";
    if (is_extern) result_file += ":foreign\n";
    clang_disposeString(name);
}

void Translator::onMacro(CXCursor c) {
    std::string name = str(clang_getCursorSpelling(c));
    if (macro_values.count(name)) return;
    if (clang_Cursor_isMacroFunctionLike(c)) return;
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(c);
    CXToken* tokens = nullptr;
    unsigned n = 0;
    clang_tokenize(tu, clang_getCursorExtent(c), &tokens, &n);
    std::vector<std::string> t;
    std::vector<CXTokenKind> k;
    for (unsigned i = 1; i < n; ++i) {
        t.push_back(str(clang_getTokenSpelling(tu, tokens[i])));
        k.push_back(clang_getTokenKind(tokens[i]));
    }
    clang_disposeTokens(tu, tokens, n);
    if (t.empty()) return;
    auto outerParens = [&]() {
        if (t.size() < 2 || t.front() != "(" || t.back() != ")") return false;
        int d = 0;
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == "(")
                d++;
            else if (t[i] == ")" && --d == 0 && i + 1 < t.size())
                return false;
        }
        return true;
    };
    for (bool changed = true; changed;) {
        changed = false;
        if (outerParens()) {
            t.erase(t.begin());
            k.erase(k.begin());
            t.pop_back();
            k.pop_back();
            changed = true;
        } else if (t.size() >= 4 && t[0] == "(" && t[2] == ")" && k[1] == CXToken_Identifier) {
            t.erase(t.begin(), t.begin() + 3);
            k.erase(k.begin(), k.begin() + 3);
            changed = true;
        }
    }
    std::string value;
    if (t.size() == 1 && k[0] == CXToken_Literal)
        value = t[0];
    else if (t.size() == 2 && t[0] == "-" && k[1] == CXToken_Literal)
        value = "-" + t[1];
    else if (t.size() == 1 && k[0] == CXToken_Identifier) {
        if (t[0] == "NULL" || t[0] == "nullptr")
            value = "nullptr";
        else if (t[0] == "true" || t[0] == "false")
            value = t[0];
        else if (auto it = macro_values.find(t[0]); it != macro_values.end())
            value = it->second;
    }
    if (value.empty()) {
        size_t i = 0;
        std::string prefix;
        if (!t.empty() && (t[0] == "*" || t[0] == "&")) {
            prefix = t[0];
            i = 1;
        }
        if (i < t.size() && k[i] == CXToken_Identifier && t[i] != name) {
            const std::string& id = t[i];
            bool call = (t.size() == i + 3 && t[i + 1] == "(" && t[i + 2] == ")");
            bool bare = (t.size() == i + 1);
            bool ok = (call && prefix != "&") || (bare && !prefix.empty());
            if (ok && !macro_values.count(id)) value = "(" + prefix + id + (call ? "()" : "") + ")";
        }
    }
    if (value.empty()) return;
    macro_values[name] = value;
    result_file += "#define " + name + " " + value + "\n";
}
std::string Translator::translateType(CXType t) {
    switch (t.kind) {
    case CXType_Void: return "void";
    case CXType_Bool: return "bool";
    case CXType_Char_U: return "byte";
    case CXType_UChar: return "byte";
    case CXType_Char_S: return "char";
    case CXType_SChar: return "char";
    case CXType_UShort: return "i16u_t";
    case CXType_Short: return "short int";
    case CXType_UInt: return "i32u_t";
    case CXType_Int: return "int";
    case CXType_ULong: return "addr_t";
    case CXType_Long: return "long int";
    case CXType_ULongLong: return "ai64u_t";
    case CXType_LongLong: return "ai64_t";
    case CXType_Float: return "float";
    case CXType_Double: return "double";
    case CXType_LongDouble: return "long double";
    case CXType_Pointer: {
        CXType pointee = clang_getPointeeType(t);
        CXType canon = clang_getCanonicalType(pointee);
        if (canon.kind == CXType_FunctionProto || canon.kind == CXType_FunctionNoProto) return translateType(canon);
        std::string base = translateType(pointee);
        if (clang_isConstQualifiedType(pointee)) return "const " + base + "*";
        return base + "*";
    }
    case CXType_ConstantArray: {
        CXType elem = clang_getArrayElementType(t);
        long long size = clang_getArraySize(t);
        return translateType(elem) + "[" + std::to_string(size) + "]";
    }
    case CXType_IncompleteArray: {
        CXType elem = clang_getArrayElementType(t);
        return translateType(elem) + "[]";
    }
    case CXType_FunctionProto: {
        int n = clang_getNumArgTypes(t);
        std::string out = "fn(";
        for (int i = 0; i < n; ++i) {
            if (i) out += ", ";
            out += translateType(clang_getArgType(t, i));
        }
        if (clang_isFunctionTypeVariadic(t)) {
            if (n) out += ", ";
            out += "...";
        }
        return out + ") -> " + translateType(clang_getResultType(t));
    }
    case CXType_FunctionNoProto: return "fn() -> " + translateType(clang_getResultType(t));
    case CXType_Record: {
        CXCursor decl = clang_getTypeDeclaration(t);
        onRecord(decl);
        if (isAnon(decl)) return anonName(decl);
        return str(clang_getCursorSpelling(decl));
    }
    case CXType_Enum: {
        CXCursor decl = clang_getTypeDeclaration(t);
        if (isAnon(decl)) return anonName(decl);
        return str(clang_getCursorSpelling(decl));
    }
    case CXType_Typedef: {
        CXCursor decl = clang_getTypeDeclaration(t);
        CXString nameStr = clang_getCursorSpelling(decl);
        const char* cname = clang_getCString(nameStr);
        std::string result = cname ? cname : "";
        clang_disposeString(nameStr);
        onTypedef(decl);
        if ((*ctx->skipped).count(result)) {
            if (speculative) {
                poisoned = true;
                return "unknown";
            }
            error("type '" + result + "' uses an unsupported type");
        }
        return result;
    }
    case CXType_Elaborated: {
        CXType named = clang_Type_getNamedType(t);
        if (named.kind == CXType_Invalid) break;
        return translateType(named);
    }
    case CXType_Atomic: {
        CXType value = clang_Type_getValueType(t);
        return "atomic " + translateType(value);
    }
    default: {
        CXType canon = clang_getCanonicalType(t);
        if (canon.kind != t.kind) return translateType(canon);
        if (speculative) {
            poisoned = true;
            return "unknown";
        }
        error("unsupported type: " + str(clang_getTypeKindSpelling(t.kind)) + " (" + str(clang_getTypeSpelling(t)) + ")");
    }
    }
    return "unknown";
}
