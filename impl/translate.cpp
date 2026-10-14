#include "translate.h"
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
} // namespace
void Translator::error(const std::string& msg) {
    ctx->errors.push_back(new CTranslateError(msg));
}

int Translator::translate_header() {
    CXIndex idx = clang_createIndex(0, 0);
    std::vector<const char*> argv = {"-x", "c"};
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
    result_file += " as ";
    translateType(clang_getCursorType(children[0]));
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
    result_file += " " + std::string(clang_getCString(opStr)) + " ";
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
    if (!fci) if (!clang_Location_isFromMainFile(clang_getCursorLocation(c))) return CXChildVisit_Continue;
    switch (clang_getCursorKind(c)) {
    case CXCursor_FunctionDecl: onFunction(c); break;
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl: onRecord(c); break;
    case CXCursor_EnumDecl: onEnum(c); break;
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
    append_indentation();
    CXString nameStr = clang_getCursorSpelling(c);
    const char* name = clang_getCString(nameStr);
    CXType fn_type = clang_getCursorType(c);
    CXType ret_type = clang_getResultType(fn_type);
    result_file += translateType(ret_type) + " " + (name ? name : "") + "(";
    int n = clang_Cursor_getNumArguments(c);
    for (int i = 0; i < n; ++i) {
        CXCursor arg = clang_Cursor_getArgument(c, i);
        CXString argNameStr = clang_getCursorSpelling(arg);
        const char* argName = clang_getCString(argNameStr);
        result_file += translateType(clang_getCursorType(arg));
        result_file += " ";
        if (argName && *argName)
            result_file += argName;
        else
            result_file += "arg" + std::to_string(i);
        if (i + 1 < n) result_file += ", ";
        clang_disposeString(argNameStr);
    }
    if (clang_isFunctionTypeVariadic(fn_type) && std::string(name ? name : "") != "main") {
        if (n > 0) result_file += ", ";
        result_file += "...";
    }
    result_file += ")";
    if (clang_isCursorDefinition(c)) {
        result_file += " ";
        clang_visitChildren(c, visit_trampoline, this);
    } else {
        result_file += ";\n";
        append_indentation();
    }
    clang_disposeString(nameStr);
}

void Translator::onRecord(CXCursor c, const std::string& name_override) {
    CXCursor def = clang_getCursorDefinition(c);
    if (!clang_Cursor_isNull(def)) c = def;
    CXCursorKind kind = clang_getCursorKind(c);
    bool is_union = kind == CXCursor_UnionDecl;
    CXString spelling = clang_getCursorSpelling(c);
    std::string name = name_override;
    if (name.empty()) {
        const char* s = clang_getCString(spelling);
        if (s) name = s;
    }
    clang_disposeString(spelling);
    if (name.empty()) name = "__anonymous_record";
    if (!mark_type_emitted(name)) return;
    CXType type = clang_getCursorType(c);
    long long size = clang_Type_getSizeOf(type);
    if (clang_Cursor_isNull(def) && !clang_isCursorDefinition(c)) {
        result_file += "opaque " + name + ";\n";
        append_indentation();
        return;
    }
    result_file += is_union ? "union " : "struct ";
    result_file += name + " {\n";
    indent_level++;
    struct Context {
        Translator* self;
        std::string* output;
    };
    Context ctx{this, &result_file};
    clang_visitChildren(
        c,
        [](CXCursor child, CXCursor, CXClientData data) {
            if (clang_getCursorKind(child) != CXCursor_FieldDecl) return CXChildVisit_Continue;
            auto* ctx = static_cast<Context*>(data);
            CXString field_spelling = clang_getCursorSpelling(child);
            const char* field_name = clang_getCString(field_spelling);
            CXType field_type = clang_getCursorType(child);
            ctx->self->append_indentation();
            *ctx->output += ctx->self->translateType(field_type);
            *ctx->output += " ";
            *ctx->output += field_name ? field_name : "__anonymous_field";
            if (clang_Cursor_isBitField(child)) {
                int width = clang_getFieldDeclBitWidth(child);
                *ctx->output += " : " + std::to_string(width);
            }
            *ctx->output += ";\n";
            clang_disposeString(field_spelling);
            return CXChildVisit_Continue;
        },
        &ctx);
    indent_level--;
    append_indentation();
    result_file += "};\n";
}

void Translator::onEnum(CXCursor c, const std::string& name_override) {
    CXString spelling = clang_getCursorSpelling(c);
    std::string name = name_override.empty() ? (clang_getCString(spelling) ? clang_getCString(spelling) : "") : name_override;
    clang_disposeString(spelling);
    CXType underlying = clang_getEnumDeclIntegerType(c);
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
    std::string name = clang_getCString(name_str);
    clang_disposeString(name_str);
    CXType underlying = clang_getTypedefDeclUnderlyingType(c);
    CXCursor underlyingDecl = clang_getTypeDeclaration(underlying);
    CXString underlyingNameStr = clang_getCursorSpelling(underlyingDecl);
    std::string underlyingName = clang_getCString(underlyingNameStr);
    clang_disposeString(underlyingNameStr);
    if (name == underlyingName) { return; }
    switch (underlying.kind) {
    case CXType_Enum: {
        onEnum(underlyingDecl, name);
        return;
    }
    case CXType_Record: {
        if (!underlyingName.empty()) {
            result_file += "type " + name + " = " + underlyingName + ";\n";
        } else {
            onRecord(underlyingDecl, name);
        }
        return;
    }
    default: result_file += "type " + name + " = " + translateType(underlying) + ";\n"; return;
    }
}
void Translator::onVar(CXCursor c) {
    append_indentation();
    CXType type = clang_getCursorType(c);
    CXString name = clang_getCursorSpelling(c);
    result_file += translateType(type) + " " + clang_getCString(name);
    std::vector<CXCursor> children;
    getChildren(c, &children);
    std::vector<CXCursor> initValues;
    for (CXCursor child : children) {
        if (clang_getCursorKind(child) != CXCursor_TypeRef) {
            initValues.push_back(child);
        }
    }
    if (!initValues.empty()) {
        result_file += " = ";
        visit_expr(initValues.back());
    }
    result_file += ";\n";
    clang_disposeString(name);
}

void Translator::onMacro(CXCursor c) {
    if (clang_Cursor_isMacroFunctionLike(c)) return;
    CXString nameStr = clang_getCursorSpelling(c);
    const char* name = clang_getCString(nameStr);
    result_file += std::string("#define ") + (name ? name : "");
    clang_disposeString(nameStr);
    CXSourceRange range = clang_getCursorExtent(c);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(c);
    CXToken* tokens = nullptr;
    unsigned numTokens = 0;
    clang_tokenize(tu, range, &tokens, &numTokens);
    if (numTokens > 0) {
        result_file += " ";
        for (unsigned i = 1; i < numTokens; ++i) {
            CXString s = clang_getTokenSpelling(tu, tokens[i]);
            const char* text = clang_getCString(s);
            if (text) result_file += text;
            clang_disposeString(s);
            if (i + 1 < numTokens) result_file += " ";
        }
    }
    result_file += "\n";
    clang_disposeTokens(tu, tokens, numTokens);
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
        std::string base = translateType(pointee);
        if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
            std::string out = "fn(";
            int n = clang_getNumArgTypes(pointee);
            for (int i = 0; i < n; ++i) {
                if (i) out += ", ";
                out += translateType(clang_getArgType(pointee, i));
            }
            if (clang_isFunctionTypeVariadic(pointee)) {
                if (n) out += ", ";
                out += "...";
            }
            CXType result = clang_getResultType(pointee);

            return out + ") -> " + translateType(result);
        }
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
        CXType result = clang_getResultType(t);
        int n = clang_getNumArgTypes(t);
        std::string out = translateType(result) + "(";
        for (int i = 0; i < n; ++i) {
            if (i) out += ", ";
            out += translateType(clang_getArgType(t, i));
        }
        if (clang_isFunctionTypeVariadic(t)) {
            if (n) out += ", ";
            out += "...";
        }
        return out + ")";
    }
    case CXType_FunctionNoProto: {
        CXType result = clang_getResultType(t);
        return translateType(result) + "(?)";
    }
    case CXType_Record: {
        CXCursor decl = clang_getTypeDeclaration(t);
        CXString name = clang_getCursorSpelling(decl);
        std::string result = clang_getCString(name) ? clang_getCString(name) : "";
        clang_disposeString(name);
        return result.empty() ? "anonymous_record" : result;
    }
    case CXType_Enum: {
        CXCursor decl = clang_getTypeDeclaration(t);
        CXString name = clang_getCursorSpelling(decl);
        std::string result = clang_getCString(name) ? clang_getCString(name) : "";
        clang_disposeString(name);
        return result.empty() ? "anonymous_enum" : result;
    }
    case CXType_Typedef: {
        CXCursor decl = clang_getTypeDeclaration(t);
        CXString name = clang_getCursorSpelling(decl);
        std::string result = clang_getCString(name) ? clang_getCString(name) : "";
        clang_disposeString(name);
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
    default: error("unsupported type: " + str(clang_getTypeKindSpelling(t.kind))); // you don't need i128
    }
    return "unknown";
}
