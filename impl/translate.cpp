#include "translate.h"
namespace {
std::string str(CXString s) {
    const char* p = clang_getCString(s);
    std::string out = p ? p : "";
    clang_disposeString(s);
    return out;
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
CXChildVisitResult Translator::visit(CXCursor c) {
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(c))) return CXChildVisit_Continue;
    switch (clang_getCursorKind(c)) {
    case CXCursor_FunctionDecl: onFunction(c); break;
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl: onRecord(c); break;
    case CXCursor_EnumDecl: onEnum(c); break;
    case CXCursor_TypedefDecl: onTypedef(c); break;
    case CXCursor_VarDecl: onVar(c); break;
    case CXCursor_MacroDefinition: onMacro(c); break;
    default: break;
    }
    /*
CXCursor_CompoundStmt
CXCursor_DeclStmt
CXCursor_NullStmt
CXCursor_IfStmt
CXCursor_SwitchStmt
CXCursor_CaseStmt
CXCursor_DefaultStmt
CXCursor_WhileStmt
CXCursor_DoStmt
CXCursor_ForStmt
CXCursor_BreakStmt
CXCursor_ContinueStmt
CXCursor_ReturnStmt
CXCursor_GotoStmt
CXCursor_LabelStmt
CXCursor_UnexposedStmt
CXCursor_IntegerLiteral
CXCursor_FloatingLiteral
CXCursor_StringLiteral
CXCursor_CharacterLiteral
CXCursor_ImaginaryLiteral
CXCursor_FixedPointLiteral
CXCursor_DeclRefExpr
CXCursor_MemberRefExpr
CXCursor_ArraySubscriptExpr
CXCursor_UnaryOperator
CXCursor_BinaryOperator
CXCursor_CompoundAssignOperator
CXCursor_ConditionalOperator
CXCursor_ParenExpr
CXCursor_CallExpr
CXCursor_CStyleCastExpr
CXCursor_UnaryExpr
CXCursor_InitListExpr
CXCursor_CompoundLiteralExpr
*/
    return CXChildVisit_Continue;
}

void Translator::onFunction(CXCursor c) {
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
    if (clang_isFunctionTypeVariadic(fn_type)) {
        if (n > 0) result_file += ", ";
        result_file += "...";
    }
    result_file += ");\n";
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
        return;
    }
    result_file += is_union ? "union " : "struct ";
    result_file += name + " {\n";
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
            *ctx->output += "    ";
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
    switch (underlying.kind) {
    case CXType_Enum: onEnum(c, name); return;
    case CXType_Record: onRecord(c, name); return;
    default: result_file += "type " + name + " = " + translateType(underlying) + ";\n"; return;
    }
}

void Translator::onVar(CXCursor c) {
    CXType type = clang_getCursorType(c);
    CXString name = clang_getCursorSpelling(c);
    result_file += translateType(type) + " " + clang_getCString(name);
    CXCursor init = clang_Cursor_getVarDeclInitializer(c);
    if (!clang_Cursor_isNull(init)) {
        CXEvalResult result = clang_Cursor_Evaluate(c);
        if (result) {
            result_file += " = ";
            switch (clang_EvalResult_getKind(result)) {
            case CXEval_Int: result_file += std::to_string(clang_EvalResult_getAsLongLong(result)); break;
            case CXEval_Float: result_file += std::to_string(clang_EvalResult_getAsDouble(result)); break;
            case CXEval_StrLiteral: {
                const char* s = clang_EvalResult_getAsStr(result);
                if (s) result_file += "\"" + std::string(s) + "\"";
                break;
            }
            default: break;
            }
            clang_EvalResult_dispose(result);
        }
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
