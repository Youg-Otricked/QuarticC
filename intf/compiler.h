#ifndef COMPILER_H
#define COMPILER_H
#include "errors.h"
#include "main.h"
#include "nodes.h"
#include "token.h"
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#ifdef ENABLE_LLVM
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#endif
#if defined(_WIN32) || defined(_WIN64)
#include <print>
#endif
extern bool isHeader;
extern uint64_t invokeCounter;
bool isCharInSet(char, const std::string&);
inline int levenshteinDistance(const std::string& a, const std::string& b) { // hehe fancy word
    std::vector<int> prev(b.size() + 1);
    std::vector<int> curr(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= a.size(); i++) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); j++) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            curr[j] = std::min({curr[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}
std::string trim(const std::string& str);
template <typename T, typename V> T* safe_get(V& variant) {
    auto ptr_to_ptr = std::get_if<T*>(&variant);
    if (ptr_to_ptr != nullptr) { return *ptr_to_ptr; }
    return nullptr;
}
#ifdef ENABLE_LLVM
struct AsmOp {
    bool isRW;
    bool isOutput;
    int index;
    char kind;
};
struct FunctionSignature {
    llvm::FunctionType* type;
    std::vector<llvm::Value*> defaultValues;
};
struct ExceptionHandlerInfo {
    llvm::Function* function;
    llvm::BasicBlock* landingPad;
    std::string catchType;
};
class LLVMCompiler {
  public:
    std::string to_snake_case(std::string_view name, bool scream) {
        std::string result;
        result.reserve(name.size());
        for (size_t i = 0; i < name.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(name[i]);
            if (!name.contains("_") && c != std::tolower(c) && i != 0) { result += '_'; }
            result += static_cast<char>(scream ? std::toupper(c) : std::tolower(c));
        }
        return result;
    }
    std::string to_pascal_case(std::string_view name) {
        std::string result;
        result.reserve(name.size());
        bool capitalize = true;
        for (size_t i = 0; i < name.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(name[i]);
            if (c == '_') {
                capitalize = true;
                continue;
            }
            bool word_boundary = i > 0 && std::islower(static_cast<unsigned char>(name[i - 1])) && std::isupper(c);
            bool acronym_boundary = i > 1 && std::isupper(static_cast<unsigned char>(name[i - 1])) && std::isupper(c) && i + 1 < name.size() &&
                                    std::islower(static_cast<unsigned char>(name[i + 1]));
            if (word_boundary || acronym_boundary) { capitalize = true; }
            if (capitalize) {
                result += static_cast<char>(std::toupper(c));
                capitalize = false;
            } else {
                result += static_cast<char>(std::tolower(c));
            }
        }
        return result;
    }
    void add_var_warning(std::string name, Position& pos, bool scream) {
        std::string converted = to_snake_case(name, scream);
        if (converted != name) {
            warn("var-casing", pos,
                 (scream ? ("Constant '" + name + "' is not in SCREAMING_SNAKE_CASE") : ("Variable '" + name + "' is not in snake_case.")),
                 "QC-WC01");
            if (getWarningLevel("var-casing") != run::WarningLevel::Disabled && getWarningLevel("var-casing") != run::WarningLevel::None) {
                cg_help(pos, "Consider '" + converted + "' instead.", converted);
                cg_insight(scream ? "C^4 uses SCREAMING_SNAKE_CASE for constants as it is common across almost all languages."
                                  : "C^4 uses snake_case for variables because it is distinct from all of the other casing conventions allowing "
                                    "quick discerning between functions and variables.");
            }
        }
    }
    void add_type_warning(std::string name, Position& pos) {
        std::string converted = to_pascal_case(name);
        if (converted != name) {
            warn("type-casing", pos, "Usertype '" + name + "' is not in PascalCase.", "QC-WC01");
            if (getWarningLevel("type-casing") != run::WarningLevel::Disabled && getWarningLevel("type-casing") != run::WarningLevel::None) {
                cg_help(pos, "Consider '" + converted + "' instead.", converted);
                cg_insight("C^4 uses PascalCase for user types because that is the standard casing for almost every programming language.");
            }
        }
    }
    run::RunConfig config;
    const std::unordered_set<std::string> core = {"unreachable-code",   "missing-return",    "null-deref",     "asm-clobber-stack-pointer",
                                                  "fn-conflict",        "fn-redecl",         "default-member", "truncation",
                                                  "implicit-int-float", "constant-condition"};
    const std::unordered_set<std::string> extra = {"large-by-value",  "float-equal",       "auto",       "shadow",
                                                   "implicit-extend", "array-param-decay", "empty-body", "empty-catch"};
    const std::unordered_set<std::string> pedantic = {"struct-like-class", "type-casing", "var-casing"};
    run::WarningLevel getWarningLevel(const std::string& warningClass) {
        auto level = config.warnings[warningClass];
        if (level != run::WarningLevel::Disabled) return level;
        if (core.contains(warningClass)) {
            level = config.warnings["core"];
            if (level != run::WarningLevel::Disabled) return level;
        }
        if (extra.contains(warningClass)) {
            level = config.warnings["extra"];
            if (level != run::WarningLevel::Disabled) return level;
        }
        if (pedantic.contains(warningClass)) {
            level = config.warnings["pedantic"];
            if (level != run::WarningLevel::Disabled) return level;
        }
        return config.warnings["all"];
    }
    void warn(const std::string& warningClass, const Position& pos, const std::string& error_text, const std::string& error_code) {
        run::WarningLevel level = getWarningLevel(warningClass);
        if (level == run::WarningLevel::Warning) {
            cg_warn(pos, error_text + " [-W" + warningClass + "]", error_code);
        } else if (level == run::WarningLevel::Error) {
            cg_error(pos, error_text + " [-E" + warningClass + "]", error_code);
        }
    }
    llvm::LLVMContext& context;
    llvm::Module* module;
    llvm::IRBuilder<>* builder;
    bool startsWithQIn(const AnyNode& node) {
        if (auto bin = std::get_if<BinOpNode*>(&node)) { return startsWithQIn((*bin)->left_node); }
        if (auto qin = std::get_if<QInNode>(&node)) { return true; }
        return false;
    }
    std::string baseTypeName(const std::string& mangled) {
        size_t angle = mangled.find('<');
        if (angle == std::string::npos) return mangled;
        int depth = 0;
        size_t end = angle;
        for (size_t i = angle; i < mangled.size(); i++) {
            if (mangled[i] == '<')
                depth++;
            else if (mangled[i] == '>') {
                depth--;
                if (depth == 0) {
                    end = i + 1;
                    break;
                }
            }
        }
        return mangled.substr(0, angle) + mangled.substr(end);
    }
    llvm::Constant* getStringConstant(const std::string& str) {
        llvm::Constant* stringConstant = llvm::ConstantDataArray::getString(context, str, true);
        llvm::GlobalVariable* globalString = new llvm::GlobalVariable(*module, stringConstant->getType(), true, llvm::GlobalValue::PrivateLinkage,
                                                                      stringConstant, ".qc.str");
        llvm::Constant* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::SmallVector<llvm::Constant*, 2> indices = {zero, zero};
        return llvm::ConstantExpr::getInBoundsGetElementPtr(stringConstant->getType(), globalString, indices);
    }
    void generateStructReprFunctions();
    llvm::Value* callStringConcat(llvm::Value* a, llvm::Value* b);
    std::vector<llvm::Type*> getLargestDiscriminantType(const UserTypeInfo& info) {
        std::vector<size_t> sizes;
        auto& DL = module->getDataLayout();
        for (const EnumEntry& entry : info.enumEntries) {
            for (int i = 0; i < entry.tags.size(); i++) {
                if (i >= sizes.size()) { sizes.push_back(0); }
                llvm::Type *ty = llvmTypeFor(entry.tags[i]);
                if (!ty) {
                    cg_error(info.pos, "unknow type: " + entry.tags[i], "QC-EM05");
                    sizes.back() = 1;
                    continue;
                }
                size_t sz = DL.getTypeAllocSize(ty);
                if (sz > sizes.back()) sizes.back() = sz;
            }
        }
        std::vector<llvm::Type*> res = {llvmTypeFor(info.enumType)};
        for (size_t size : sizes) { res.push_back(llvm::ArrayType::get(builder->getIntNTy(8), size)); }
        return res;
    }
    llvm::Type* generateEnum(const std::string& name, const UserTypeInfo& info) {
        if (!info.generics.empty()) return nullptr;
        auto it = enumTypes.find(resolveTypeName(name));
        if (it == enumTypes.end()) return nullptr;
        llvm::StructType* ty = it->second;
        if (ty->isOpaque()) ty->setBody(getLargestDiscriminantType(info));
        return ty;
    }
    void createUserTypes();
    static bool isIndirectType(const std::string& t) { return !t.empty() && (t.back() == '*' || t.back() == '&'); }
    void generateStruct(const std::string& mapKey, const UserTypeInfo& info);
    void generateClass(const std::string& mapKey, const UserTypeInfo& info);
    llvm::Value* convertToString(llvm::Value* val, AnyNode& expr, Position pos);
    LLVMCompiler(std::unordered_map<std::string, UserTypeInfo>& userTys, llvm::Module* mod, llvm::LLVMContext& ctx, bool is_main = false);
    std::vector<CTError> compile(
        StatementsNode* root, std::unordered_map<std::string, FunctionSignature> visibleFunctionSignatures,
        std::unordered_map<std::string, FuncDefNode*> visibleFunctionDefs, std::unordered_map<std::string, std::pair<int, int>> visibleJaggedArrays,
        std::unordered_map<std::string, std::string> visibleArrayTypeStrings, std::unordered_map<std::string, int> visibleArrayLengths,
        std::unordered_map<std::string, std::string> visibleVarTypes, std::unordered_map<std::string, llvm::AllocaInst*> visibleRuntimeArraySizes,
        std::unordered_map<std::string, llvm::FunctionType*> visibleLambdaTypes, std::map<std::string, llvm::Function*> visibleSpecializedFunctions,
        std::unordered_map<std::string, llvm::GlobalVariable*> visibleGlobals);
    bool is_main;
    void cg_warn(const Position& pos, const std::string& msg, std::string code = "");
    void cg_error(const Position& pos, const std::string& msg, std::string code = "");
    void cg_note(const Position& pos, const std::string& msg, bool context = false) {
        if (errors.empty()) return;
        errors.back().notes.emplace_back(pos, msg, context);
    }
    void cg_help(const Position& pos, const std::string& msg, std::optional<std::string> replacement = std::nullopt) {
        if (errors.empty()) return;
        errors.back().helps.emplace_back(pos, msg, std::move(replacement));
    }
    void cg_insight(const std::string& msg) {
        if (errors.empty()) return;
        errors.back().insights.emplace_back(msg);
    }
    std::vector<CTError> errors;
    llvm::BasicBlock* currentBreakBB = nullptr;
    llvm::BasicBlock* currentContinueBB = nullptr;
    std::unordered_map<std::string, std::vector<size_t>> genericMethodIndices;
    bool isEnumType(llvm::Type* ty, std::string* outName = nullptr) {
        auto* st = llvm::dyn_cast<llvm::StructType>(ty);
        if (!st) return false;

        std::string name = st->getName().str();
        auto it = enumTypes.find(name);
        if (it == enumTypes.end()) return false;

        if (outName) *outName = name;
        return true;
    }
    bool isUnionType(llvm::Type* ty, std::string* outName = nullptr) {
        auto* st = llvm::dyn_cast<llvm::StructType>(ty);
        if (!st) return false;

        std::string name = st->getName().str();
        auto it = unionTypes.find(name);
        if (it == unionTypes.end()) return false;

        if (outName) *outName = name;
        return true;
    }
    std::unordered_map<std::string, UserTypeInfo>& userTypes;
    std::string getMethodReturnTypeName(const std::string& typeName, const std::string& methodName) {
        auto it = userTypes.find(baseTypeName(typeName));
        if (it == userTypes.end()) return "";
        std::string returnType;
        for (auto& method : it->second.classMethods) {
            if (method.name_tok.value == methodName) {
                if (method.return_types.empty()) return "";
                returnType = method.return_types[0].value;
                break;
            }
        }
        if (returnType.empty()) return "";
        auto concreteParams = genericParamsFromName(typeName);
        auto& generics = it->second.generics;
        for (size_t i = 0; i < generics.size() && i < concreteParams.size(); ++i) {
            const std::string& genericName = generics[i].name;
            const std::string& concreteType = concreteParams[i];
            size_t pos = 0;
            while ((pos = returnType.find(genericName, pos)) != std::string::npos) {
                size_t end = pos + genericName.size();
                bool leftOk = pos == 0 || !(std::isalnum(static_cast<unsigned char>(returnType[pos - 1])) || returnType[pos - 1] == '_');
                bool rightOk = end == returnType.size() || !(std::isalnum(static_cast<unsigned char>(returnType[end])) || returnType[end] == '_');
                if (leftOk && rightOk) {
                    returnType.replace(pos, genericName.size(), concreteType);
                    pos += concreteType.size();
                } else {
                    pos += genericName.size();
                }
            }
        }
        return returnType;
    }
    std::unordered_map<std::string, std::pair<std::string, std::vector<std::string>>> enumMemberInfo;
    std::unordered_map<std::string, llvm::StructType*> enumTypes;
    std::unordered_map<std::string, std::string> typeAliases;
    std::unordered_map<std::string, llvm::StructType*> structTypes;
    std::unordered_map<std::string, llvm::StructType*> unionTypes;
    std::unordered_map<std::string, llvm::StructType*> classTypes;
    std::unordered_map<std::string, llvm::GlobalVariable*> vtables;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> vtableSlotIndex;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<llvm::Function*>>> classMethods;
    std::unordered_map<std::string, bool> genericClasses;
    std::unordered_map<std::string, bool> genericEnums;
    std::unordered_map<std::string, bool> genericStructs;
    std::unordered_map<std::string, bool> genericAliases;
    std::unordered_map<std::string, bool> genericUnions;
    std::unordered_map<std::string, bool> genericConcepts;
    std::unordered_map<std::string, ConceptInfo> concepts;
    struct ModifierInfo {
        StatementsNode* onCall = nullptr;
        StatementsNode* onReturn = nullptr;
        StatementsNode* onUse = nullptr;
    };
    std::unordered_map<std::string, ModifierInfo> modifiers;
    std::unordered_map<std::string, UserTypeInfo> substitutedUnions;
    std::unordered_map<std::string, llvm::Type*> currentGenericTypes;
    std::unordered_map<std::string, std::string> currentGenericTypeStrings;
    std::unordered_map<std::string, GenericType> currentNonTypeGenericValues;
    std::vector<ExceptionHandlerInfo> handlers;
    llvm::Value* currentThis = nullptr;
    std::string currentClassName = "";
    struct EHHandler {
        TryCatchNode::CatchBody body;
        llvm::BasicBlock* block;
    };

    struct EHScope {
        llvm::BasicBlock* landingPad;
        llvm::BasicBlock* continuation;
        std::vector<EHHandler> handlers;
        size_t deferDepth;
    };
    std::vector<EHScope> ehScopes;

    std::vector<std::vector<StatementsNode*>> defersStack = {{}};
#define defers defersStack.back()
    bool insideTry() { return !ehScopes.empty(); }
    llvm::BasicBlock* currentLandingPad() { return ehScopes.back().landingPad; }
    llvm::Value* copySpreadToArray(llvm::Value* collVal, AnyNode& collExpr, llvm::Value* destArray, llvm::Value* startIndex, llvm::Type* elemTy,
                                   int elemTypeCode);
    std::string substituteGenerics(const std::string& typeStr) {
        size_t anglePos = typeStr.find('<');
        size_t suffix_start = anglePos;
        if (anglePos == std::string::npos) {
            size_t modifierPos = typeStr.find_first_of("*&[");
            std::string base = modifierPos == std::string::npos ? typeStr : typeStr.substr(0, modifierPos);
            std::string suffix = modifierPos == std::string::npos ? "" : typeStr.substr(modifierPos);
            base.erase(0, base.find_first_not_of(" "));
            base.erase(base.find_last_not_of(" ") + 1);
            auto typeIt = currentGenericTypeStrings.find(base);
            if (typeIt != currentGenericTypeStrings.end()) { base = typeIt->second; }
            auto nonTypeIt = currentNonTypeGenericValues.find(base);
            if (nonTypeIt != currentNonTypeGenericValues.end()) { base = nonTypeIt->second.name; }
            for (size_t i = 0; i < suffix.size();) {
                if (suffix[i] == '[') {
                    size_t end = suffix.find(']', i);
                    if (end == std::string::npos) { break; }
                    std::string size = suffix.substr(i + 1, end - i - 1);
                    std::string newSize = substituteGenerics(size);
                    suffix.replace(i + 1, end - i - 1, newSize);
                    i += newSize.size() + 2;
                } else {
                    i++;
                }
            }
            return base + suffix;
        }
        std::string baseName = typeStr.substr(0, anglePos);
        std::string inner = typeStr.substr(anglePos + 1, typeStr.rfind('>') - anglePos - 1);
        std::string suffix = typeStr.substr(typeStr.rfind('>') + 1); // catches trailing * & etc
        std::vector<std::string> args;
        int depth = 0;
        std::string current;
        for (char c : inner) {
            if (c == '<')
                depth++;
            else if (c == '>')
                depth--;
            else if (c == ',' && depth == 0) {
                args.push_back(current);
                current.clear();
                continue;
            }
            current += c;
        }
        if (!current.empty()) args.push_back(current);
        for (auto& arg : args) arg = substituteGenerics(arg);
        std::string result = baseName + "<";
        for (size_t i = 0; i < args.size(); i++) {
            if (i != 0) result += ",";
            result += args[i];
        }
        return result + ">" + suffix;
    }
    unsigned pointerSizeBits;
    llvm::Value* createRuntimeSizedArray(std::vector<AnyNode>& elements, llvm::Value* totalSize);
    void expandSpreadIntoVector(llvm::Value* collVal, AnyNode& collExpr, std::vector<llvm::Value*>& elements);
    llvm::Value* emitSpreadFunctionCall(llvm::Value* calleeVal, llvm::FunctionType* fnTy, CallNode& call);
    bool fulfillsGenericConstraints(std::vector<GenericType> generics, std::vector<std::string> genericParams, Position pos = Position());
    llvm::StructType* generateGenericEnum(std::string enumName, UserTypeInfo enumInfo, std::vector<std::string> genericParams);
    ConceptInfo generateGenericConcept(std::string conceptName, UserTypeInfo conceptInfo, std::vector<std::string> genericParams);
    llvm::StructType* generateGenericClass(std::string className, UserTypeInfo classInfo, std::vector<std::string> genericParams);
    llvm::StructType* generateGenericStruct(std::string structName, UserTypeInfo structInfo, std::vector<std::string> genericParams);
    UserTypeInfo generateGenericUnion(std::string unionName, UserTypeInfo unionInfo, std::vector<std::string> genericParams);
    std::string generateGenericAlias(std::string aliasName, UserTypeInfo aliasInfo, std::vector<std::string> genericParams);
    std::unordered_map<std::string, llvm::GlobalVariable*> globals;
    std::unordered_map<std::string, FunctionSignature> functionSignatures;
    std::unordered_map<std::string, FuncDefNode*> functionDefs;
    std::vector<std::unordered_map<std::string, std::string>> arrayTypeStringsStack;
    std::vector<std::unordered_map<std::string, int>> arrayLengthsStack;
    std::vector<std::unordered_map<std::string, std::string>> varTypesStack;
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> localsStack;
    std::vector<std::unordered_map<std::string, bool>> volatileVarsStack;
    std::unordered_map<std::string, llvm::AllocaInst*> runtimeArraySizes;
    std::unordered_map<std::string, llvm::FunctionType*> lambdaTypes;
    int findUnionVariantTag(const std::string& unionName, AnyNode& valueNode, llvm::Value* val);
    llvm::Value* storeAndGetPointer(llvm::Value* val);
    std::map<std::string, llvm::Function*> specializedFunctions;
    template <typename MapType> bool foundInStack(const std::vector<MapType>& stack, const std::string& key) {
        if (stack.empty()) return false;
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (it->find(key) != it->end()) { return true; }
        }
        return false;
    }
    unsigned getPtrSize() { return pointerSizeBits; }
#define hasVarType(name) foundInStack(varTypesStack, name)
#define hasLocal(name) foundInStack(localsStack, name)
#define hasArrayType(name) foundInStack(arrayTypeStringsStack, name)
#define hasArrayLength(name) foundInStack(arrayLengthsStack, name)
#define hasVolatileVar(name) foundInStack(volatileVarsStack, name)
#define volatileVars (volatileVarsStack.back())
#define arrayLengths (arrayLengthsStack.back())
#define locals (localsStack.back())
#define varTypes (varTypesStack.back())
#define arrayTypeStrings (arrayTypeStringsStack.back())
#define findLocal(name) findInStack(localsStack, name)
#define findVolatileVar(name) findInStack(volatileVarsStack, name)
#define findArrayLength(name) findInStack(arrayLengthsStack, name)
#define findVarType(name) findInStack(varTypesStack, name)
#define findArrayType(name) findInStack(arrayTypeStringsStack, name)
    void enterScope() {
        defersStack.push_back({});
        localsStack.push_back({});
        arrayTypeStringsStack.push_back({});
        arrayLengthsStack.push_back({});
        varTypesStack.push_back({});
        volatileVarsStack.push_back({});
    }

    void exitScope() {
        auto* currentBlock = builder->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) { emitDefersDownTo(defersStack.size()); }
        defersStack.pop_back();
        localsStack.pop_back();
        volatileVarsStack.pop_back();
        arrayTypeStringsStack.pop_back();
        arrayLengthsStack.pop_back();
        varTypesStack.pop_back();
    }
    size_t targetDeferDepth = 0;
    std::vector<size_t> loopStack;
    void emitDefersDownTo(size_t targetDepth) {
        if (defersStack.empty()) return;
        int first = std::max(0, (int)targetDepth - 1);
        for (int i = (int)defersStack.size() - 1; i >= first; --i) {
            auto defersToRun = defersStack[i];
            for (auto it = defersToRun.rbegin(); it != defersToRun.rend(); ++it) {
                enterScope();
                emitStmt(*it);
                exitScope();
            }
        }
    }
    llvm::Value* getCollectionLength(llvm::Value* collVal, AnyNode& collExpr);
    llvm::Value* expandSpreadIntoArrays(llvm::Value* collVal, AnyNode& collExpr, llvm::AllocaInst* argsArray, llvm::AllocaInst* typesArray,
                                        llvm::Value* startIndex);
    template <typename MapType> auto findInStack(std::vector<MapType>& stack, const std::string& key) {
        if (stack.empty()) {
            cg_error(Position(Position::INVALID_FILE_ID, 0, 0, 0), "Stack is empty", "QC-S282");
            return stack.back().end();
        }
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto found = it->find(key);
            if (found != it->end()) { return found; }
        }
        return stack.back().end();
    }
    std::string makeTypeSignature(const std::vector<std::string>& types) {
        std::string sig;
        for (auto& t : types) {
            if (!sig.empty()) sig += "_";
            sig += t;
        }
        return sig;
    }
    std::string getExpressionType(AnyNode& node, bool strip = true) {
        if (auto mapLit = std::get_if<MapLiteralNode*>(&node)) {
            if (!(*mapLit)->struct_type.empty()) { return (*mapLit)->struct_type; }
        } else if (auto arrLit = std::get_if<ArrayLiteralNode*>(&node)) {
            if (!(*arrLit)->type.empty()) { return (*arrLit)->type; }
            if (!(*arrLit)->elements.empty()) {
                std::string elemType = getExpressionType((*arrLit)->elements[0]);
                return elemType + "[]";
            }
            return "int[]";
        } else if (auto unaryOp = std::get_if<UnaryOpNode*>(&node)) {
            std::string type = getExpressionType((*unaryOp)->node, strip);
            if ((*unaryOp)->op_tok.type == TokenType::MUL) {
                if (type.ends_with("*")) { type.pop_back(); }
            }
            if ((*unaryOp)->op_tok.type == TokenType::AMPERSAND) { return type + "*"; }
            if ((*unaryOp)->op_tok.type == TokenType::SIZEOF) { return "addr_t"; }
            return type;
        } else if (auto binOp = std::get_if<BinOpNode*>(&node)) {
            std::string leftType = getExpressionType((*binOp)->left_node);
            std::string rightType = getExpressionType((*binOp)->right_node);
            if (leftType == "char" && rightType == "char") return "int";
            if (leftType == rightType) return leftType;
            if (leftType == "double" || rightType == "double") return "double";
            if (leftType == "float" || rightType == "float") return "float";
            return leftType;
        } else if (auto strNode = std::get_if<StringNode>(&node)) {
            return "string";
        } else if (auto numNode = std::get_if<NumberNode>(&node)) {
            switch (numNode->tok.type) {
            case TokenType::INT: return "int";
            case TokenType::FLOAT: return "float";
            case TokenType::DOUBLE: return "double";
            case TokenType::LONG_DOUBLE: return "long double";
            case TokenType::LONG_INT: return "long int";
            case TokenType::SHORT_INT: return "short int";
            case TokenType::ADDR_T: return "addr_t";
            case TokenType::BYTE: return "byte";
            case TokenType::NIBBLE: return "nibble";
            default: break;
            }
        } else if (auto boolNode = std::get_if<BoolNode>(&node)) {
            return "bool";
        } else if (auto charNode = std::get_if<CharNode>(&node)) {
            return "char";
        } else if (auto qboolNode = std::get_if<QBoolNode>(&node)) {
            return "qbool";
        } else if (auto varAcc = std::get_if<VarAccessNode*>(&node)) {
            std::string varName = (*varAcc)->var_name_tok.value;
            if (!this->resolveVarType(varName).empty()) {
                std::string t = this->resolveVarType(varName);
                if (t.ends_with("&") && strip) t.pop_back();
                return substituteGenerics(t);
            }
            if (hasArrayType(varName)) { return substituteGenerics(findArrayType(varName)->second) + "[]"; }
        } else if (auto arrAcc = std::get_if<ArrayAccessNode*>(&node)) {
            std::string baseType = getExpressionType((*arrAcc)->base);
            if (baseType.ends_with(']')) {
                const size_t open = baseType.rfind('[');
                if (open != std::string::npos) { return baseType.substr(0, open); }
            }
            if (baseType.ends_with("*")) {
                return baseType.substr(0, baseType.size() - 1);
            } else if (baseType == "string") {
                return "char";
            } else {
                if (auto varAcc = std::get_if<VarAccessNode*>(&(*arrAcc)->base)) {
                    std::string name = (*varAcc)->var_name_tok.value;
                    if (auto arrayType = resolveArrayType(name)) { return *arrayType; }
                }
            }
            std::string className = baseTypeName(baseType);
            while (!className.empty() && userTypes.contains(className)) {
                auto& info = userTypes[className];
                auto originalGenericMap = currentGenericTypeStrings;
                std::string ret;
                for (auto& m : info.classMethods) {
                    if (m.name_tok.value == "operator[]" && !m.return_types.empty()) {
                        ret = m.return_types[0].value;
                        std::vector<std::string> actualArgs = genericParamsFromName(baseType);
                        auto& formalParams = userTypes[className].generics;
                        for (size_t i = 0; i < formalParams.size() && i < actualArgs.size(); ++i) {
                            currentGenericTypeStrings[formalParams[i].name] = actualArgs[i];
                        }
                        if (ret.ends_with("&") && strip) ret.pop_back();
                        ret = substituteGenerics(ret);
                        currentGenericTypeStrings = std::move(originalGenericMap);
                        return ret;
                    }
                }
                std::string parentTypeStr = info.baseClassName;
                if (parentTypeStr.empty()) break;
                std::vector<std::string> parentActualArgs = genericParamsFromName(parentTypeStr);
                std::string nextClassName = baseTypeName(parentTypeStr);
                std::unordered_map<std::string, std::string> nextGenericMap;
                if (userTypes.contains(nextClassName)) {
                    auto& parentFormalParams = userTypes[nextClassName].generics;
                    for (size_t i = 0; i < parentFormalParams.size() && i < parentActualArgs.size(); ++i) {
                        nextGenericMap[parentFormalParams[i].name] = substituteGenerics(parentActualArgs[i]);
                    }
                }
                className = nextClassName;
                currentGenericTypeStrings = std::move(nextGenericMap);
            }
            return "unknown";
        } else if (auto nullp = std::get_if<NullptrNode>(&node)) {
            return "@nullptr";
        } else if (auto propAcc = std::get_if<PropertyAccessNode*>(&node)) {
            if (auto varAccess = std::get_if<VarAccessNode*>(&*(*propAcc)->base)) {
                auto baseName = (*varAccess)->var_name_tok.value;
                std::string resolved = resolveTypeName(baseName);
                auto enumIt = enumTypes.find(resolved);
                if (enumIt != enumTypes.end()) return resolved;
            }
            std::string currentType = getExpressionType(*((*propAcc)->base));
            if (currentType.ends_with("*") || currentType.ends_with("&")) { currentType.pop_back(); }
            std::string fieldName = (*propAcc)->property_name.value;
            while (!currentType.empty() && userTypes.contains(baseTypeName(currentType))) {
                auto& info = userTypes[baseTypeName(currentType)];
                auto savedGenericTypeStrings = currentGenericTypeStrings;
                auto savedNonTypeGenerics = currentNonTypeGenericValues;
                currentGenericTypeStrings.clear();
                currentNonTypeGenericValues.clear();
                std::vector<std::string> concreteArgs = genericParamsFromName(currentType);
                for (size_t idx = 0; idx < info.generics.size(); ++idx) {
                    GenericType gen = info.generics[idx];
                    std::string val = (idx < concreteArgs.size()) ? concreteArgs[idx] : gen.defaultValue;
                    if (val.empty()) continue;
                    if (gen.isNonType) {
                        std::string gname = gen.name;
                        gen.name = val;
                        currentNonTypeGenericValues[gname] = gen;
                    } else {
                        currentGenericTypeStrings[gen.name] = val;
                    }
                }
                for (const auto& f : info.fields) {
                    if (f.name == fieldName) {
                        std::string res = substituteGenerics(f.type);
                        currentGenericTypeStrings = savedGenericTypeStrings;
                        currentNonTypeGenericValues = savedNonTypeGenerics;
                        return res;
                    }
                }
                for (const auto& f : info.classFields) {
                    if (f.name == fieldName) {
                        std::string res = substituteGenerics(f.type);
                        currentGenericTypeStrings = savedGenericTypeStrings;
                        currentNonTypeGenericValues = savedNonTypeGenerics;
                        return res;
                    }
                }
                currentGenericTypeStrings = savedGenericTypeStrings;
                currentNonTypeGenericValues = savedNonTypeGenerics;
                currentType = info.baseClassName;
            }
            return "unknown";
        } else if (auto callNode = std::get_if<CallNode*>(&node)) {
            if (auto varAcc = std::get_if<VarAccessNode*>(&(*callNode)->node_to_call)) {
                std::string funcName = (*varAcc)->var_name_tok.value;
                if (funcName == "`is_empty" || funcName == "`to_bool") return "bool";
                if (funcName == "`inline" || funcName == "`qout") return "void";
                if (funcName == "`time") return "addr_t";
                if (funcName == "`seed") return "void";
                if (funcName == "`random") {
                    if ((*callNode)->arg_nodes.size() > 1) { return "double"; }
                    return "int";
                }
                if (funcName == "`len") return "addr_t";
                if (funcName == "`to_lower" || funcName == "`to_upper" || funcName == "`substring" || funcName == "`trim" || funcName == "`replace" ||
                    funcName == "`read" || funcName == "`to_string")
                    return "string";
                if (funcName == "`contains" || funcName == "`startswith" || funcName == "`endswith") return "bool";
                if (funcName == "`to_int") return "int";
                if (funcName == "`to_float") return "float";
                if (funcName == "`to_double") return "double";
                if (funcName == "`to_char") return "char";
                if (funcName == "`to_addr_t" || funcName == "`to_address") return "addr_t";
                if (funcName == "`to_qbool") return "qbool";
                if (funcName == "`to_long_int") return "long int";
                if (funcName == "`to_short_int") return "short int";
                if (funcName == "`to_byte") return "byte";
                if (funcName == "`to_nibble") return "nibble";
                if (funcName == "`open") return "int";
                if (funcName == "`close" || funcName == "`write" || funcName == "`free" || funcName == "`flush") return "void";
                if (funcName == "`malloc" || funcName == "`calloc" || funcName == "`realloc" || funcName == "`mapped_ptr") return "void*";

                if (funcName == "`typeof") { return "string"; }
                if (funcName == "`ternary") {
                    if ((*callNode)->arg_nodes.size() < 3) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `ternary", "QC-S283");
                        cg_note((*varAcc)->var_name_tok.pos, "`ternary expects 3 arguments: condition, true_value, false_value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    return getExpressionType(*std::next((*callNode)->arg_nodes.begin()));
                }
                if (funcName == "`next") {
                    if ((*callNode)->arg_nodes.size() < 2) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `next", "QC-S284");
                        cg_note((*varAcc)->var_name_tok.pos, "`next expects 2 arguments");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = *std::next((*callNode)->arg_nodes.begin());
                    if (auto str = std::get_if<StringNode>(&node)) {
                        return resolveTypeName(substituteGenerics(str->tok.value), false);
                    } else if (auto ty = std::get_if<TypeValueNode>(&node)) {
                        return resolveTypeName(substituteGenerics(ty->tok.value), false);
                    } else {
                        cg_error((*varAcc)->var_name_tok.pos, "arg 2 to `next is a comptime string", "QC-S285");
                        cg_note((*varAcc)->var_name_tok.pos, "expected comptime string, got " + getExpressionType(node));
                        return "unknown";
                    }
                }
                if (funcName == "`opendir") return "void*";
                if (funcName == "`readdir") return "bool";
                if (funcName == "`closedir") return "int";
                if (funcName == "`lseek") return "long int";
                if (funcName == "`cast") {
                    if ((*callNode)->arg_nodes.size() < 2) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `cast", "QC-S287");
                        cg_note((*varAcc)->var_name_tok.pos, "`cast expects 2 arguments");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.back();
                    if (auto n = std::get_if<TypeValueNode>(&node)) return n->tok.value;
                    return "unknown";
                }
                if (funcName == "`float_bits") return "float";
                if (funcName == "`double_bits") return "double";
                if (funcName == "`atomic_load") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_load", "QC-S288");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_load expects 1 argument: atomic variable");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_store") { return "void"; }
                if (funcName == "`atomic_exchange") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_exchange", "QC-S289");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_exchange expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_add") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_add", "QC-S290");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_add expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_sub") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_sub", "QC-S291");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_sub expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_and") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_and", "QC-S292");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_and expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_or") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_or", "QC-S293");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_or expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_xor") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_xor", "QC-S294");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_xor expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_nand") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_nand", "QC-S295");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_nand expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_min") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_min", "QC-S296");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_min expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_max") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_max", "QC-S297");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_max expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_umin") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_umin", "QC-S298");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_umin expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_umax") {
                    if ((*callNode)->arg_nodes.size() < 1) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_umax", "QC-S299");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_umax expects 2 arguments: atomic variable, value");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    return getExpressionType(node, false);
                }
                if (funcName == "`atomic_cmpxchg") {
                    if ((*callNode)->arg_nodes.size() < 3) {
                        cg_error((*varAcc)->var_name_tok.pos, "too few arguments to `atomic_cmpxchg", "QC-S300");
                        cg_note((*varAcc)->var_name_tok.pos, "`atomic_cmpxchg expects 3 arguments: atomic variable, expected, desired");
                        cg_note((*varAcc)->var_name_tok.pos, "got " + std::to_string((*callNode)->arg_nodes.size()) + " arguments");
                        return "unknown";
                    }
                    auto node = (*callNode)->arg_nodes.front();
                    std::string valueType = getExpressionType(node, false);
                    return "{ " + valueType + ", bool }";
                }
                if (funcName == "`atomic_fence") { return "void"; }
                if (classTypes.contains(resolveTypeName(funcName, false))) { return resolveTypeName(funcName, false); }
                if (functionDefs.contains(funcName)) {
                    auto* def = functionDefs[funcName];
                    if (!def->return_types.empty()) {
                        std::string ret = def->return_types[0].value;
                        if (ret.ends_with("&") && strip) ret.pop_back();
                        return substituteGenerics(ret);
                    }
                }
            }
            return "unknown";
        } else if (auto methCall = std::get_if<MethodCallNode*>(&node)) {
            if (auto varAccess = std::get_if<VarAccessNode*>(&(*methCall)->base)) {
                auto baseName = (*varAccess)->var_name_tok.value;
                std::string resolved = resolveTypeName(baseName);
                auto intTy = userTypes.find(baseTypeName(resolved));
                if (intTy != userTypes.end()) { return resolved; }
            }
            std::string baseType = getExpressionType((*methCall)->base);
            if (baseType.ends_with("*") || baseType.ends_with("&")) baseType.pop_back();
            auto originalGenericMap = currentGenericTypeStrings;
            std::vector<std::string> actualArgs = genericParamsFromName(baseType);
            std::string className = baseTypeName(baseType);
            std::string methodName = (*methCall)->method_name.value;
            if (userTypes.contains(className)) {
                auto& formalParams = userTypes[className].generics;
                for (size_t i = 0; i < formalParams.size() && i < actualArgs.size(); ++i) {
                    currentGenericTypeStrings[formalParams[i].name] = actualArgs[i];
                }
            }
            std::string foundReturnType = "unknown";
            while (!className.empty() && userTypes.contains(className)) {
                auto& info = userTypes[className];
                bool methodFound = false;
                for (auto& m : info.classMethods) {
                    if (m.name_tok.value == methodName && !m.return_types.empty()) {
                        std::string ret = m.return_types[0].value;
                        if (ret.ends_with("&") && strip) ret.pop_back();
                        foundReturnType = substituteGenerics(ret);
                        methodFound = true;
                        break;
                    }
                }
                if (methodFound) break;
                std::string parentTypeStr = info.baseClassName;
                if (parentTypeStr.empty()) break;
                std::vector<std::string> parentActualArgs = genericParamsFromName(parentTypeStr);
                std::string nextClassName = baseTypeName(parentTypeStr);
                std::unordered_map<std::string, std::string> nextGenericMap;
                if (userTypes.contains(nextClassName)) {
                    auto& parentFormalParams = userTypes[nextClassName].generics;
                    for (size_t i = 0; i < parentFormalParams.size() && i < parentActualArgs.size(); ++i) {
                        nextGenericMap[parentFormalParams[i].name] = substituteGenerics(parentActualArgs[i]);
                    }
                }
                className = nextClassName;
                currentGenericTypeStrings = std::move(nextGenericMap);
            }
            currentGenericTypeStrings = std::move(originalGenericMap);

            return foundReturnType;
        } else if (TypeValueNode* typeValue = std::get_if<TypeValueNode>(&node)) {
            return resolveTypeName(typeValue->tok.value, false);
        } else if (ModifierNode* modNode = safe_get<ModifierNode>(node)) {
            if (!modNode || modNode->modifiers.empty()) { return getExpressionType(modNode->node, strip); }
            std::string currentType = getExpressionType(modNode->node, strip);
            for (int i = (int)modNode->modifiers.size() - 1; i >= 0; --i) {
                Token modTok = modNode->modifiers[i];
                if (!modifiers.count(modTok.value)) continue;
                ModifierInfo& modInfo = modifiers[modTok.value];
                if (!modInfo.onUse) continue;
                enterScope();
                varTypes["value"] = currentType;
                std::string retType = "void";
                for (auto& stmt : modInfo.onUse->statements) {
                    if (std::holds_alternative<ReturnNode*>(stmt)) {
                        auto* retNode = std::get<ReturnNode*>(stmt);
                        if (retNode) { retType = getExpressionType(retNode->value, false); }
                        break;
                    }
                }
                exitScope();
                currentType = retType;
            }
            return currentType;
        }
        return "unknown";
    }
    llvm::Value* emitModifierNode(ModifierNode* node) {
        if (!node || node->modifiers.empty()) { return emitExpr(node->node); }
        llvm::Value* currentValue = emitExpr(node->node);
        if (!currentValue) return nullptr;
        std::string currentTypeStr = getExpressionType(node->node, false);
        for (int i = (int)node->modifiers.size() - 1; i >= 0; --i) {
            Token modTok = node->modifiers[i];
            if (!modifiers.count(modTok.value)) {
                cg_error(modTok.pos, "unknown modifier '" + modTok.value + "'", "QC-S112");
                addTypeNotes(modTok.value, modTok.pos, UserTypeKind::Modifier);
                return nullptr;
            }
            ModifierInfo& modInfo = modifiers[modTok.value];
            if (!modInfo.onUse) {
                cg_error(modTok.pos, "modifier '" + modTok.value + "' has no 'on_use' handler", "QC-S301");
                return nullptr;
            }
            static unsigned onUseId = 0;
            Token helperNameTok = modTok;
            helperNameTok.value = "_on_use_" + modTok.value + "_" + std::to_string(onUseId++);
            std::list<Parameter> params;
            Parameter valParam;
            valParam.name = Token{TokenType::IDENTIFIER, "value", modTok.pos};
            valParam.type = Token{TokenType::IDENTIFIER, currentTypeStr, modTok.pos};
            params.push_back(valParam);
            enterScope();
            varTypes["value"] = currentTypeStr;
            std::string deducedRetType = "void";
            for (auto& stmt : modInfo.onUse->statements) {
                if (std::holds_alternative<ReturnNode*>(stmt)) {
                    auto* retNode = std::get<ReturnNode*>(stmt);
                    if (retNode) { deducedRetType = getExpressionType(retNode->value, false); }
                    break;
                }
            }
            exitScope();
            std::vector<Token> retTypes;
            if (deducedRetType != "void") { retTypes.push_back(Token{TokenType::IDENTIFIER, deducedRetType, modTok.pos}); }
            FuncDefNode helperFn(retTypes, std::optional<Token>(helperNameTok), params, modInfo.onUse);
            llvm::Function* helperFunc = emitFuncDef(helperFn);
            if (!helperFunc) return nullptr;
            currentValue = builder->CreateCall(helperFunc, {currentValue});
        }
        return currentValue;
    }

    bool returnsRef(unsigned index = 0) {
        auto* md = currentFunction->getMetadata("qc.return_types");
        if (!md || index >= md->getNumOperands()) return false;

        auto* s = llvm::dyn_cast<llvm::MDString>(md->getOperand(index));
        return s && s->getString().ends_with("&");
    }
    /*
    class TypedValue {
        llvm::Value* val;
        std::string qcType; // "int&", "int*", "string", etc.
        TypedValue(llvm::Value* val) {
            this->val = val;
        }
        TypedValue(std::string qcType, llvm::Value* val) {
            this->qcType = qcType;
            this->val = val;
        }
        operator llvm::Value*() const { return this->val; }
    };
    */
    llvm::Value* derefIfReference(llvm::Value* val, AnyNode& argNode) {
        val = normalizeValue(val, argNode);
        if (!val || !val->getType()->isPointerTy()) return val;
        std::string qcType = substituteGenerics(getExpressionType(argNode, false));
        if (!qcType.ends_with("&")) return val;
        if (qcType.ends_with("&")) qcType.pop_back();
        llvm::Type* pointeeTy = llvmTypeFor(qcType);
        if (!pointeeTy || pointeeTy->isVoidTy()) return val;
        return builder->CreateLoad(pointeeTy, val, "deref_ref");
    }
    llvm::Value* emitPropertyAddress(PropertyAccessNode& prop, bool fallback = false) {
        std::string propName = prop.property_name.value;
        llvm::Value* baseAddr = emitLValue(*prop.base);
        if (!baseAddr) {
            llvm::Value* rval = emitExpr(*prop.base);
            baseAddr = createEntryAlloca("temp_lval_base", rval->getType());
            builder->CreateStore(rval, baseAddr);
        }
        std::string typeName = getExpressionType(*prop.base);
        if (classTypes.count(typeName) || (genericClasses.count(baseTypeName(typeName)) && genericClasses[baseTypeName(typeName)])) {
            llvm::StructType* classTy = genericiseOrFindClass(typeName);
            int fieldIdx = getFlattenedFieldIndex(baseTypeName(typeName), propName);
            if (fieldIdx == -1) {
                cg_error(prop.property_name.pos, "field " + propName + " not found in class " + typeName, "QC-S247");
                if (propName.length() > 3) {
                    std::vector<std::pair<int, std::string>> suggestions;
                    for (auto& field : userTypes[baseTypeName(typeName)].classFields) {
                        int distance = levenshteinDistance(propName, field.name);
                        if (distance <= 2) { suggestions.push_back({distance, field.name}); }
                    }
                    std::sort(suggestions.begin(), suggestions.end());
                    if (!suggestions.empty()) {
                        std::string note = "similar fields:";
                        for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                        cg_note(prop.property_name.pos, note);
                    }
                }
                return nullptr;
            }
            auto [fieldOwnerClass, fieldAccess] = getFieldOwner(baseTypeName(typeName), propName);
            if (!canAccessField(currentClassName, fieldOwnerClass, fieldAccess)) {
                cg_error(prop.property_name.pos, "cannot access " + fieldAccess + " field " + propName, "QC-S248");
                return nullptr;
            }
            return builder->CreateStructGEP(classTy, baseAddr, fieldIdx, propName + "_ptr");
        }
        if (structTypes.count(typeName) || (genericStructs.count(baseTypeName(typeName)) && genericStructs[baseTypeName(typeName)])) {
            auto structTy = genericiseOrFindStruct(typeName);
            auto& info = userTypes[baseTypeName(typeName)];
            int fieldIdx = -1;
            for (size_t i = 0; i < info.fields.size(); i++) {
                if (info.fields[i].name == propName) {
                    fieldIdx = (int)i;
                    break;
                }
            }
            if (fieldIdx == -1) {
                cg_error(prop.property_name.pos, "field " + propName + " not found in struct " + typeName, "QC-S302");
                if (propName.length() > 3) {
                    std::vector<std::pair<int, std::string>> suggestions;
                    for (auto& field : userTypes[baseTypeName(typeName)].fields) {
                        int distance = levenshteinDistance(propName, field.name);
                        if (distance <= 2) { suggestions.push_back({distance, field.name}); }
                    }
                    std::sort(suggestions.begin(), suggestions.end());
                    if (!suggestions.empty()) {
                        std::string note = "similar fields:";
                        for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                        cg_note(prop.property_name.pos, note);
                    }
                }

                return nullptr;
            }
            return builder->CreateStructGEP(structTy, baseAddr, fieldIdx, propName + "_ptr");
        }
        if (unionTypes.count(typeName) || (genericUnions.count(baseTypeName(typeName)) && genericUnions[baseTypeName(typeName)])) {
            auto unionInfo = genericiseOrFindUnion(typeName);
            llvm::StructType* unionTy = unionTypes[typeName];
            for (auto& member : unionInfo.members) {
                std::string variantName = resolveTypeName(member.type);
                int fieldIdx = getFlattenedFieldIndex(variantName, propName);
                if (fieldIdx == -1 && userTypes.count(variantName)) {
                    auto& sInfo = userTypes[variantName];
                    for (size_t i = 0; i < sInfo.fields.size(); i++) {
                        if (sInfo.fields[i].name == propName) {
                            fieldIdx = i;
                            break;
                        }
                    }
                }
                if (fieldIdx != -1) {
                    llvm::Value* dataFieldPtr = builder->CreateStructGEP(unionTy, baseAddr, 1);
                    llvm::Value* dataPtr = builder->CreateLoad(llvm::PointerType::get(context, 0), dataFieldPtr);
                    llvm::Type* varTy = nullptr;
                    if (classTypes.count(variantName))
                        varTy = genericiseOrFindClass(variantName);
                    else if (structTypes.count(variantName))
                        varTy = genericiseOrFindStruct(variantName);
                    if (!varTy) continue;
                    return builder->CreateStructGEP(varTy, dataPtr, fieldIdx, propName + "_ptr");
                }
            }
        }
        if (fallback) return emitExpr(&prop);
        cg_error(prop.property_name.pos, "cannot resolve address for property '" + propName + "' on type '" + typeName + "'", "QC-T060");
        return nullptr;
    }
    llvm::StructType* getOrCreateStructType(std::vector<llvm::Type*> fields, const std::string& name) {
        if (auto* existing = llvm::StructType::getTypeByName(context, name)) { return existing; }
        auto* newTy = llvm::StructType::create(context, fields, name);
        return newTy;
    }
    llvm::StructType* getOrCreateStructType(const std::string& name) {
        if (auto* existing = llvm::StructType::getTypeByName(context, name)) { return existing; }
        auto* newTy = llvm::StructType::create(context, name);
        return newTy;
    }
#if !defined(__OPTIMIZE__)
    template <typename T> void dump_val(T* v) {
        if (v) {
            v->dump();
            llvm::errs() << "\n";
        }
    }
#endif
    llvm::Value* emitLValue(AnyNode& node, bool fallback = false) {
        if (auto var = std::get_if<VarAccessNode*>(&node)) {
            std::string name = (*var)->var_name_tok.value;
            if (name == "this") {
                if (currentThis) {
                    return currentThis;
                } else {
                    cg_error((*var)->var_name_tok.pos, "'this' used outside class method", "QC-S150");
                    return nullptr;
                }
            }
            llvm::Value* addr = getVarAddress(name);
            if (!addr && fallback) return emitExpr(node);
            return addr;
        } else if (auto unary = std::get_if<UnaryOpNode*>(&node)) {
            if ((*unary)->op_tok.type == TokenType::MUL) { return emitExpr((*unary)->node); }
            if ((*unary)->op_tok.type == TokenType::AMPERSAND) { return emitLValue((*unary)->node); }
            if (fallback) return emitExpr(node);
        } else if (auto prop = std::get_if<PropertyAccessNode*>(&node)) {
            return emitPropertyAddress(**prop, fallback);
        } else if (auto call = std::get_if<CallNode*>(&node)) {
            std::string retType = getExpressionType(node, false);
            if (retType.ends_with("&") || retType.ends_with("*")) { return emitExpr(node); }
            if (fallback) return emitExpr(node);
        } else if (auto method = std::get_if<MethodCallNode*>(&node)) {
            std::string retType = getExpressionType(node, false);
            if (retType.ends_with("&") || retType.ends_with("*")) { return emitExpr(node); }
            if (fallback) return emitExpr(node);
        } else if (auto arrAcc = std::get_if<ArrayAccessNode*>(&node)) {
            std::string ptrTy = getExpressionType((*arrAcc)->base);
            if (ptrTy.ends_with("*")) {
                ptrTy.pop_back();
                llvm::Value* base = emitExpr((*arrAcc)->base);
                if (!base) return nullptr;
                llvm::Value* idx = emitExpr((*arrAcc)->indices[0]);
                if (!idx) return nullptr;
                return builder->CreateGEP(llvmTypeFor(ptrTy), base, idx, "lval_ptr_arr_addr");
            }
            if (ptrTy.ends_with("[]")) {
                llvm::Value* slot = emitLValue((*arrAcc)->base);
                llvm::Value* index = emitExpr((*arrAcc)->indices[0]);
                if (!slot || !index) return nullptr;
                std::string elementName = ptrTy.substr(0, ptrTy.size() - 2);
                llvm::Type* elementTy = llvmTypeFor(elementName);
                if (!elementTy) {
                    cg_error(get_pos((*arrAcc)->base), "invalid array element type: " + elementName, "QC-T061");
                    return nullptr;
                }
                llvm::Value* dataPtr = builder->CreateLoad(builder->getPtrTy(), slot, "array_data");
                return builder->CreateInBoundsGEP(elementTy, dataPtr, index, "lval_arr_addr");
            }
            if (ptrTy.ends_with("]")) {
                llvm::Value* base = emitLValue((*arrAcc)->base);
                llvm::Value* index = emitExpr((*arrAcc)->indices[0]);
                if (!base) { return nullptr; }
                llvm::Type* arrayTy = llvmTypeFor(ptrTy);
                if (!arrayTy->isArrayTy()) {
                    if (fallback) return emitExpr(node);
                    cg_error(get_pos((*arrAcc)->base), "invalid array type for indexing", "QC-T062");
                    return nullptr;
                }
                return builder->CreateGEP(arrayTy, base, {builder->getInt32(0), index}, "lval_arr_addr");
            }
            if (genericiseOrFindClass(ptrTy)) {
                llvm::Value* obj = emitLValue((*arrAcc)->base, true);
                llvm::Value* idx = emitExpr((*arrAcc)->indices[0]);
                return emitVirtualOrDirectCall(ptrTy, "operator[]", obj, {idx});
            }
            if (fallback) return emitExpr(node);
        }
        if (fallback) return emitExpr(node);
        return nullptr;
    }
    std::string getElementType(std::string fullType) {
        size_t start = fullType.find("<");
        size_t end = fullType.rfind(">");
        if (start != std::string::npos && end != std::string::npos) { return fullType.substr(start + 1, end - start - 1); }
        return "";
    }
    std::pair<std::string, std::string> splitMapTypes(const std::string& t) {
        size_t start = t.find("<");
        size_t end = t.rfind(">");
        std::string inner = t.substr(start + 1, end - start - 1);

        int depth = 0;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '<') depth++;
            if (inner[i] == '>') depth--;
            if (inner[i] == ',' && depth == 0) { return {inner.substr(0, i), inner.substr(i + 1)}; }
        }
        return {inner, ""};
    }
    llvm::Value* emitPrimitiveConversion(llvm::Value* arg, const std::string& target, Position pos = Position()) {
        if (!arg) return nullptr;

        llvm::Type* ty = arg->getType();
        std::string fnName;
        llvm::Type* retTy = nullptr;

        if (target == "int") {
            retTy = builder->getInt32Ty();
            if (ty->isPointerTy())
                fnName = "qc_to_int_from_string";
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToSI(arg, retTy);
            else if (ty->isIntegerTy(64))
                return builder->CreateTrunc(arg, retTy);
            else if (ty->isIntegerTy(32))
                return arg;
            else if (ty->isIntegerTy())
                return builder->CreateSExt(arg, retTy);
        } else if (target == "short int") {
            retTy = builder->getInt16Ty();
            if (ty->isPointerTy())
                fnName = "qc_to_short_int_from_string";
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToSI(arg, retTy);
            else if (ty->isIntegerTy(64) || ty->isIntegerTy(32))
                return builder->CreateTrunc(arg, retTy);
            else if (ty->isIntegerTy(16))
                return arg;
            else if (ty->isIntegerTy())
                return builder->CreateSExt(arg, retTy);
        } else if (target == "long int") {
            retTy = builder->getIntNTy(getPtrSize());
            if (ty->isPointerTy())
                fnName = "qc_to_int_from_string";
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToSI(arg, retTy);
            else if (ty == retTy)
                return arg;
            else if (ty->isIntegerTy())
                return builder->CreateSExt(arg, retTy);
        } else if (target == "float") {
            retTy = builder->getFloatTy();
            if (ty->isPointerTy())
                fnName = "qc_to_float_from_string";
            else if (ty->isIntegerTy())
                return builder->CreateSIToFP(arg, retTy);
            else if (ty->isDoubleTy())
                return builder->CreateFPTrunc(arg, retTy);
            else if (ty->isFloatTy())
                return arg;
        } else if (target == "double") {
            retTy = builder->getDoubleTy();
            if (ty->isPointerTy())
                fnName = "qc_to_double_from_string";
            else if (ty->isIntegerTy())
                return builder->CreateSIToFP(arg, retTy);
            else if (ty->isFloatTy())
                return builder->CreateFPExt(arg, retTy);
            else if (ty->isDoubleTy())
                return arg;
        } else if (target == "bool") {
            retTy = builder->getInt1Ty();
            if (ty->isIntegerTy(1))
                return arg;
            else if (ty->isIntegerTy())
                return builder->CreateICmpNE(arg, llvm::ConstantInt::get(ty, 0));
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFCmpONE(arg, llvm::ConstantFP::get(ty, 0.0));
            else if (ty->isPointerTy())
                fnName = "qc_to_bool_from_string";
        } else if (target == "char") {
            retTy = builder->getInt8Ty();
            if (ty->isPointerTy())
                fnName = "qc_to_char_from_string";
            else if (ty->isIntegerTy(8))
                return arg;
            else if (ty->isIntegerTy())
                return builder->CreateSExtOrTrunc(arg, retTy);
        } else if (target == "addr_t") {
            retTy = builder->getIntNTy(getPtrSize());
            if (ty->isPointerTy())
                fnName = "qc_to_addr_t_from_string";
            else if (ty->isIntegerTy())
                return builder->CreateZExtOrTrunc(arg, retTy);
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToUI(arg, retTy);
        } else if (target == "byte") {
            retTy = builder->getInt8Ty();
            if (ty->isPointerTy())
                fnName = "qc_to_byte_from_string";
            else if (ty->isIntegerTy())
                return builder->CreateZExtOrTrunc(arg, retTy);
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToUI(arg, retTy);
        } else if (target == "nibble") {
            retTy = builder->getIntNTy(4);
            if (ty->isPointerTy())
                fnName = "qc_to_nibble_from_string";
            else if (ty->isIntegerTy())
                return builder->CreateZExtOrTrunc(arg, retTy);
            else if (ty->isFloatTy() || ty->isDoubleTy())
                return builder->CreateFPToUI(arg, retTy);
        }

        if (fnName.empty() || !retTy) {
            cg_error(pos, "Cannot convert to " + target, "QC-S303");
            return nullptr;
        }

        llvm::Function* fn = module->getFunction(fnName);
        if (!fn) {
            llvm::FunctionType* fty = llvm::FunctionType::get(retTy, {ty}, false);
            fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fnName, module);
        }

        return builder->CreateCall(fn, {arg}, "to_" + target);
    }
    std::string getFieldType(const std::string& className, const std::string& fieldName) {
        auto it = userTypes.find(className);
        if (it == userTypes.end()) return "";
        auto& info = it->second;
        for (auto& f : info.classFields) {
            if (f.name == fieldName) return f.type;
        }
        if (!info.baseClassName.empty()) { return getFieldType(info.baseClassName, fieldName); }

        return "";
    }
    llvm::Value* emitBuiltinConversion(llvm::Value* rawArg, const std::string& target, Position pos = Position()) {
        if (!rawArg) return nullptr;

        std::string unionName;
        if (isUnionType(rawArg->getType(), &unionName)) {
            llvm::Value* tag = builder->CreateExtractValue(rawArg, 0, "conv_tag");
            llvm::Value* payload = builder->CreateExtractValue(rawArg, 1, "conv_payload");

            auto& members = userTypes[unionName].members;
            llvm::Type* resultTy = nullptr;

            if (target == "int")
                resultTy = builder->getInt32Ty();
            else if (target == "float")
                resultTy = builder->getFloatTy();
            else if (target == "double")
                resultTy = builder->getDoubleTy();
            else if (target == "bool")
                resultTy = builder->getInt1Ty();
            else if (target == "char")
                resultTy = builder->getInt8Ty();
            else if (target == "addr_t" || target == "long int")
                resultTy = builder->getIntNTy(getPtrSize());
            else if (target == "short int")
                resultTy = builder->getInt16Ty();
            else if (target == "nibble")
                resultTy = builder->getIntNTy(4);
            else if (target == "byte")
                resultTy = builder->getInt8Ty();
            else if (target == "qbool")
                resultTy = builder->getIntNTy(2);
            else {
                cg_error(pos, "Unknown conversion target: " + target, "QC-S304");
                return nullptr;
            }

            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "conv_union_end", currentFunction);
            llvm::BasicBlock* failBB = llvm::BasicBlock::Create(context, "conv_union_fail", currentFunction);
            llvm::SwitchInst* sw = builder->CreateSwitch(tag, failBB, members.size());

            std::vector<std::pair<llvm::BasicBlock*, llvm::Value*>> incoming;

            for (size_t i = 0; i < members.size(); i++) {
                llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "conv_union_case_" + std::to_string(i), currentFunction);
                sw->addCase(builder->getInt32(i), caseBB);
                builder->SetInsertPoint(caseBB);

                std::string typeStr = members[i].type;
                size_t colonPos = typeStr.find(':');
                if (colonPos != std::string::npos) { typeStr = typeStr.substr(0, colonPos); }

                llvm::Type* memberTy = llvmTypeFor(typeStr);
                llvm::Value* typedPtr = builder->CreateBitCast(payload, llvm::PointerType::get(context, 0));
                llvm::Value* loaded = builder->CreateLoad(memberTy, typedPtr, "conv_loaded");

                llvm::Value* converted = emitPrimitiveConversion(loaded, target, pos);
                if (!converted) return nullptr;

                incoming.push_back({builder->GetInsertBlock(), converted});
                builder->CreateBr(endBB);
            }

            builder->SetInsertPoint(failBB);
            builder->CreateUnreachable();

            builder->SetInsertPoint(endBB);
            llvm::PHINode* phi = builder->CreatePHI(resultTy, incoming.size(), "conv_union_phi");
            for (auto& [bb, val] : incoming) { phi->addIncoming(val, bb); }
            return phi;
        }
        return emitPrimitiveConversion(rawArg, target, pos);
    }
    llvm::Value* adaptArgumentForParam(llvm::Value* v, AnyNode& argNode, llvm::Type* paramTy, size_t argIndex) {
        if (!v) return nullptr;

        llvm::Type* srcTy = v->getType();
        if (srcTy->isPointerTy()) {
            if (getExpressionType(argNode, false).ends_with("&")) { v = builder->CreateLoad(paramTy, v, "strip_ref"); }
        }
        for (auto& [unionName, unionTy] : unionTypes) {
            if (srcTy == unionTy && !isUnionType(paramTy)) {
                llvm::Value* dataPtr = builder->CreateExtractValue(v, 1, "union_data");
                if (paramTy->isPointerTy()) {
                    v = builder->CreateBitCast(dataPtr, paramTy);
                } else {
                    llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                    v = builder->CreateLoad(paramTy, typedPtr);
                }
                srcTy = paramTy;
                break;
            }

            if (!isUnionType(srcTy) && paramTy == unionTy) {
                int tag = findUnionVariantTag(unionName, argNode, v);
                if (tag == -1) {
                    cg_error(get_pos(argNode), "argument doesn't match union variant for " + unionName + " parameter " + std::to_string(argIndex),
                             "QC-S305");
                    return nullptr;
                }

                llvm::Value* unionVal = llvm::UndefValue::get(unionTy);
                unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);

                llvm::Value* dataPtr = storeAndGetPointer(v);
                unionVal = builder->CreateInsertValue(unionVal, dataPtr, 1);

                v = unionVal;
                srcTy = paramTy;
                break;
            }
        }
        if (v->getType()->isArrayTy() && paramTy->isPointerTy()) {
            v = decayArrayToPointer(v);
            if (!v) return nullptr;
        }
        if (isEnumType(srcTy) && paramTy->isIntegerTy()) {
            v = builder->CreateExtractValue(v, {0});
            srcTy = v->getType();
        }
        if (srcTy->isIntegerTy() && isEnumType(paramTy)) {
            llvm::Value *structVal = llvm::ConstantAggregateZero::get(llvm::cast<llvm::StructType>(paramTy));
            v = builder->CreateInsertValue(structVal, v, {0});
        }
        if (srcTy->isIntegerTy() && paramTy->isIntegerTy()) {
            unsigned srcBits = srcTy->getIntegerBitWidth();
            unsigned dstBits = paramTy->getIntegerBitWidth();
            if (srcBits < dstBits) {
                v = builder->CreateSExt(v, paramTy, "arg_sext");
            } else if (srcBits > dstBits) {
                v = builder->CreateTrunc(v, paramTy, "arg_trunc");
            }
            return v;
        }
        if (srcTy->isFloatTy() && paramTy->isDoubleTy()) { return builder->CreateFPExt(v, paramTy, "arg_fpext"); }
        if (srcTy->isDoubleTy() && paramTy->isFloatTy()) { return builder->CreateFPTrunc(v, paramTy, "arg_fptrunc"); }
        if (srcTy->isIntegerTy() && paramTy->isFloatingPointTy()) { return builder->CreateSIToFP(v, paramTy, "arg_sitofp"); }
        if (srcTy->isFloatingPointTy() && paramTy->isIntegerTy()) { return builder->CreateFPToSI(v, paramTy, "arg_fptosi"); }
        return v;
    }
    std::vector<llvm::Value*> emitAdaptedArgs(const std::list<AnyNode>& argNodes, llvm::FunctionType* fnTy,
                                              const std::vector<std::string>& paramTypeStrings) {
        std::vector<llvm::Value*> args;
        size_t i = 0;
        for (auto it = argNodes.begin(); it != argNodes.end(); ++it, ++i) {
            AnyNode& argNode = const_cast<AnyNode&>(*it);
            llvm::Value* v = nullptr;
            if (i < paramTypeStrings.size() && paramTypeStrings[i].ends_with("&")) {
                v = emitLValue(argNode);
                if (!v) {
                    cg_error(get_pos(argNode), "L-value required for reference parameter", "QC-S307");
                    return {};
                }
            } else {
                v = emitExpr(argNode);
            }
            if (!v) {
                cg_error(get_pos(argNode), "Failed to emit argument", "QC-S308");
                return {};
            }
            if (i < fnTy->getNumParams()) {
                llvm::Type* paramTy = fnTy->getParamType(i);
                v = adaptArgumentForParam(v, argNode, paramTy, i);
                if (!v) return {};
            } else {
            }
            if (v->getType()->isArrayTy() && i < fnTy->getNumParams() && fnTy->getParamType(i)->isPointerTy()) { v = decayArrayToPointer(v); }
            args.push_back(v);
        }
        return args;
    }
    std::pair<llvm::Function*, std::string> findMethodInHierarchy(const std::string& className, const std::string& methodName) {
        std::string resolvedClassName = className;
        if (className.find("::") == std::string::npos && !getCurrentNamespace().empty()) {
            std::string qualifiedName = getCurrentNamespace() + "::" + className;
            if (classMethods.find(qualifiedName) != classMethods.end()) { resolvedClassName = qualifiedName; }
        }

        std::string currentClass = resolvedClassName;

        while (!currentClass.empty()) {
            auto classIt = classMethods.find(currentClass);
            if (classIt != classMethods.end()) {
                auto methodIt = classIt->second.find(methodName);
                if (methodIt != classIt->second.end()) {
                    if (!methodIt->second.empty()) { return {methodIt->second[0], currentClass}; }
                }
            }

            auto typeIt = userTypes.find(currentClass);
            if (typeIt != userTypes.end()) {
                std::string baseClass = typeIt->second.baseClassName;
                if (!baseClass.empty() && baseClass.find("::") == std::string::npos) {
                    size_t lastColon = currentClass.rfind("::");
                    if (lastColon != std::string::npos) {
                        std::string ns = currentClass.substr(0, lastColon);
                        std::string qualifiedBase = ns + "::" + baseClass;
                        if (userTypes.find(qualifiedBase) != userTypes.end()) { baseClass = qualifiedBase; }
                    }
                }
                currentClass = baseClass;
            } else {
                break;
            }
        }

        return {nullptr, ""};
    }
    int getFlattenedFieldIndex(const std::string& className, const std::string& fieldName) {
        int index = 0;
        std::function<bool(const std::string&)> searchFields = [&](const std::string& currentClass) -> bool {
            auto& classInfo = userTypes[currentClass];
            if (!classInfo.baseClassName.empty()) {
                std::string baseClass = classInfo.baseClassName;
                if (baseClass.find("::") == std::string::npos) {
                    size_t pos = currentClass.rfind("::");
                    if (pos != std::string::npos) {
                        std::string ns = currentClass.substr(0, pos);
                        std::string qualified = ns + "::" + baseClass;
                        if (userTypes.find(qualified) != userTypes.end()) { baseClass = qualified; }
                    }
                }
                if (searchFields(baseTypeName(baseClass))) { return true; }
            }
            for (auto& field : classInfo.classFields) {
                if (field.name == "__vptr") { continue; }
                if (field.name == fieldName) { return true; }
                index++;
            }
            return false;
        };
        std::string resolvedClass = className;
        if (className.find("::") == std::string::npos && !getCurrentNamespace().empty()) {
            std::string qualified = getCurrentNamespace() + "::" + className;
            if (userTypes.find(qualified) != userTypes.end()) { resolvedClass = qualified; }
        }
        index = 1;
        if (searchFields(resolvedClass)) { return index; }
        return -1;
    }
    bool canAccessMethod(const std::string& callerClass, const std::string& methodClass, const std::string& methodName) {
        auto& classInfo = userTypes[methodClass];
        for (auto& method : classInfo.classMethods) {
            if (method.name_tok.value == methodName) {
                std::string access = method.access;

                if (access == "public") return true;
                if (access == "private") return callerClass == methodClass;
                if (access == "protected") {
                    if (callerClass == methodClass) return true;
                    std::string current = callerClass;
                    while (!current.empty()) {
                        if (current == methodClass) return true;
                        current = userTypes[current].baseClassName;
                    }
                    return false;
                }
            }
        }

        return true;
    }
    std::pair<std::string, std::string> getFieldOwner(const std::string& className, const std::string& fieldName) {
        std::string currentClass = className;

        while (!currentClass.empty()) {
            auto it = userTypes.find(baseTypeName(currentClass));
            if (it == userTypes.end()) { return {"", "public"}; }

            auto& classInfo = it->second;
            for (auto& field : classInfo.classFields) {
                if (field.name == fieldName) { return {currentClass, field.access}; }
            }
            currentClass = classInfo.baseClassName;
        }

        return {"", "public"};
    }
    bool canAccessField(const std::string& callerClass, const std::string& fieldOwnerClass, const std::string& access) {
        auto userTypeIt = userTypes.find(resolveTypeName(fieldOwnerClass));
        UserTypeInfo usertype;
        if (userTypeIt != userTypes.end()) { usertype = userTypeIt->second; }
        bool isFriend = usertype.kind == UserTypeKind::Class && std::ranges::any_of(usertype.friendClasses, [&](const std::string& friendName) {
                            return resolveTypeName(friendName, false) == resolveTypeName(callerClass, false);
                        });
        bool isFriendly = usertype.kind == UserTypeKind::Class && std::ranges::any_of(usertype.friendlyClasses, [&](const std::string& friendName) {
                              return resolveTypeName(friendName, false) == resolveTypeName(callerClass, false);
                          });
        if (access == "public") return true;
        if (access == "private") { return callerClass == fieldOwnerClass || isFriend; }
        if (access == "protected") {
            if (callerClass == fieldOwnerClass || isFriend || isFriendly) return true;
            std::string current = callerClass;
            while (!current.empty()) {
                if (current == fieldOwnerClass) return true;
                current = userTypes[current].baseClassName;
            }
            return false;
        }
        return true;
    }
    std::vector<std::string> namespaceStack;
    std::string getCurrentNamespace() {
        if (namespaceStack.empty()) return "";

        std::string result = namespaceStack[0];
        for (size_t i = 1; i < namespaceStack.size(); i++) { result += "::" + namespaceStack[i]; }
        return result;
    }
    std::vector<std::string> getAccessibleNamespaces() {
        std::vector<std::string> accessible;
        accessible.push_back("");
        std::string current = "";
        for (auto& ns : namespaceStack) {
            if (!current.empty()) current += "::";
            current += ns;
            accessible.push_back(current);
        }

        return accessible;
    }
    MethodCallNode* methodCallFromCall(CallNode* call, std::string name) {
        return new MethodCallNode(call->node_to_call, Token(TokenType::IDENTIFIER, name, get_pos(call)),
                                  std::vector<AnyNode>(call->arg_nodes.begin(), call->arg_nodes.end()));
    }
    FuncDefNode* funcDefFromClassMethod(const ClassMethodInfo& method, const std::string& className, std::string delim = "::") {
        std::string mangledName = className + delim + method.name_tok.value;
        Token nameTok(TokenType::IDENTIFIER, mangledName, method.name_tok.pos);
        return new FuncDefNode(method.return_types, nameTok, std::list(method.params.begin(), method.params.end()), method.body, "", false, false,
                               method.generics, false, isHeader, method.modifiers);
    }
    std::unordered_map<std::string, std::unordered_map<std::string, llvm::Function*>> genericisedMethods;
    llvm::Value* tryHandleSpecialized(const std::string& className, const std::string& methodName, MethodCallNode* node, llvm::Value* thisPtr) {
        if (auto methodIt = std::find_if(
                userTypes[baseTypeName(className)].classMethods.begin(), userTypes[baseTypeName(className)].classMethods.end(),
                [&](ClassMethodInfo method) { return method.name_tok.value == baseTypeName(methodName) && !method.generics.empty(); });
            methodIt != userTypes[baseTypeName(className)].classMethods.end()) {
            auto genericParams = genericParamsFromName(className);
            auto oldGenericTypes = this->currentGenericTypes;
            auto oldGenericTypeStrings = currentGenericTypeStrings;
            auto oldNonTypeGenerics = currentNonTypeGenericValues;
            if (!genericParams.empty()) {
                auto classInfo = userTypes[baseTypeName(className)];
                if (!fulfillsGenericConstraints(classInfo.generics, genericParams, classInfo.pos)) {
                    this->currentGenericTypes = oldGenericTypes;
                    currentGenericTypeStrings = oldGenericTypeStrings;
                    currentNonTypeGenericValues = oldNonTypeGenerics;
                    return nullptr;
                }
            }
            size_t methodIdx = std::distance(userTypes[baseTypeName(className)].classMethods.begin(), methodIt);
            if (genericisedMethods[fixMangling(className)][fixMangling(methodName)] == nullptr) {
                genericisedMethods[fixMangling(className)][fixMangling(methodName)] = generateSpecializedMethod(fixMangling(className), methodIdx,
                                                                                                                fixMangling(methodName));
            }
            llvm::Function* fn = genericisedMethods[fixMangling(className)][fixMangling(methodName)];
            auto& info = userTypes[baseTypeName(className)].classMethods[methodIdx];
            auto args = prepareArgs(&info, node->args);
            bool isVariadic = !info.params.empty() && info.params.back().type.value == "...";
            if (isVariadic) {
                size_t numFixedParams = info.params.size() - 1;
                std::vector<llvm::Value*> varVals;
                if (args.size() > numFixedParams) {
                    varVals.assign(args.begin() + numFixedParams, args.end());
                    args.resize(numFixedParams);
                }
                args.push_back(packVariadicArgs(varVals));
            }
            this->currentGenericTypes = oldGenericTypes;
            currentGenericTypeStrings = oldGenericTypeStrings;
            currentNonTypeGenericValues = oldNonTypeGenerics;
            return emitMethodCall(fn, thisPtr, args, methodName);
        }
        return nullptr;
    }
    void addTypeNotes(const std::string& typeName, Position pos, std::optional<UserTypeKind> search_type = std::nullopt) {
        std::vector<std::string> types;
        std::string current = getCurrentNamespace();
        while (true) {
            std::string prefix = current.empty() ? "" : current + "::";
            for (auto& [name, info] : userTypes) {
                if (search_type && info.kind != *search_type) continue;
                if (!name.starts_with(prefix)) continue;
                std::string relative = name.substr(prefix.size());
                types.push_back(relative);
            }
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        std::vector<std::pair<int, std::string>> matches;
        if (typeName.size() >= 3) {
            for (auto& tname : types) {
                int distance = levenshteinDistance(typeName, tname);
                if (distance <= 2) { matches.push_back({distance, tname}); }
            }
        }
        std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (!matches.empty()) {
            std::string note = "did you mean ";
            size_t count = std::min<size_t>(3, matches.size());
            for (size_t i = 0; i < count; i++) {
                if (i != 0) note += ", ";
                note += "`" + matches[i].second + "`";
            }
            note += "?";
            cg_note(pos, note);
        }
    }
    void addMethodNotes(const std::string& className, const std::string& methodName, const std::vector<std::string>& args, Position pos) {
        auto& methods = userTypes[baseTypeName(className)].classMethods;
        struct Candidate {
            int score;
            ClassMethodInfo* method;
        };
        std::vector<Candidate> candidates;
        for (auto& method : methods) {
            if (method.name_tok.value != methodName) continue;
            int score = 0;
            size_t argCount = args.size();
            size_t paramCount = method.params.size();
            score -= std::abs((int)argCount - (int)paramCount) * 5;
            size_t count = std::min(argCount, paramCount);
            for (size_t i = 0; i < count; i++) {
                llvm::Type* argTy = llvmTypeFor(args[i]);
                llvm::Type* paramTy = llvmTypeFor(method.params[i].type.value);
                if (argTy == paramTy) {
                    score += 3;
                } else if ((argTy->isIntegerTy() || argTy->isFloatTy() || argTy->isDoubleTy()) &&
                           (paramTy->isIntegerTy() || paramTy->isFloatTy() || paramTy->isDoubleTy())) {
                    score += 1;
                } else if (argTy->isPointerTy() && paramTy->isPointerTy()) {
                    score += 1;
                } else {
                    score -= 3;
                }
            }
            candidates.push_back({score, &method});
        }
        if (candidates.empty()) return;
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        if (candidates[0].score > 0) { cg_note(pos, "closest matching overload: " + candidates[0].method->print()); }
        if (candidates.size() <= 5) {
            std::string note = "available overloads:";
            for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
            cg_note(pos, note);
        } else {
            std::string note = "other overloads";
            size_t shown = 0;
            for (auto& candidate : candidates) {
                if (shown >= 3) break;
                note += "\n  - " + candidate.method->print();
                shown++;
            }
            cg_note(pos, note);
        }
    }
    void addMethodNotes(const std::string& className, const std::string& methodName, const std::vector<llvm::Value*>& args, Position pos) {
        auto& methods = userTypes[baseTypeName(className)].classMethods;
        struct Candidate {
            int score;
            ClassMethodInfo* method;
        };
        std::vector<Candidate> candidates;
        for (auto& method : methods) {
            if (method.name_tok.value != methodName) continue;
            int score = 0;
            size_t argCount = args.size();
            size_t paramCount = method.params.size();
            score -= std::abs((int)argCount - (int)paramCount) * 5;
            size_t count = std::min(argCount, paramCount);
            for (size_t i = 0; i < count; i++) {
                llvm::Type* argTy = args[i]->getType();
                llvm::Type* paramTy = llvmTypeFor(method.params[i].type.value);
                if (argTy == paramTy) {
                    score += 3;
                } else if ((argTy->isIntegerTy() || argTy->isFloatTy() || argTy->isDoubleTy()) &&
                           (paramTy->isIntegerTy() || paramTy->isFloatTy() || paramTy->isDoubleTy())) {
                    score += 1;
                } else if (argTy->isPointerTy() && paramTy->isPointerTy()) {
                    score += 1;
                } else {
                    score -= 3;
                }
            }
            candidates.push_back({score, &method});
        }
        if (candidates.empty()) return;
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        if (candidates[0].score > 0) { cg_note(pos, "closest matching overload: " + candidates[0].method->print()); }
        if (candidates.size() <= 5) {
            std::string note = "available overloads:";
            for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
            cg_note(pos, note);
        } else {
            std::string note = "other overloads";
            size_t shown = 0;
            for (auto& candidate : candidates) {
                if (shown >= 3) break;
                note += "\n  - " + candidate.method->print();
                shown++;
            }
            cg_note(pos, note);
        }
    }
    void addConstructorNotes(const std::string& className, const std::vector<llvm::Value*>& args, Position pos) {
        auto& methods = userTypes[baseTypeName(className)].classMethods;
        struct Candidate {
            int score;
            ClassMethodInfo* method;
        };
        std::vector<Candidate> candidates;
        for (auto& method : methods) {
            if (!method.is_constructor) continue;
            int score = 0;
            size_t argCount = args.size();
            size_t paramCount = method.params.size();
            score -= std::abs((int)argCount - (int)paramCount) * 5;
            size_t count = std::min(argCount, paramCount);
            for (size_t i = 0; i < count; i++) {
                llvm::Type* argTy = args[i]->getType();
                llvm::Type* paramTy = llvmTypeFor(method.params[i].type.value);
                if (argTy == paramTy) {
                    score += 3;
                } else if ((argTy->isIntegerTy() || argTy->isFloatTy() || argTy->isDoubleTy()) &&
                           (paramTy->isIntegerTy() || paramTy->isFloatTy() || paramTy->isDoubleTy())) {
                    score += 1;
                } else if (argTy->isPointerTy() && paramTy->isPointerTy()) {
                    score += 1;
                } else {
                    score -= 3;
                }
            }
            candidates.push_back({score, &method});
        }
        if (candidates.empty()) return;
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        if (candidates[0].score > 0) { cg_note(pos, "closest matching constructor: " + candidates[0].method->print()); }
        if (candidates.size() <= 5) {
            std::string note = "available constructors:";
            for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
            cg_note(pos, note);
        } else {
            std::string note = "other constructors:";
            size_t shown = 0;
            for (auto& candidate : candidates) {
                if (shown >= 3) break;
                note += "\n  - " + candidate.method->print();
                shown++;
            }
            cg_note(pos, note);
        }
    }
    std::string fixMangling(std::string type) { return buildMangledName(baseTypeName(type), genericParamsFromName(type), true); }
    std::vector<llvm::Value*> reconcileArgs(llvm::Function* func, llvm::Value* thisPtr, const std::vector<llvm::Value*>& args) {
        std::vector<llvm::Value*> reconciled = {thisPtr};
        auto* funcTy = func->getFunctionType();
        for (size_t i = 0; i < args.size(); i++) {
            llvm::Type* expected = funcTy->getParamType(i + 1);
            llvm::Value* actual = args[i];
            if (actual->getType()->isPointerTy() && expected->isStructTy()) {
                actual = builder->CreateLoad(expected, actual);
                reconciled.push_back(actual);
            } else if (actual->getType() == expected) {
                reconciled.push_back(actual);
            } else if (!expected->isPointerTy() && actual->getType()->isPointerTy()) {
                reconciled.push_back(builder->CreateLoad(expected, actual));
            } else if (expected->isIntegerTy() && actual->getType()->isIntegerTy()) {
                unsigned expBits = expected->getIntegerBitWidth();
                unsigned actBits = actual->getType()->getIntegerBitWidth();
                if (actBits > expBits) {
                    reconciled.push_back(builder->CreateTrunc(actual, expected));
                } else if (actBits < expBits) {
                    reconciled.push_back(builder->CreateSExt(actual, expected));
                } else {
                    reconciled.push_back(actual);
                }
            } else {
                reconciled.push_back(builder->CreateBitCast(actual, expected));
            }
        }
        return reconciled;
    }
    std::vector<llvm::Value*> prepareArgs(ClassMethodInfo* info, std::vector<AnyNode>& argNodes) {
        std::vector<llvm::Value*> args;
        size_t explicitArgCount = argNodes.size();
        for (size_t i = 0; i < explicitArgCount; ++i) {
            llvm::Type* paramTy = nullptr;
            bool isRef = false;
            if (info && i < info->params.size()) {
                const std::string& typeName = info->params[i].type.value;
                isRef = typeName.ends_with("&");
                paramTy = llvmTypeFor(typeName);
            }
            llvm::Value* value = nullptr;
            if (isRef) {
                value = emitLValue(argNodes[i]);
            } else {
                value = emitExpr(argNodes[i]);
            }
            if (!value) {
                cg_error(get_pos(argNodes[i]), "failed to emit method argument", "QC-S309");
                return {};
            }
            if (paramTy) {
                value = adaptArgumentForParam(value, argNodes[i], paramTy, i);
                if (!value) return {};
            }
            args.push_back(value);
        }
        if (info && explicitArgCount < info->params.size()) {
            for (size_t i = explicitArgCount; i < info->params.size(); ++i) {
                const auto& param = info->params[i];
                if (param.type.value == "...") { break; }
                if (!param.default_value.has_value()) {
                    cg_error(info->name_tok.pos, "missing required argument for parameter '" + param.name.value + "'", "QC-S169");
                    return {};
                }
                AnyNode& defaultRef = const_cast<AnyNode&>(param.default_value.value());
                llvm::Value* defVal = emitExpr(defaultRef);
                if (!defVal) {
                    cg_error(info->name_tok.pos, "failed to evaluate default parameter for '" + param.name.value + "'", "QC-S168");
                    return {};
                }
                llvm::Type* paramTy = llvmTypeFor(param.type.value);
                if (paramTy) {
                    defVal = adaptArgumentForParam(defVal, defaultRef, paramTy, i);
                    if (!defVal) return {};
                }
                args.push_back(defVal);
            }
        }
        return args;
    }
    llvm::Value* emitMethodCall(llvm::Function* method, llvm::Value* thisPtr, const std::vector<llvm::Value*>& args, const std::string& name);
    std::string resolveVirtualTargetClass(const std::string& declaredClass, const std::string& methodName, size_t argCount) {
        for (auto& m : userTypes[baseTypeName(declaredClass)].classMethods) {
            if (m.name_tok.value == methodName && m.params.size() == argCount) return declaredClass;
        }
        for (auto& [name, info] : userTypes) {
            if (info.baseClassName == baseTypeName(declaredClass)) {
                for (auto& m : info.classMethods) {
                    if (m.name_tok.value == methodName && m.params.size() == argCount) return name;
                }
            }
        }
        return declaredClass;
    }
    int implicitCastPenalty(llvm::Type* expected, llvm::Type* actual) {
        if (expected == actual) return 0;
        auto isCharLike = [](llvm::Type* t) -> bool {
            if (t->isPointerTy()) return true;
            if (t->isArrayTy() && t->getArrayElementType()->isIntegerTy(8)) return true;
            return false;
        };
        if (isCharLike(expected) && isCharLike(actual)) return 0;
        if (expected->isIntegerTy() && isEnumType(actual)) return 1;
        if (isEnumType(actual) && expected->isIntegerTy()) return 0;
        if (expected->isIntegerTy() && actual->isIntegerTy()) {
            unsigned expBits = expected->getIntegerBitWidth();
            unsigned actBits = actual->getIntegerBitWidth();
            return expBits >= actBits ? 2 : 1;
        }
        return -1;
    }
    ClassMethodInfo* findMethodInfo(const std::string& className, const std::string& methodName, const std::vector<std::string>& argTypes) {
        std::string resolvedClassName = className;
        if (className.find("::") == std::string::npos && !getCurrentNamespace().empty()) {
            std::string qualifiedName = getCurrentNamespace() + "::" + className;
            if (userTypes.find(qualifiedName) != userTypes.end()) { resolvedClassName = qualifiedName; }
        }
        std::string currentClass = resolvedClassName;
        while (!currentClass.empty()) {
            std::string baseName = baseTypeName(currentClass);
            if (baseName.find('<') != std::string::npos) { baseName = baseName.substr(0, baseName.find('<')); }
            auto typeIt = userTypes.find(baseName);
            if (typeIt != userTypes.end()) {
                ClassMethodInfo* bestMatch = nullptr;
                int bestScore = 999999;
                for (auto& method : typeIt->second.classMethods) {
                    if (method.name_tok.value != methodName) continue;
                    size_t totalParams = method.params.size();
                    bool isVariadic = !method.params.empty() && method.params.back().type.value == "...";
                    size_t requiredParams = 0;
                    for (const auto& p : method.params) {
                        if (p.default_value.has_value() || p.type.value == "...") break;
                        requiredParams++;
                    }
                    size_t argCount = argTypes.size();
                    if (isVariadic) {
                        if (argCount < totalParams - 1) continue;
                    } else {
                        if (argCount < requiredParams || argCount > totalParams) continue;
                    }
                    int currentScore = 0;
                    bool compatible = true;
                    for (size_t i = 0; i < argCount; ++i) {
                        if (isVariadic && i >= totalParams - 1) {
                            currentScore += 2;
                            continue;
                        }
                        std::string expected = resolveTypeName(method.params[i].type.value, false);
                        std::string actual = resolveTypeName(argTypes[i], false);
                        if (expected == actual) {
                            currentScore += 0;
                        } else if (expected.ends_with("&") && expected.substr(0, expected.size() - 1) == actual) {
                            currentScore += 1;
                        } else if ((expected.ends_with("*") || expected.ends_with("[]")) &&
                                   (actual.ends_with("*") || actual.ends_with("[]") || actual == "nullptr")) {
                            currentScore += 2;
                        } else if (llvmTypeFor(expected) && llvmTypeFor(actual)) {
                            llvm::Type* expTy = llvmTypeFor(expected);
                            llvm::Type* actTy = llvmTypeFor(actual);
                            int castPenalty = implicitCastPenalty(expTy, actTy);
                            if (castPenalty >= 0) {
                                currentScore += 3 + castPenalty;
                            } else {
                                compatible = false;
                                break;
                            }
                        } else {
                            compatible = false;
                            break;
                        }
                    }
                    if (!compatible) continue;
                    if (argCount < totalParams && !isVariadic) { currentScore += (int)(totalParams - argCount) * 5; }
                    if (currentScore < bestScore) {
                        bestScore = currentScore;
                        bestMatch = &method;
                        if (bestScore == 0) return bestMatch;
                    }
                }
                if (bestMatch) return bestMatch;
                currentClass = typeIt->second.baseClassName;
                if (!currentClass.empty() && currentClass.find("::") == std::string::npos) {
                    size_t lastColon = resolvedClassName.rfind("::");
                    if (lastColon != std::string::npos) {
                        std::string ns = resolvedClassName.substr(0, lastColon);
                        std::string qualifiedBase = ns + "::" + currentClass;
                        if (userTypes.find(qualifiedBase) != userTypes.end()) { currentClass = qualifiedBase; }
                    }
                }
            } else {
                break;
            }
        }
        return nullptr;
    }
    llvm::Function* findMethodOverload(const std::string& className, const std::string& methodName, const std::vector<llvm::Value*>& args) {
        std::string resolvedClassName = className;
        if (className.find("::") == std::string::npos && !getCurrentNamespace().empty()) {
            std::string qualifiedName = getCurrentNamespace() + "::" + className;
            if (classMethods.find(qualifiedName) != classMethods.end()) { resolvedClassName = qualifiedName; }
        }
        std::string currentClass = resolvedClassName;
        while (!currentClass.empty()) {
            auto classIt = classMethods.find(currentClass);
            if (classIt != classMethods.end()) {
                auto methodIt = classIt->second.find(methodName);
                if (methodIt != classIt->second.end()) {
                    llvm::Function* bestMatch = nullptr;
                    int bestScore = 999;
                    for (auto* fn : methodIt->second) {
                        std::string lastargtype = "";
                        auto utIt = userTypes.find(baseTypeName(currentClass));
                        if (utIt == userTypes.end()) {
                            std::string baseName = currentClass.find('<') != std::string::npos ? currentClass.substr(0, currentClass.find('<'))
                                                                                               : currentClass;
                            utIt = userTypes.find(baseTypeName(baseName));
                        }
                        if (utIt == userTypes.end()) break;
                        for (const auto& method : utIt->second.classMethods) {
                            if (method.name_tok.value == methodName && method.params.size() == (fn->arg_size() - 1)) {
                                if (!method.params.empty()) { lastargtype = method.params.back().type.value; }
                                break;
                            }
                        }
                        int currentScore = 0;
                        bool matches = true;
                        if (lastargtype == "...") {
                            currentScore++;
                            for (size_t i = 0; i < fn->arg_size() - 2; i++) {
                                llvm::Type* expected = fn->getFunctionType()->getParamType(i + 1);
                                llvm::Type* actual = args[i]->getType();
                                if (expected != actual) {
                                    if (expected->isPointerTy() || actual->isPointerTy()) {
                                        currentScore += 1;
                                    } else if (int cast = implicitCastPenalty(expected, actual); cast >= 0) {
                                        currentScore += cast;
                                    } else {
                                        matches = false;
                                        break;
                                    }
                                }
                            }
                            if (matches && currentScore < bestScore) {
                                bestMatch = fn;
                                bestScore = currentScore;
                                if (bestScore == 0) break;
                            }
                            continue;
                        }

                        if (fn->arg_size() - 1 != args.size())
                            continue;
                        else {
                            for (size_t i = 0; i < args.size(); i++) {

                                llvm::Type* expected = fn->getFunctionType()->getParamType(i + 1);
                                if (args[i] == nullptr) {
                                    cg_error(Position(Position::INVALID_FILE_ID, 0, 0, 0), "Failed to emit method arg", "QC-M001");
                                    return nullptr;
                                }
                                llvm::Type* actual = args[i]->getType();

                                if (expected != actual) {
                                    if (expected->isPointerTy() || actual->isPointerTy() || expected->isArrayTy() && actual->isPointerTy() ||
                                        expected->isPointerTy() && actual->isArrayTy()) {
                                        currentScore += 1;
                                    } else if (int cast = implicitCastPenalty(expected, actual); cast >= 0) {
                                        currentScore += cast;
                                    } else {
                                        matches = false;
                                        break;
                                    }
                                }
                            }

                            if (matches && currentScore < bestScore) {
                                bestMatch = fn;
                                bestScore = currentScore;
                                if (bestScore == 0) break;
                            }
                        }
                    }
                    return bestMatch;
                }
            }
            auto typeIt = userTypes.find(baseTypeName(currentClass));
            if (typeIt == userTypes.end()) {
                std::string baseName = currentClass.find('<') != std::string::npos ? currentClass.substr(0, currentClass.find('<')) : currentClass;
                typeIt = userTypes.find(baseTypeName(baseName));
            }

            if (typeIt != userTypes.end()) {
                currentClass = typeIt->second.baseClassName;
                if (!currentClass.empty() && currentClass.find("::") == std::string::npos) {
                    size_t lastColon = resolvedClassName.rfind("::");
                    if (lastColon != std::string::npos) {
                        std::string ns = resolvedClassName.substr(0, lastColon);
                        std::string qualifiedBase = ns + "::" + currentClass;
                        if (userTypes.find(qualifiedBase) != userTypes.end()) { currentClass = qualifiedBase; }
                    }
                }
            } else {
                break;
            }
        }

        return nullptr;
    }
    llvm::Value* decayArrayToPointer(llvm::Value* v) {
        if (!v) return nullptr;
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(v)) {
            if (auto* arrayTy = llvm::dyn_cast<llvm::ArrayType>(load->getType())) {
                llvm::Value* address = load->getPointerOperand();
                llvm::Value* result = builder->CreateInBoundsGEP(arrayTy, address, {builder->getInt32(0), builder->getInt32(0)}, "decayptr");
                if (load->use_empty()) load->eraseFromParent();

                return result;
            }
        }
        if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(v)) {
            auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(global->getValueType());
            if (arrTy) { return builder->CreateInBoundsGEP(arrTy, global, {builder->getInt32(0), builder->getInt32(0)}, "decayptr"); }
        }
        if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(v)) {
            if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(alloca->getAllocatedType())) {
                return builder->CreateInBoundsGEP(arrTy, alloca, {builder->getInt32(0), builder->getInt32(0)}, "decayptr");
            }
        }
        if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(v->getType())) {
            llvm::AllocaInst* tmp = builder->CreateAlloca(arrTy);
            builder->CreateStore(v, tmp);
            return builder->CreateInBoundsGEP(arrTy, tmp, {builder->getInt32(0), builder->getInt32(0)});
        }
        if (v->getType()->isPointerTy()) return v;
        return nullptr;
    }
    llvm::Value* emitVirtualOrDirectCall(const std::string& ty, const std::string& methodName, llvm::Value* payload,
                                         const std::vector<llvm::Value*>& args) {
        llvm::Function* method = findMethodOverload(ty, methodName, args);
        if (!method) return nullptr;
        ClassMethodInfo* info = nullptr;
        std::string searchClass = baseTypeName(ty);
        while (!searchClass.empty() && !info) {
            for (auto& m : userTypes.at(baseTypeName(searchClass)).classMethods) {
                if (m.name_tok.value != methodName || m.params.size() != args.size()) continue;
                bool typesMatch = true;
                for (size_t i = 0; i < m.params.size(); i++) {
                    llvm::Type* declaredTy = llvmTypeFor(m.params[i].type.value);
                    llvm::Type* actualTy = method->getFunctionType()->getParamType(i + 1);
                    if (declaredTy != actualTy) {
                        typesMatch = false;
                        break;
                    }
                }
                if (typesMatch) {
                    info = &m;
                    break;
                }
            }
            searchClass = userTypes.at(baseTypeName(searchClass)).baseClassName;
        }
        auto vtableIt = vtables.find(ty);
        auto slotIt = vtableSlotIndex.find(ty);
        if (vtableIt != vtables.end() && slotIt != vtableSlotIndex.end()) {
            std::string mangledName = ty + "_" + methodName;
            if (info && classMethods[ty][methodName].size() > 1) {
                for (auto& param : info->params) { mangledName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
            }
            auto indexIt = slotIt->second.find(mangledName);
            if (indexIt != slotIt->second.end()) {
                int slotIndex = indexIt->second;
                llvm::StructType* classTy = genericiseOrFindClass(ty);
                llvm::Value* objPtr = payload;
                llvm::Value* vptrField;
                if (!objPtr->getType()->isPointerTy()) {
                    llvm::AllocaInst* tempSlot = createEntryAlloca("vcall_temp", objPtr->getType());
                    builder->CreateStore(objPtr, tempSlot);
                    objPtr = tempSlot;
                }
                vptrField = builder->CreateStructGEP(classTy, objPtr, 0, "vptr_field");
                llvm::Value* vptr = builder->CreateLoad(builder->getPtrTy(), vptrField, "vptr");
                llvm::Value* fnPtrAddr = builder->CreateGEP(builder->getPtrTy(), vptr, builder->getInt32(slotIndex), "vtable_slot");
                llvm::Value* fnPtr = builder->CreateLoad(builder->getPtrTy(), fnPtrAddr, "fn_ptr");
                std::vector<llvm::Value*> allArgs = reconcileArgs(method, objPtr, args);
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    llvm::InvokeInst* invoke = builder->CreateInvoke(method->getFunctionType(), fnPtr, contBB, currentLandingPad(), allArgs);
                    builder->SetInsertPoint(contBB);
                    return invoke;
                }
                return builder->CreateCall(method->getFunctionType(), fnPtr, allArgs);
            }
        }
        return emitMethodCall(method, payload, args, methodName);
    }
    uint64_t parseInteger(std::string s) {
        int base = 10;
        if (s.starts_with("0x") || s.starts_with("0X")) {
            base = 16;
        } else if (s.starts_with("0b") || s.starts_with("0B")) {
            base = 2;
            s = s.substr(2);
        } else if (s.size() > 1 && s[0] == '0') {
            base = 8;
        }
        return std::stoull(s, nullptr, base);
    }
    llvm::Function* generateSpecializedMethod(const std::string& className, size_t methodIdx, const std::string& specializedName) {
        llvm::Function* savedFunction = currentFunction;
        llvm::BasicBlock* savedBlock = builder->GetInsertBlock();
        auto savedGlobals = globals;
        auto savedThis = currentThis;
        auto savedClassName = currentClassName;
        auto savedNamespaceStack = namespaceStack;
        auto oldGenericTypes = this->currentGenericTypes;
        auto oldGenericTypeStrings = currentGenericTypeStrings;
        auto oldNonTypeGenerics = currentNonTypeGenericValues;
        auto genericParams = genericParamsFromName(specializedName);
        auto& method = userTypes[baseTypeName(className)].classMethods[methodIdx];
        if (!fulfillsGenericConstraints(method.generics, genericParams, method.name_tok.pos)) {
            this->currentGenericTypes = oldGenericTypes;
            currentGenericTypeStrings = oldGenericTypeStrings;
            currentNonTypeGenericValues = oldNonTypeGenerics;
            return nullptr;
        }
        namespaceStack.clear();
        size_t nsSep = specializedName.rfind("::");
        if (nsSep != std::string::npos) {
            namespaceStack = {specializedName.substr(0, nsSep)};
        } else if (!userTypes[baseTypeName(className)].namespace_path.empty()) {
            namespaceStack = {userTypes[baseTypeName(className)].namespace_path};
        }
        enterScope();
        std::vector<llvm::Type*> paramTypes;
        paramTypes.push_back(llvm::PointerType::get(context, 0));
        llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(method.return_types, method.params);
        for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
        llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
        llvm::Function* fn = module->getFunction(specializedName);
        if (!fn) fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, specializedName, module);
        size_t paramIdx = 0;
        if (method.is_volatile) {
            fn->addFnAttr(llvm::Attribute::NoInline);
            fn->addFnAttr(llvm::Attribute::OptimizeNone);
            fn->addFnAttr("noipa");
        }
        for (int i = 1; i < fnTy->getNumParams(); i++) {
            if (method.params[i - 1].type.value.starts_with("out ")) {
                fn->addParamAttr(i, llvm::Attribute::WriteOnly);
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            } else if (method.params[i - 1].type.value.starts_with("inout ")) {
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            }
            if (method.params[i - 1].type.value.ends_with("restrict")) { fn->addParamAttr(i, llvm::Attribute::NoAlias); }
        }
        llvm::Function* currentTarget = fn;
        if (!method.modifiers.empty()) {
            std::string implName = "_impl_" + specializedName;
            currentTarget = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, implName, module);
        }
        currentFunction = currentTarget;
        currentClassName = className;
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", currentTarget);
        builder->SetInsertPoint(entry);
        currentThis = currentTarget->getArg(0);
        varTypes["this"] = className + "*";
        volatileVars["this"] = false;
        for (size_t i = 0; i < method.params.size(); i++) {
            auto& param = method.params[i];
            llvm::Type* paramTy;
            std::string typeDescriptor;
            if (param.signature.has_value()) {
                paramTy = llvm::PointerType::get(context, 0);
                typeDescriptor = "fn";
            } else {
                typeDescriptor = resolveTypeName(param.type.value, false);
                paramTy = llvmTypeFor(typeDescriptor);
            }
            llvm::AllocaInst* alloc = createEntryAlloca(param.name.value, paramTy);
            builder->CreateStore(currentTarget->getArg(i + 1), alloc);
            locals[param.name.value] = alloc;
            varTypes[param.name.value] = typeDescriptor;
            volatileVars[param.name.value] = param.isVolatile;
        }
        if (method.body) {
            for (auto& stmt : method.body->statements) { emitStmt(stmt); }
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            if (baseFuncTy->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateRet(llvm::Constant::getNullValue(baseFuncTy->getReturnType()));
            }
        }
        exitScope();
        if (!method.modifiers.empty()) {
            for (int i = (int)method.modifiers.size() - 1; i >= 0; --i) {
                Token modTok = method.modifiers[i];
                if (!modifiers.count(modTok.value)) {
                    cg_error(modTok.pos, "unknown modifier '" + modTok.value + "'", "QC-S112");
                    continue;
                }
                ModifierInfo& modInfo = modifiers[modTok.value];
                bool isOutermost = (i == 0);
                std::string layerName = isOutermost ? specializedName : ("_mod_" + std::to_string(i) + "_" + specializedName);
                auto linkage = isOutermost ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
                std::string proceedName = "_proceed_" + std::to_string(i) + "_" + specializedName;
                llvm::Function* proceedFunc = synthesizeProceed(proceedName, currentTarget, modInfo.onReturn,
                                                                *funcDefFromClassMethod(method, className, "_"));
                llvm::Function* layerFunc = isOutermost ? fn : module->getFunction(layerName);
                if (!layerFunc) { layerFunc = llvm::Function::Create(fnTy, linkage, layerName, module); }
                auto savedDefers = defersStack;
                defersStack.clear();
                enterScope();
                llvm::BasicBlock* savedInsertBlock = builder->GetInsertBlock();
                auto* entryBB = llvm::BasicBlock::Create(context, "entry", layerFunc);
                builder->SetInsertPoint(entryBB);
                auto* prevFunction = currentFunction;
                auto prevThis = currentThis;
                currentFunction = layerFunc;
                currentThis = layerFunc->getArg(0);
                varTypes["this"] = className + "*";
                std::vector<llvm::Value*> forwardArgs;
                forwardArgs.push_back(currentThis);
                for (size_t pIdx = 0; pIdx < method.params.size(); pIdx++) {
                    auto& param = method.params[pIdx];
                    llvm::Value* argVal = layerFunc->getArg(pIdx + 1);
                    argVal->setName(param.name.value);
                    auto* alloca = createEntryAlloca(param.name.value, argVal->getType());
                    builder->CreateStore(argVal, alloca);
                    locals[param.name.value] = alloca;
                    varTypes[param.name.value] = param.signature.has_value() ? "fn" : resolveTypeName(param.type.value, false);
                    forwardArgs.push_back(argVal);
                }
                functions["proceed"] = proceedFunc;
                if (modInfo.onCall) {
                    for (auto& stmt : modInfo.onCall->statements) { emitStmt(stmt); }
                } else {
                    if (fnTy->getReturnType()->isVoidTy()) {
                        builder->CreateCall(proceedFunc, forwardArgs);
                        builder->CreateRetVoid();
                    } else {
                        llvm::Value* retVal = builder->CreateCall(proceedFunc, forwardArgs);
                        builder->CreateRet(retVal);
                    }
                }
                if (!builder->GetInsertBlock()->getTerminator()) {
                    if (fnTy->getReturnType()->isVoidTy()) {
                        builder->CreateRetVoid();
                    } else {
                        builder->CreateRet(llvm::ConstantAggregateZero::get(fnTy->getReturnType()));
                    }
                }
                if (savedInsertBlock) { builder->SetInsertPoint(savedInsertBlock); }
                currentFunction = prevFunction;
                currentThis = prevThis;
                functions.erase("proceed");
                exitScope();
                defersStack = savedDefers;
                currentTarget = layerFunc;
            }
        }
        namespaceStack = savedNamespaceStack;
        currentFunction = savedFunction;
        globals = savedGlobals;
        currentThis = savedThis;
        currentClassName = savedClassName;
        this->currentGenericTypes = oldGenericTypes;
        currentGenericTypeStrings = oldGenericTypeStrings;
        currentNonTypeGenericValues = oldNonTypeGenerics;
        if (savedBlock) { builder->SetInsertPoint(savedBlock); }
        return fn;
    }
    std::string strip_decorations(std::string old) {
        old = old.substr(0, old.find("&"));
        old = old.substr(0, old.find("*"));
        old = old.substr(0, old.find("["));
        return old;
    }
    std::string remove_last_ptr(std::string old) {
        if (old.ends_with("*")) {
            old.pop_back();
        } else if (old.ends_with("]")) {
            size_t pos = old.rfind("[");
            if (pos != std::string::npos) { old.erase(pos); }
        }
        return old;
    }
    llvm::Function* generateSpecializedFunction(FuncDefNode* funcDef, std::string specializedName) {
        llvm::Function* savedFunction = currentFunction;
        llvm::BasicBlock* savedBlock = builder->GetInsertBlock();
        auto savedGlobals = globals;
        auto savedNamespaceStack = namespaceStack;
        auto oldGenericTypes = this->currentGenericTypes;
        auto oldGenericTypeStrings = currentGenericTypeStrings;
        auto oldNonTypeGenerics = currentNonTypeGenericValues;
        auto genericParams = genericParamsFromName(specializedName);
        if (!fulfillsGenericConstraints(funcDef->generics, genericParams, get_pos(funcDef))) {
            this->currentGenericTypes = oldGenericTypes;
            currentGenericTypeStrings = oldGenericTypeStrings;
            currentNonTypeGenericValues = oldNonTypeGenerics;
            return nullptr;
        }
        llvm::Value* savedThis = currentThis;
        std::string savedClassName = currentClassName;
        currentThis = nullptr;
        currentClassName = "";
        if (!funcDef->modifiers.empty()) {
            FuncDefNode specFn = *funcDef;
            specFn.generics.clear();
            Token specNameTok = funcDef->name_tok.value_or(Token{TokenType::IDENTIFIER, specializedName, get_pos(funcDef)});
            specNameTok.value = specializedName;
            specFn.name_tok = specNameTok;
            for (auto& param : specFn.params) { param.type.value = substituteGenerics(param.type.value); }
            for (auto& ret : specFn.return_types) { ret.value = substituteGenerics(ret.value); }
            llvm::Function* specializedResult = emitFuncDef(specFn);
            this->currentGenericTypes = oldGenericTypes;
            currentGenericTypeStrings = oldGenericTypeStrings;
            currentNonTypeGenericValues = oldNonTypeGenerics;
            namespaceStack = savedNamespaceStack;
            currentFunction = savedFunction;
            globals = savedGlobals;
            if (savedBlock) { builder->SetInsertPoint(savedBlock); }
            return specializedResult;
        }
        namespaceStack.clear();
        size_t nsSep = specializedName.rfind("::");
        if (nsSep != std::string::npos) { namespaceStack = {specializedName.substr(0, nsSep)}; }
        enterScope();
        llvm::FunctionType* fnTy = llvmFuncTypeFor(funcDef->return_types, funcDef->params);
        llvm::Function* fn = module->getFunction(specializedName);
        if (!fn) fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, specializedName, module);
        if (funcDef->is_volatile) {
            fn->addFnAttr("noipa");
            fn->addFnAttr(llvm::Attribute::NoInline);
            fn->addFnAttr(llvm::Attribute::OptimizeNone);
        }
        for (int i = 0; i < fnTy->getNumParams(); i++) {
            auto it = funcDef->params.begin();
            std::advance(it, i);
            if (it->type.value.starts_with("out ")) {
                fn->addParamAttr(i, llvm::Attribute::WriteOnly);
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            } else if (it->type.value.starts_with("inout ")) {
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            }
            if (it->type.value.ends_with("restrict")) { fn->addParamAttr(i, llvm::Attribute::NoAlias); }
        }
        currentFunction = fn;
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", fn);
        builder->SetInsertPoint(entry);
        auto argIt = fn->arg_begin();
        unsigned idx = 0;
        for (auto& arg : fn->args()) {
            auto& param = *std::next(funcDef->params.begin(), idx);
            arg.setName(param.name.value);
            auto* alloca = createEntryAlloca(arg.getName().str(), arg.getType());
            builder->CreateStore(&arg, alloca);
            locals[param.name.value] = alloca;
            if (param.signature.has_value()) {
                varTypes[param.name.value] = "fn";
                lambdaTypes[param.name.value] = llvmFuncTypeFor(param.signature->return_types, param.signature->params);
            } else {
                std::string t = param.type.value;
                if (t.find("[]") != std::string::npos) {
                    int dims = 0;
                    size_t pos = t.find("[]");
                    while (pos != std::string::npos) {
                        dims++;
                        pos = t.find("[]", pos + 2);
                    }
                    if (dims > 0) {
                        cg_warn(get_pos(funcDef),
                                "Using type " + t + " as parameter to function, which will degrade to " + ([](std::string str) {
                                    size_t pos = 0;
                                    while ((pos = str.find("[]", pos)) != std::string::npos) {
                                        str.replace(pos, 2, "*");
                                        pos += 1;
                                    }
                                    return str;
                                }(t)) +
                                    ". Please consider changing the type of this parameter to that type instead, and if you need the length "
                                    "property (which won't exist on pointers), add an additional length parameter.",
                                "W004");
                    }
                    if (dims > 1) {
                        std::string base = t.substr(0, t.find("[]"));
                        int baseTypeCode = getTypeCode(base);
                        if (alloca->getType()->isArrayTy()) { arrayLengths[param.name.value] = alloca->getType()->getArrayNumElements(); }
                        arrayTypeStrings[param.name.value] = base;
                        varTypes[param.name.value] = param.type.value;
                    } else {
                        std::string base = t.substr(0, t.find("[]"));
                        if (alloca->getType()->isArrayTy()) { arrayLengths[param.name.value] = alloca->getType()->getArrayNumElements(); }
                        arrayTypeStrings[param.name.value] = base;
                        varTypes[param.name.value] = param.type.value;
                    }
                } else {
                    varTypes[param.name.value] = t;
                }
            }
            volatileVars[param.name.value] = param.isVolatile;
            idx++;
        }

        for (auto& stmt : funcDef->body->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            if (fnTy->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateRet(llvm::Constant::getNullValue(fnTy->getReturnType()));
            }
        }
        currentThis = savedThis;
        currentClassName = savedClassName;
        exitScope();
        namespaceStack = savedNamespaceStack;
        currentFunction = savedFunction;
        globals = savedGlobals;
        this->currentGenericTypes = oldGenericTypes;
        currentGenericTypeStrings = oldGenericTypeStrings;
        currentNonTypeGenericValues = oldNonTypeGenerics;
        if (savedBlock) { builder->SetInsertPoint(savedBlock); }
        return fn;
    }
    llvm::GlobalVariable* getOrCreateVtable(const std::string& name, llvm::ArrayType* type, llvm::Constant* initializer) {
        if (auto* existing = module->getNamedGlobal(name)) { return existing; }
        return new llvm::GlobalVariable(*module, type, true, llvm::GlobalValue::ExternalLinkage, initializer, name);
    }
    llvm::Value* packVariadicArgs(const std::vector<llvm::Value*>& var_vals) {
        llvm::Value* args_cnt = builder->getInt32(var_vals.size());
        llvm::Value* items_array = builder->CreateAlloca(builder->getPtrTy(), args_cnt, "varargs_array");
        for (size_t i = 0; i < var_vals.size(); ++i) {
            llvm::Value* index = builder->getInt32(i);
            llvm::Value* element_ptr = builder->CreateGEP(builder->getPtrTy(), items_array, index);
            llvm::Value* valueToStore = var_vals[i];
            llvm::Type* valTy = valueToStore->getType();
            if (valTy->isStructTy()) {
                llvm::Value* tempAlloc = builder->CreateAlloca(valTy, nullptr, "vararg_struct_tmp");
                builder->CreateStore(valueToStore, tempAlloc);
                valueToStore = tempAlloc;
            } else if (valTy->isIntegerTy()) {
                valueToStore = builder->CreateIntToPtr(valueToStore, builder->getPtrTy(), "vararg_int_to_ptr");
            } else if (valTy->isFloatingPointTy()) {
                llvm::Value* int64Bits = nullptr;
                if (valTy->isFloatTy()) {
                    llvm::Value* int32Bits = builder->CreateBitCast(valueToStore, builder->getInt32Ty(), "float_to_i32");
                    int64Bits = builder->CreateZExt(int32Bits, builder->getIntNTy(getPtrSize()), "i32_to_i64");
                } else {
                    int64Bits = builder->CreateBitCast(valueToStore, builder->getIntNTy(getPtrSize()), "double_to_i64");
                }
                valueToStore = builder->CreateIntToPtr(int64Bits, builder->getPtrTy(), "fp_bits_to_ptr");
            }
            builder->CreateStore(valueToStore, element_ptr);
        }
        llvm::StructType* VariadicStructTy = llvm::StructType::get(context, {builder->getPtrTy(), builder->getInt32Ty(), builder->getInt32Ty()});
        llvm::Value* variadic_struct = builder->CreateAlloca(VariadicStructTy, nullptr, "variadic_struct");
        builder->CreateStore(items_array, builder->CreateStructGEP(VariadicStructTy, variadic_struct, 0));
        builder->CreateStore(args_cnt, builder->CreateStructGEP(VariadicStructTy, variadic_struct, 1));
        builder->CreateStore(builder->getInt32(0), builder->CreateStructGEP(VariadicStructTy, variadic_struct, 2));
        return variadic_struct;
    }
    std::vector<std::string> genericParamsFromName(std::string baseName) {
        size_t open = baseName.find('<');
        if (open == std::string::npos) { return {}; }
        size_t close = baseName.rfind('>');
        std::string inner = baseName.substr(open + 1, close - open - 1);
        std::vector<std::string> genericParams;
        std::string cur;
        int depth = 0;
        for (char c : inner) {
            if (c == '<')
                depth++;
            else if (c == '>')
                depth--;
            else if (c == ',' && depth == 0) {
                genericParams.push_back(trim(cur));
                cur.clear();
                continue;
            }
            cur += c;
        }
        if (!cur.empty()) genericParams.push_back(trim(cur));
        return genericParams;
    }
    std::string buildMangledName(const std::string& baseName, const std::vector<std::string>& params, bool noBrace = false) {
        if (params.empty() && noBrace) return baseName;
        std::string result = baseName + "<";
        for (size_t i = 0; i < params.size(); i++) {
            if (i != 0) result += ",";
            result += params[i];
        }
        return result + ">";
    }
    void generateStructReprFunction(std::string name, UserTypeInfo info) {
        llvm::BasicBlock* savedBB = builder->GetInsertBlock();
        if (info.kind != UserTypeKind::Struct) return;
        llvm::StructType* structTy = genericiseOrFindStruct(name);
        llvm::FunctionType* reprFnTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {structTy}, false);
        llvm::Function* reprFn = module->getFunction(name + "_repr");
        if (!reprFn) reprFn = llvm::Function::Create(reprFnTy, llvm::Function::ExternalLinkage, name + "_repr", module);
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", reprFn);
        builder->SetInsertPoint(entryBB);
        llvm::Value* structArg = reprFn->arg_begin();
        llvm::Value* result = builder->CreateGlobalString(name + "(");
        for (size_t i = 0; i < info.fields.size(); i++) {
            auto& field = info.fields[i];
            if (i > 0) {
                llvm::Value* comma = builder->CreateGlobalString(", ");
                result = callStringConcat(result, comma);
            }
            llvm::Value* fieldLabel = builder->CreateGlobalString(field.name + "=");
            result = callStringConcat(result, fieldLabel);
            llvm::Value* fieldVal = builder->CreateExtractValue(structArg, i);
            llvm::Value* fieldStr = nullptr;
            auto type = substituteGenerics(field.type);
            if (type == "int") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_int");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (type == "float") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_float");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (type == "double") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_double");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (type == "bool") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_bool");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (type == "char") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_char");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (type == "string") {
                fieldStr = fieldVal;
            } else if (structTypes.find(type) != structTypes.end()) {
                llvm::Function* nestedReprFn = module->getFunction(type + "_repr");
                if (nestedReprFn) {
                    fieldStr = builder->CreateCall(nestedReprFn, {fieldVal});
                } else {
                    fieldStr = builder->CreateGlobalString("?");
                }
            } else {
                fieldStr = builder->CreateGlobalString("?");
            }

            result = callStringConcat(result, fieldStr);
        }
        llvm::Value* closeParen = builder->CreateGlobalString(")");
        result = callStringConcat(result, closeParen);

        builder->CreateRet(result);
        if (savedBB) { builder->SetInsertPoint(savedBB); }
    }
    bool testExpression(AnyNode expr) {
        auto saved_ip = builder->saveIP();
        llvm::FunctionType* fnType = llvm::FunctionType::get(builder->getVoidTy(), false);
        llvm::Function* dummy_fn = llvm::Function::Create(fnType, llvm::Function::InternalLinkage, "__qc_concept_probe", *module);
        llvm::BasicBlock* dummyBB = llvm::BasicBlock::Create(context, "concept_probe", dummy_fn);
        builder->SetInsertPoint(dummyBB);
        bool valid = false;
        try {
            size_t size = errors.size();
            llvm::Value* val = emitExpr(expr);
            valid = (size == errors.size()) && (val != nullptr);
        } catch (...) { valid = false; }
        dummyBB->dropAllReferences();
        dummy_fn->eraseFromParent();
        builder->restoreIP(saved_ip);
        return valid;
    }
    void proversFromConceptInfo(std::string conceptName, UserTypeInfo info) {
        if (info.provees.empty()) return;
        auto [conceptInfo, _] = genericiseOrFindConcept(resolveTypeName(conceptName, false));
        for (auto& proof : info.provees) {
            auto oldNamespaceStack = std::move(namespaceStack);
            namespaceStack = proof.namespacePath;
            std::string mapKey = proof.proverName.value;
            auto userIt = userTypes.find(baseTypeName(resolveTypeName(mapKey)));
            if (userIt == userTypes.end()) {
                namespaceStack = oldNamespaceStack;
                continue;
            }
            UserTypeInfo userInfo = userIt->second;
            std::unordered_set<std::string> additionalProofNames;
            for (auto& proofMethod : proof.additionalProof) { additionalProofNames.insert(proofMethod.name_tok.value); }
            auto alreadyDefd = [&](const std::string& methodName, const std::vector<Parameter>& sigParams) -> bool {
                std::string mangledName = mapKey + "_" + methodName;
                auto it = functionDefs.find(mangledName);
                if (it == functionDefs.end()) return false;
                FuncDefNode* funcNode = it->second;
                if (!funcNode || funcNode->params.size() != sigParams.size()) return false;
                auto actualParamIt = funcNode->params.begin();
                for (size_t i = 0; i < sigParams.size(); ++i, ++actualParamIt) {
                    std::string expectedType = (sigParams[i].type.value == "Self") ? mapKey + "*" : sigParams[i].type.value;
                    std::string actualType = (actualParamIt->type.value == "Self") ? mapKey + "*" : actualParamIt->type.value;
                    if (resolveTypeName(expectedType, false) != resolveTypeName(actualType, false)) { return false; }
                }
                return true;
            };
            auto matchesSignature = [&](const ConceptInfo::FunctionSignature& sig, const UserTypeInfo& targetType,
                                        const ConceptProvee& proof) -> bool {
                std::string methodName = sig.name.value;
                auto matchesParams = [&](const std::vector<Parameter>& sigParams, const std::vector<Parameter>& targetParams) {
                    if (sigParams.size() != targetParams.size()) return false;
                    for (size_t i = 0; i < sigParams.size(); ++i) {
                        std::string sigParamTy = (sigParams[i].type.value == "Self") ? mapKey + "*" : sigParams[i].type.value;
                        std::string targetParamTy = (targetParams[i].type.value == "Self") ? mapKey + "*" : targetParams[i].type.value;
                        if (resolveTypeName(sigParamTy, false) != resolveTypeName(targetParamTy, false)) return false;
                    }
                    return true;
                };
                for (auto& method : targetType.classMethods) {
                    if (method.name_tok.value == methodName && matchesParams(sig.params, method.params)) return true;
                }
                for (auto& proofMethod : proof.additionalProof) {
                    if (proofMethod.name_tok.value == methodName && matchesParams(sig.params, proofMethod.params)) return true;
                }
                bool res = alreadyDefd(methodName, sig.params);
                return res;
            };
            std::function<bool(const std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>&)> verifyBlock =
                [&](const std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>& block) -> bool {
                auto b = block.first;
                std::vector<std::string> scopedParams;
                for (auto& param : b.params) {
                    std::string param_type_name = (param.first == "Proving") ? mapKey
                                                  : (param.first == "Self")  ? mapKey + "*"
                                                                             : resolveTypeName(param.first, false);
                    locals[param.second] = createEntryAlloca(param.second, llvmTypeFor(param_type_name));
                    varTypes[param.second] = param_type_name;
                    volatileVars[param.second] = false;
                    scopedParams.push_back(param.second);
                }
                std::vector<std::pair<Position, std::optional<std::string>>> failedConstraints;
                std::vector<Position> passedConstraints;
                for (const Token& requiredConcept : b.requiredConcepts) {
                    std::string reqConceptName = resolveTypeName(requiredConcept.value, false);
                    std::string reqConceptBase = baseTypeName(reqConceptName);
                    auto reqIt = userTypes.find(reqConceptBase);
                    bool isProved = false;
                    if (reqIt != userTypes.end() && reqIt->second.kind == UserTypeKind::Concept) {
                        std::string resolvedCandidate = resolveTypeName(mapKey, false);
                        isProved = std::ranges::any_of(reqIt->second.provees, [&](const ConceptProvee& c) {
                            auto oldNamespaceStck = std::move(namespaceStack);
                            namespaceStack = c.namespacePath;
                            bool res = resolveTypeName(c.proverName.value, false) == resolvedCandidate;
                            namespaceStack = std::move(oldNamespaceStck);
                            return res;
                        });
                    }
                    if (!isProved) {
                        failedConstraints.push_back({requiredConcept.pos, "missing required concept proof: " + requiredConcept.value});
                    } else {
                        passedConstraints.push_back(requiredConcept.pos);
                    }
                }
                for (const auto& sig : b.signatures) {
                    if (!matchesSignature(sig, userInfo, proof)) {
                        failedConstraints.push_back({sig.name.pos, "missing matching method: " + sig.print()});
                    } else {
                        passedConstraints.push_back(sig.name.pos);
                    }
                }
                for (const auto& sub : b.subblocks) {
                    if (!verifyBlock(std::pair(sub, std::nullopt))) {
                        failedConstraints.push_back({sub.constraint.pos, "nested block constraint failed"});
                    } else {
                        passedConstraints.push_back(sub.constraint.pos);
                    }
                }
                for (const auto& node : b.nodes) {
                    if (!testExpression(node)) {
                        failedConstraints.push_back({get_pos(node), "expression failed"});
                    } else {
                        passedConstraints.push_back(get_pos(node));
                    }
                }
                for (const auto& name : scopedParams) {
                    locals.erase(name);
                    varTypes.erase(name);
                    volatileVars.erase(name);
                }
                bool failed = false;
                bool isAtLeast = false;
                int requiredCount = -1;
                if (b.constraint.value == "all_of") {
                    failed = !failedConstraints.empty();
                } else if (b.constraint.value.ends_with("_of")) {
                    std::string val = b.constraint.value;
                    if (val.starts_with("at_least ")) {
                        isAtLeast = true;
                        val = val.substr(std::string("at_least ").length());
                    }
                    size_t pos = val.find("_of");
                    if (pos != std::string::npos) {
                        std::string numStr = val.substr(0, pos);
                        if (!numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit)) { requiredCount = std::stoi(numStr); }
                    }
                    if (isAtLeast)
                        failed = (int)passedConstraints.size() < requiredCount;
                    else
                        failed = (int)passedConstraints.size() != requiredCount;
                }
                if (failed && !block.second.has_value()) {
                    if (requiredCount >= 0) {
                        cg_error(proof.proverName.pos, "failed to prove concept " + conceptName + " for type " + mapKey, "QC-C019");
                        cg_note(block.first.constraint.pos, "due to this constraint block", true);
                        cg_note(block.first.constraint.pos,
                                (isAtLeast ? "less than " + std::to_string(requiredCount) + " constraints were fulfilled"
                                           : "the amount of fulfilled constraints was not equal to " + std::to_string(requiredCount)) +
                                    " (amount of fulfilled constraints: " + std::to_string(passedConstraints.size()) + ")");
                        cg_note(proof.proverName.pos, "failed constraints were:");
                        for (auto& [failedConstraintPos, additionalMessage] : failedConstraints) {
                            cg_note(failedConstraintPos, additionalMessage.has_value() ? ("    " + additionalMessage.value()) : "", true);
                        }
                        cg_note(proof.proverName.pos, "passed constraints were:");
                        for (auto& passedConstraintPos : passedConstraints) { cg_note(passedConstraintPos, "", true); }
                    } else {
                        cg_error(proof.proverName.pos, "failed to prove concept " + conceptName + " for type " + mapKey, "QC-C019");
                        cg_note(block.first.constraint.pos, "due to this constraint block", true);
                        cg_note(proof.proverName.pos, "failed constraints were:");
                        for (auto& [failedConstraintPos, additionalMessage] : failedConstraints) {
                            cg_note(failedConstraintPos, additionalMessage.has_value() ? ("    " + additionalMessage.value()) : "", true);
                        }
                    }
                }
                if (block.second.has_value()) return !failed;
                return !failed;
            };
            bool allConceptBlocksPassed = true;
            for (std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>& block : conceptInfo.blocks) {
                bool blockPassed = verifyBlock(block);
                if (!blockPassed && !block.second.has_value()) { allConceptBlocksPassed = false; }
                if (blockPassed || block.second.has_value()) {
                    if (info.kind == UserTypeKind::Class) {
                        for (auto& proofMethod : proof.additionalProof) {
                            info.classMethods.push_back(proofMethod);
                            size_t newIdx = info.classMethods.size() - 1;
                            if (!proofMethod.generics.empty()) {
                                if (!proofMethod.is_static) { genericMethodIndices[mapKey].push_back(newIdx); }
                            } else {
                                std::string methodName = mapKey + "_" + proofMethod.name_tok.value;
                                std::vector<llvm::Type*> paramTypes;
                                paramTypes.push_back(llvm::PointerType::get(context, 0));
                                llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(proofMethod.return_types, proofMethod.params);
                                for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
                                llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
                                llvm::Function* fn = module->getFunction(methodName);
                                if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, methodName, module); }
                                classMethods[mapKey][proofMethod.name_tok.value].push_back(fn);
                                functionDefs[methodName] = funcDefFromClassMethod(proofMethod, mapKey, "_");
                            }
                        }
                    } else {
                        for (auto& proofMethod : proof.additionalProof) {
                            std::string methodName = mapKey + "_" + proofMethod.name_tok.value;
                            ClassMethodInfo concreteMethod = proofMethod;
                            for (auto& param : concreteMethod.params) {
                                if (param.type.value == "Self") { param.type.value = mapKey + "*"; }
                            }
                            for (auto& ret : concreteMethod.return_types) {
                                if (ret.value == "Self") { ret.value = mapKey + "*"; }
                            }
                            FuncDefNode* funcNode = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                            functionDefs[methodName] = funcNode;
                            if (!concreteMethod.generics.empty()) { continue; }
                            emitFuncDef(*funcNode);
                        }
                    }
                }
                if (!blockPassed && block.second.has_value()) {
                    auto& defaultBlock = block.second.value();
                    std::string targetModifier = (info.kind == UserTypeKind::Class) ? "class" : "else";
                    for (auto& [modifierTok, defaultMethod] : defaultBlock.definitions) {
                        if (modifierTok.value == targetModifier) {
                            std::string methodName = defaultMethod.name_tok.value;
                            bool alreadyProvided = alreadyDefd(methodName, defaultMethod.params);
                            if (!alreadyProvided) {
                                for (auto& proofMethod : proof.additionalProof) {
                                    if (proofMethod.name_tok.value == methodName) {
                                        alreadyProvided = true;
                                        break;
                                    }
                                }
                            }
                            if (!alreadyProvided) {
                                ClassMethodInfo concreteMethod = defaultMethod;
                                for (auto& param : concreteMethod.params) {
                                    if (param.type.value == "Self") { param.type.value = mapKey + "*"; }
                                }
                                for (auto& ret : concreteMethod.return_types) {
                                    if (ret.value == "Self") { ret.value = mapKey + "*"; }
                                }
                                if (info.kind == UserTypeKind::Class) {
                                    info.classMethods.push_back(concreteMethod);
                                    size_t newIdx = info.classMethods.size() - 1;
                                    if (!concreteMethod.generics.empty()) {
                                        if (!concreteMethod.is_static) { genericMethodIndices[mapKey].push_back(newIdx); }
                                    } else {
                                        std::string mangledName = mapKey + "_" + concreteMethod.name_tok.value;
                                        std::vector<llvm::Type*> paramTypes;
                                        paramTypes.push_back(llvm::PointerType::get(context, 0));
                                        llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(concreteMethod.return_types, concreteMethod.params);
                                        for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
                                        llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
                                        llvm::Function* fn = module->getFunction(mangledName);
                                        if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, module); }
                                        classMethods[mapKey][concreteMethod.name_tok.value].push_back(fn);
                                        functionDefs[mangledName] = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                                    }
                                } else {
                                    std::string mangledName = mapKey + "_" + concreteMethod.name_tok.value;
                                    FuncDefNode* funcNode = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                                    functionDefs[mangledName] = funcNode;
                                    if (!concreteMethod.generics.empty()) { continue; }
                                    emitFuncDef(*funcNode);
                                }
                            }
                        }
                    }
                }
            }
            namespaceStack = oldNamespaceStack;
        }
    }
    void proveConceptsForTypeInfo(std::string mapKey, UserTypeInfo info) {
        if (info.provees.empty()) return;
        for (auto& proof : info.provees) {
            auto oldNamespaceStack = std::move(namespaceStack);
            namespaceStack = proof.namespacePath;
            std::string conceptName = proof.conceptName.value;
            auto [conceptInfo, exists] = genericiseOrFindConcept(resolveTypeName(conceptName, false));
            if (!exists) {
                cg_error(proof.conceptName.pos, "Unknown concept '" + conceptName + "'", "QC-C020");
                addTypeNotes(conceptName, proof.conceptName.pos, UserTypeKind::Concept);
                continue;
            }
            std::unordered_set<std::string> additionalProofNames;
            for (auto& proofMethod : proof.additionalProof) { additionalProofNames.insert(proofMethod.name_tok.value); }
            auto alreadyDefd = [&](const std::string& methodName, const std::vector<Parameter>& sigParams) -> bool {
                std::string mangledName = mapKey + "_" + methodName;
                auto it = functionDefs.find(mangledName);
                if (it == functionDefs.end()) { return false; }
                FuncDefNode* funcNode = it->second;
                if (!funcNode) return false;
                if (funcNode->params.size() != sigParams.size()) { return false; }
                auto actualParamIt = funcNode->params.begin();
                for (size_t i = 0; i < sigParams.size(); ++i, ++actualParamIt) {
                    std::string expectedType = (sigParams[i].type.value == "Self") ? mapKey + "*" : sigParams[i].type.value;
                    std::string actualType = (actualParamIt->type.value == "Self") ? mapKey + "*" : actualParamIt->type.value;
                    if (resolveTypeName(expectedType, false) != resolveTypeName(actualType, false)) { return false; }
                }
                return true;
            };
            auto matchesSignature = [&](const ConceptInfo::FunctionSignature& sig, const UserTypeInfo& targetType,
                                        const ConceptProvee& proof) -> bool {
                std::string methodName = sig.name.value;
                auto matchesParams = [&](const std::vector<Parameter>& sigParams, const std::vector<Parameter>& targetParams) {
                    if (sigParams.size() != targetParams.size()) return false;
                    for (size_t i = 0; i < sigParams.size(); ++i) {
                        std::string sigParamTy = (sigParams[i].type.value == "Self") ? mapKey + "*" : sigParams[i].type.value;
                        std::string targetParamTy = (targetParams[i].type.value == "Self") ? mapKey + "*" : targetParams[i].type.value;
                        if (resolveTypeName(sigParamTy, false) != resolveTypeName(targetParamTy, false)) return false;
                    }
                    return true;
                };
                for (auto& method : targetType.classMethods) {
                    if (method.name_tok.value == methodName && matchesParams(sig.params, method.params)) { return true; }
                }
                for (auto& proofMethod : proof.additionalProof) {
                    if (proofMethod.name_tok.value == methodName && matchesParams(sig.params, proofMethod.params)) { return true; }
                }
                if (alreadyDefd(methodName, sig.params)) { return true; }
                return false;
            };
            std::function<bool(const std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>&)> verifyBlock =
                [&](const std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>& block) -> bool {
                auto b = block.first;
                for (auto& param : b.params) {
                    std::string param_type_name;
                    if (param.first == "Proving") {
                        param_type_name = mapKey;
                    } else if (param.first == "Self") {
                        param_type_name = mapKey + "*";
                    } else {
                        param_type_name = resolveTypeName(param.first, false);
                    }

                    locals[param.second] = createEntryAlloca(param.second, llvmTypeFor(param_type_name));
                    varTypes[param.second] = param_type_name;
                    volatileVars[param.second] = false;
                }
                std::vector<std::pair<Position, std::optional<std::string>>> failedConstraints;
                std::vector<Position> passedConstraints;
                for (const Token& requiredConcept : b.requiredConcepts) {
                    std::string reqConceptName = resolveTypeName(requiredConcept.value, false);
                    std::string reqConceptBase = baseTypeName(reqConceptName);
                    auto reqIt = userTypes.find(reqConceptBase);
                    bool isProved = false;
                    if (reqIt != userTypes.end() && reqIt->second.kind == UserTypeKind::Concept) {
                        std::string resolvedCandidate = resolveTypeName(mapKey, false);
                        isProved = std::ranges::any_of(reqIt->second.provees, [&](const ConceptProvee& c) {
                            auto oldNamespaceStck = std::move(namespaceStack);
                            namespaceStack = proof.namespacePath;
                            bool res = resolveTypeName(c.proverName.value, false) == resolvedCandidate;
                            namespaceStack = std::move(oldNamespaceStck);
                            return res;
                        });
                    }
                    if (!isProved) {
                        failedConstraints.push_back({requiredConcept.pos, "missing required concept proof: " + requiredConcept.value});
                    } else {
                        passedConstraints.push_back(requiredConcept.pos);
                    }
                }
                for (const auto& sig : b.signatures) {
                    if (!matchesSignature(sig, info, proof)) {
                        failedConstraints.push_back({sig.name.pos, "missing matching method: " + sig.print()});
                    } else {
                        passedConstraints.push_back(sig.name.pos);
                    }
                }
                for (const auto& sub : b.subblocks) {
                    if (!verifyBlock(std::pair(sub, std::nullopt))) {
                        failedConstraints.push_back({sub.constraint.pos, "nested block constraint failed"});
                    } else {
                        passedConstraints.push_back(sub.constraint.pos);
                    }
                }
                for (const auto& node : b.nodes) {
                    if (!testExpression(node)) {
                        failedConstraints.push_back({get_pos(node), "expression failed"});
                    } else {
                        passedConstraints.push_back(get_pos(node));
                    }
                }
                bool failed = false;
                bool isAtLeast = false;
                int requiredCount = -1;
                if (b.constraint.value == "all_of") {
                    failed = !failedConstraints.empty();
                } else if (b.constraint.value.ends_with("_of")) {
                    std::string val = b.constraint.value;
                    if (val.starts_with("at_least ")) {
                        isAtLeast = true;
                        val = val.substr(std::string("at_least ").length());
                    }
                    size_t pos = val.find("_of");
                    if (pos != std::string::npos) {
                        std::string numStr = val.substr(0, pos);
                        if (!numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit)) { requiredCount = std::stoi(numStr); }
                    }
                    if (isAtLeast)
                        failed = (int)passedConstraints.size() < requiredCount;
                    else
                        failed = (int)passedConstraints.size() != requiredCount;
                }
                if (failed && !block.second.has_value()) {
                    if (requiredCount >= 0) {
                        cg_error(proof.proverName.pos, "failed to prove concept " + conceptName + " for type " + mapKey, "QC-C019");
                        cg_note(block.first.constraint.pos, "due to this constraint block", true);
                        cg_note(block.first.constraint.pos,
                                (isAtLeast ? "less than " + std::to_string(requiredCount) + " constraints were fulfiled"
                                           : "the amount of fulfiled constraints was not equal to " + std::to_string(requiredCount)) +
                                    "(amount of fulfilled constraints: " + std::to_string(passedConstraints.size()) + ")");
                        cg_note(proof.proverName.pos, "failed constraints were:");
                        for (auto& [failedConstraintPos, additionalMessage] : failedConstraints) {
                            cg_note(failedConstraintPos, additionalMessage.has_value() ? ("    " + additionalMessage.value()) : "", true);
                        }
                        cg_note(proof.proverName.pos, "passed constraints were:");
                        for (auto& passedConstraintPos : passedConstraints) { cg_note(passedConstraintPos, "", true); }

                    } else {
                        cg_error(proof.proverName.pos, "failed to prove concept " + conceptName + " for type " + mapKey, "QC-C019");
                        cg_note(block.first.constraint.pos, "due to this constraint block", true);
                        cg_note(proof.proverName.pos, "failed constraints were:");
                        for (auto& [failedConstraintPos, additionalMessage] : failedConstraints) {
                            cg_note(failedConstraintPos, additionalMessage.has_value() ? ("    " + additionalMessage.value()) : "", true);
                        }
                    }
                }
                if (block.second.has_value()) return !failed;
                return true;
            };
            for (std::pair<ConceptInfo::Block, std::optional<ConceptInfo::DefaultBlock>>& block : conceptInfo.blocks) {
                bool blockPassed = verifyBlock(block);
                if (blockPassed || block.second.has_value()) {
                    if (info.kind == UserTypeKind::Class) {
                        for (auto& proofMethod : proof.additionalProof) {
                            info.classMethods.push_back(proofMethod);
                            size_t newIdx = info.classMethods.size() - 1;
                            if (!proofMethod.generics.empty()) {
                                if (!proofMethod.is_static) { genericMethodIndices[mapKey].push_back(newIdx); }
                            } else {
                                std::string methodName = mapKey + "_" + proofMethod.name_tok.value;
                                std::vector<llvm::Type*> paramTypes;
                                paramTypes.push_back(llvm::PointerType::get(context, 0));
                                llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(proofMethod.return_types, proofMethod.params);
                                for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
                                llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
                                llvm::Function* fn = module->getFunction(methodName);
                                if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, methodName, module); }
                                classMethods[mapKey][proofMethod.name_tok.value].push_back(fn);
                            }
                        }
                    } else {
                        for (auto& proofMethod : proof.additionalProof) {
                            std::string methodName = mapKey + "_" + proofMethod.name_tok.value;
                            ClassMethodInfo concreteMethod = proofMethod;
                            for (auto& param : concreteMethod.params) {
                                if (param.type.value == "Self") { param.type.value = mapKey + "*"; }
                            }
                            for (auto& ret : concreteMethod.return_types) {
                                if (ret.value == "Self") { ret.value = mapKey + "*"; }
                            }
                            FuncDefNode* funcNode = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                            functionDefs[methodName] = funcNode;
                            if (!concreteMethod.generics.empty()) { continue; }
                            emitFuncDef(*funcNode);
                        }
                    }
                }
                if (!blockPassed) {
                    if (block.second.has_value()) {
                        auto& defaultBlock = block.second.value();
                        std::string targetModifier = (info.kind == UserTypeKind::Class) ? "class" : "else";
                        for (auto& [modifierTok, defaultMethod] : defaultBlock.definitions) {
                            if (modifierTok.value == targetModifier) {
                                std::string methodName = defaultMethod.name_tok.value;
                                bool alreadyProvided = alreadyDefd(methodName, defaultMethod.params);
                                if (!alreadyProvided) {
                                    for (auto& proofMethod : proof.additionalProof) {
                                        if (proofMethod.name_tok.value == methodName) {
                                            alreadyProvided = true;
                                            break;
                                        }
                                    }
                                }
                                if (!alreadyProvided) {
                                    ClassMethodInfo concreteMethod = defaultMethod;
                                    for (auto& param : concreteMethod.params) {
                                        if (param.type.value == "Self") { param.type.value = mapKey + "*"; }
                                    }
                                    for (auto& ret : concreteMethod.return_types) {
                                        if (ret.value == "Self") { ret.value = mapKey + "*"; }
                                    }
                                    if (info.kind == UserTypeKind::Class) {
                                        info.classMethods.push_back(concreteMethod);
                                        size_t newIdx = info.classMethods.size() - 1;
                                        if (!concreteMethod.generics.empty()) {
                                            if (!concreteMethod.is_static) { genericMethodIndices[mapKey].push_back(newIdx); }
                                        } else {
                                            std::string mangledName = mapKey + "_" + concreteMethod.name_tok.value;
                                            std::vector<llvm::Type*> paramTypes;
                                            paramTypes.push_back(llvm::PointerType::get(context, 0));
                                            llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(concreteMethod.return_types, concreteMethod.params);
                                            for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
                                            llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
                                            llvm::Function* fn = module->getFunction(mangledName);
                                            if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, module); }
                                            classMethods[mapKey][concreteMethod.name_tok.value].push_back(fn);
                                            functionDefs[mangledName] = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                                        }
                                    } else {
                                        std::string mangledName = mapKey + "_" + concreteMethod.name_tok.value;
                                        FuncDefNode* funcNode = funcDefFromClassMethod(concreteMethod, mapKey, "_");
                                        functionDefs[mangledName] = funcNode;
                                        if (!concreteMethod.generics.empty()) { continue; }
                                        emitFuncDef(*funcNode);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            namespaceStack = oldNamespaceStack;
        }
    }
    std::pair<ConceptInfo, bool> genericiseOrFindConcept(const std::string& baseName) {
        const auto base = baseTypeName(baseName);
        if (genericConcepts.contains(base) && genericConcepts.at(base)) {
            if (auto it = concepts.find(baseName); it != concepts.end()) { return std::make_pair(it->second, true); }
            auto userIt = userTypes.find(base);
            if (userIt == userTypes.end()) { return std::make_pair(ConceptInfo{}, false); }
            return std::make_pair(generateGenericConcept(base, userIt->second, genericParamsFromName(baseName)), true);
        }
        if (auto it = concepts.find(baseName); it != concepts.end()) { return std::make_pair(it->second, true); }
        return std::make_pair(ConceptInfo{}, false);
    }
    llvm::StructType* genericiseOrFindClass(const std::string& baseName) {
        const auto base = baseTypeName(baseName);
        if (genericClasses.contains(base) && genericClasses.at(base)) {
            if (auto it = classTypes.find(baseName); it != classTypes.end()) { return it->second; }
            auto userIt = userTypes.find(base);
            if (userIt == userTypes.end()) { return nullptr; }
            return generateGenericClass(base, userIt->second, genericParamsFromName(baseName));
        }
        if (auto it = classTypes.find(baseName); it != classTypes.end()) {
            if (it->second->isOpaque()) generateClass(baseName, userTypes.at(baseName));
            return it->second;
        }
        return nullptr;
    }
    llvm::StructType* genericiseOrFindEnum(const std::string& baseName) {
        const auto base = baseTypeName(baseName);
        if (genericEnums.contains(base) && genericEnums.at(base)) {
            if (auto it = enumTypes.find(baseName); it != enumTypes.end()) { return it->second; }
            auto userIt = userTypes.find(base);
            if (userIt == userTypes.end()) { return nullptr; }
            return generateGenericEnum(base, userIt->second, genericParamsFromName(baseName));
        }
        if (auto it = enumTypes.find(baseName); it != enumTypes.end()) {
            auto userIt = userTypes.find(base);
            if (userIt == userTypes.end()) { return it->second; }
            return llvm::cast<llvm::StructType>(generateEnum(baseName, userIt->second));
        }
        return nullptr;
    }
    llvm::StructType* genericiseOrFindStruct(std::string baseName) {
        if (genericStructs.contains(baseTypeName(baseName)) && genericStructs[baseTypeName(baseName)]) {
            if (structTypes.count(baseName)) return structTypes[baseName];
            llvm::StructType* structTy = generateGenericStruct(baseTypeName(baseName), userTypes[baseTypeName(baseName)],
                                                               genericParamsFromName(baseName));
            if (structTy == nullptr) {
                cg_error(userTypes[baseTypeName(baseName)].pos, "Failed to create specialized version of struct " + baseTypeName(baseName),
                         "QC-S310");
                return nullptr;
            }
            return structTy;
        }
        if (structTypes.contains(baseName)) {
            llvm::StructType* structTy = structTypes[baseName];
            if (structTy->isOpaque()) generateStruct(baseName, userTypes.at(baseName));
            return structTy;
        }
        return nullptr;
    }
    UserTypeInfo genericiseOrFindUnion(std::string baseName) {
        if (genericUnions.contains(baseTypeName(baseName)) && genericUnions[baseTypeName(baseName)]) {
            if (unionTypes.count(baseName)) return substitutedUnions[baseName];
            UserTypeInfo unionInfo = generateGenericUnion(baseTypeName(baseName), userTypes[baseTypeName(baseName)], genericParamsFromName(baseName));
            if (static_cast<int>(unionInfo.kind) == 0) {
                cg_error(userTypes[baseTypeName(baseName)].pos, "Failed to create specialized version of union " + baseTypeName(baseName), "QC-S311");
                return {};
            }
            return unionInfo;
        }
        return userTypes.contains(baseTypeName(baseName)) ? userTypes[baseTypeName(baseName)] : UserTypeInfo{};
    }
    std::string genericiseOrFindAlias(std::string baseName) {
        if (genericAliases.count(baseTypeName(baseName)) && genericAliases[baseTypeName(baseName)]) {
            if (typeAliases.count(baseName)) return resolveTypeName(typeAliases[baseName]);
            std::string newAlias = generateGenericAlias(baseTypeName(baseName), userTypes[baseTypeName(baseName)], genericParamsFromName(baseName));
            if (newAlias == "") {
                cg_error(userTypes[baseTypeName(baseName)].pos, "Failed to create specialized version of union " + baseTypeName(baseName), "QC-S311");
                return "";
            }
            return resolveTypeName(newAlias);
        }
        if (typeAliases.count(baseName)) return resolveTypeName(typeAliases[baseName]);
        return baseName;
    }
    std::string resolveTypeName(std::string name, bool strip = true) {
        if (name == "int" || name == "float" || name == "double" || name == "char" || name == "bool" || name == "qbool" || name == "string" ||
            name == "byte" || name == "void" || name == "auto" || name == "short int" || name == "long int" || name == "long double" ||
            name == "addr_t" || name == "nibble") {
            return name;
        }
        std::string suffix;
        while (!name.empty() && (name.ends_with("*") || name.ends_with("&") || name.ends_with("[]"))) {
            if (name.ends_with("[]")) {
                suffix = "[]" + suffix;
                name = name.substr(0, name.size() - 2);
            } else {
                suffix = std::string(1, name.back()) + suffix;
                name = name.substr(0, name.size() - 1);
            }
        }
        auto finish = [&](std::string s) { return s + suffix; };
        name = genericiseOrFindAlias(name);
        std::string savedName = name;
        std::vector<std::string> params;
        for (std::string param : genericParamsFromName(savedName)) { params.push_back(resolveTypeName(param, false)); }
        if (!params.empty()) savedName = buildMangledName(baseTypeName(name), params);
        name = baseTypeName(name);

        if (name.find("::") != std::string::npos) {
            if (classTypes.find(savedName) != classTypes.end()) return finish(strip ? name : savedName);
            if (genericClasses.find(name) != genericClasses.end()) return finish(strip ? name : savedName);
            if (genericStructs.find(name) != genericStructs.end()) return finish(strip ? name : savedName);
            if (genericUnions.find(name) != genericUnions.end()) return finish(strip ? name : savedName);
            if (genericEnums.find(name) != genericEnums.end()) return finish(strip ? name : savedName);
            if (structTypes.find(savedName) != structTypes.end()) return finish(strip ? name : savedName);
            if (enumTypes.find(savedName) != enumTypes.end()) return finish(strip ? name : savedName);
            if (unionTypes.find(savedName) != unionTypes.end()) return finish(strip ? name : savedName);
            if (typeAliases.find(savedName) != typeAliases.end()) return finish(strip ? name : savedName);
            if (concepts.find(savedName) != concepts.end()) return finish(strip ? name : savedName);
            if (genericConcepts.find(savedName) != genericConcepts.end()) return finish(strip ? name : savedName);
        }

        std::string current = getCurrentNamespace();

        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            std::string fullSavedName = current.empty() ? savedName : current + "::" + savedName;
            std::string result = strip ? fullName : fullSavedName;
            if (concepts.count(fullSavedName)) return finish(result);
            if (genericConcepts.count(fullName)) return finish(result);
            if (typeAliases.count(fullSavedName)) return finish(result);
            if (classTypes.count(fullSavedName)) return finish(result);
            if (genericClasses.count(fullName)) return finish(result);
            if (genericStructs.count(fullName)) return finish(result);
            if (genericUnions.count(fullName)) return finish(result);
            if (structTypes.count(fullSavedName)) return finish(result);
            if (enumTypes.count(fullSavedName)) return finish(result);
            if (genericEnums.count(fullName)) return finish(result);
            if (unionTypes.count(fullSavedName)) return finish(result);
            if (hasArrayType(fullSavedName)) return finish(result);
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return finish(name);
    }
    llvm::Type* getPointeeType(const std::string& name) {
        std::string typeStr = resolveVarType(name);
        if (typeStr.empty()) {
            llvm::Value* ptr = resolveVariable(name);
            if (!ptr) {
                cg_error(Position(Position::INVALID_FILE_ID, 0, 0, 0), "Failed to resolve var type");
                return nullptr;
            }
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(ptr)) { return alloca->getAllocatedType(); }
            if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(ptr)) { return gv->getValueType(); }
            return nullptr;
        }
        if (typeStr.ends_with("&")) { typeStr.pop_back(); }

        return llvmTypeFor(typeStr);
    }
    llvm::Value* resolveGlobal(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            auto git = globals.find(name);
            if (git != globals.end()) return git->second;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            auto git = globals.find(fullName);
            if (git != globals.end()) return git->second;

            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        auto git = globals.find(name);
        if (git != globals.end()) return git->second;
        return nullptr;
    }
    llvm::FunctionType* resolveLambdaType(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            auto it = lambdaTypes.find(name);
            if (it != lambdaTypes.end()) return it->second;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            auto it = lambdaTypes.find(fullName);
            if (it != lambdaTypes.end()) return it->second;

            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        auto it = lambdaTypes.find(name);
        if (it != lambdaTypes.end()) return it->second;
        return nullptr;
    }
    llvm::Value* resolveVariable(const std::string& name) {
        if (name == "this" && currentThis && !currentClassName.empty()) return currentThis;
        if (name.find("::") != std::string::npos) {
            if (hasLocal(name)) return findLocal(name)->second;
            if (globals.count(name)) return globals[name];
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (hasLocal(fullName)) return findLocal(fullName)->second;
            auto git = globals.find(fullName);
            if (git != globals.end()) { return git->second; }
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        if (hasLocal(name)) return findLocal(name)->second;
        if (globals.count(name)) return globals[name];
        return nullptr;
    }

    std::vector<std::string> getVisibleVariables() {
        std::vector<std::string> vars;
        std::string current = getCurrentNamespace();
        while (true) {
            std::string prefix = current.empty() ? "" : current + "::";
            for (auto& [name, type] : varTypes) {
                if (!name.starts_with(prefix)) continue;
                std::string relative = name.substr(prefix.size());
                vars.push_back(relative);
            }
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return vars;
    }
    std::string resolveVarType(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            if (hasVarType(name)) return findVarType(name)->second;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (hasVarType(fullName)) return findVarType(fullName)->second;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        if (hasVarType(name)) return findVarType(name)->second;
        return "";
    }
    bool resolveVolatileVar(const std::string& name) {
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (hasVolatileVar(fullName)) return findVolatileVar(fullName)->second;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        if (hasVolatileVar(name)) return findVolatileVar(name)->second;
        return false;
    }
    std::optional<std::string> resolveArrayType(const std::string& name) {
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (hasArrayType(fullName)) { return findArrayType(fullName)->second; }
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = pos == std::string::npos ? "" : current.substr(0, pos);
        }
        if (hasArrayType(name)) { return findArrayType(name)->second; }
        return std::nullopt;
    }
    llvm::Value* getVarAddress(const std::string& name) {
        llvm::Value* addr = resolveVariable(name);
        if (resolveVarType(name).ends_with("&")) { return builder->CreateLoad(builder->getPtrTy(), addr); }
        return addr;
    }
    std::string resolveMetadataName(const std::string& name) {
        if (name.find("::") != std::string::npos) return name;

        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return name;
    }
    llvm::Function* resolveFunction(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            if (llvm::Function* f = module->getFunction(name)) return f;
            if (auto fi = functions.find(name); fi != functions.end()) return fi->second;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;

            llvm::Function* fn = module->getFunction(fullName);
            if (fn) return fn;
            auto fi = functions.find(fullName);
            if (fi != functions.end()) return fi->second;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return nullptr;
    }
    FuncDefNode* resolveFuncDef(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            if (auto fi = functionDefs.find(name); fi != functionDefs.end()) return fi->second;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (auto fi = functionDefs.find(fullName); fi != functionDefs.end()) return fi->second;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return nullptr;
    }
    auto resolveFuncDefIt(const std::string& name) {
        if (name.find("::") != std::string::npos) {
            if (auto fi = functionDefs.find(name); fi != functionDefs.end()) return fi;
        }
        std::string current = getCurrentNamespace();
        while (true) {
            std::string fullName = current.empty() ? name : current + "::" + name;
            if (auto fi = functionDefs.find(fullName); fi != functionDefs.end()) return fi;
            if (current.empty()) break;
            size_t pos = current.rfind("::");
            current = (pos == std::string::npos) ? "" : current.substr(0, pos);
        }
        return functionDefs.end();
    }
    llvm::Value* toTruthiness(llvm::Value* v, const Position& pos) {
        llvm::Type* ty = v->getType();

        if (ty->isIntegerTy(1)) { return v; }

        if (ty->isIntegerTy(2)) {
            llvm::Value* zero = builder->getIntN(2, 0);
            return builder->CreateICmpNE(v, zero, "qbool_truthy");
        }

        if (ty->isIntegerTy()) {
            llvm::Value* zero = llvm::ConstantInt::get(ty, 0);
            return builder->CreateICmpNE(v, zero, "int_truthy");
        }

        if (ty->isFloatingPointTy()) {
            llvm::Value* zero = llvm::ConstantFP::get(ty, 0.0);
            return builder->CreateFCmpONE(v, zero, "float_truthy");
        }

        if (ty->isPointerTy()) {
            llvm::Value* nullPtr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ty));
            return builder->CreateICmpNE(v, nullPtr, "ptr_truthy");
        }
        if (auto structTy = llvm::dyn_cast<llvm::StructType>(ty)) {
            std::string className = structTy->getName().str();

            auto evalMethod = findMethodInHierarchy(className, "_eval");
            if (evalMethod.first) {
                std::vector<llvm::Value*> args;

                llvm::AllocaInst* temp = createEntryAlloca("temp_eval", ty);
                builder->CreateStore(v, temp);
                args.push_back(temp);
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    llvm::InvokeInst* invoke = builder->CreateInvoke(evalMethod.first, contBB, currentLandingPad(), args);
                    builder->SetInsertPoint(contBB);
                    return invoke;
                }
                return builder->CreateCall(evalMethod.first, args);
            }
        }
        return builder->getInt1(1);
    }
    std::string getCombinationalOperatorMethodName(TokenType op) {
        switch (op) {
        case TokenType::PLUS_EQ: return "operator+=";
        case TokenType::MINUS_EQ: return "operator-=";
        case TokenType::MUL_EQ: return "operator*=";
        case TokenType::DIV_EQ: return "operator/=";
        case TokenType::MOD_EQ: return "operator%=";
        case TokenType::BIT_X_EQ: return "operator$=";
        case TokenType::BIT_A_EQ: return "operator&=";
        case TokenType::BIT_O_EQ: return "operator|=";
        case TokenType::LSH_EQ: return "operator<<=";
        case TokenType::RSH_EQ: return "operator|>=";
        case TokenType::LRSH_EQ: return "operator:>=";
        case TokenType::RROT_EQ: return "operator|>>=";
        case TokenType::LROT_EQ: return "operator<<<=";
        default: return "";
        }
    }
    std::string getRoperatorMethodName(TokenType op) {
        switch (op) {
        case TokenType::MINUS: return "roperator-";
        case TokenType::DIV: return "roperator/";
        case TokenType::MOD: return "roperator%";
        case TokenType::POWER: return "roperator#^";
        case TokenType::RSHIFT: return "roperator|>";
        case TokenType::LOGICAL_RSHIFT: return "roperator:>";
        case TokenType::R_ROT: return "roperator|>>";
        case TokenType::LSHIFT: return "roperator<<";
        case TokenType::L_ROT: return "roperator<<<";
        default: return "";
        }
    }
    std::string getOperatorMethodName(TokenType op) {
        switch (op) {
        case TokenType::PLUS: return "operator+";
        case TokenType::MINUS: return "operator-";
        case TokenType::MUL: return "operator*";
        case TokenType::DIV: return "operator/";
        case TokenType::EQ_TO: return "operator==";
        case TokenType::NOT_EQ: return "operator!=";
        case TokenType::OR: return "operator||";
        case TokenType::AND: return "operator&&";
        case TokenType::NOT: return "operator!";
        case TokenType::MORE: return "operator>";
        case TokenType::LESS: return "operator<";
        case TokenType::MORE_EQ: return "operator>=";
        case TokenType::LESS_EQ: return "operator<=";
        case TokenType::POWER: return "operator#^";
        case TokenType::MOD: return "operator%";
        case TokenType::QNOT: return "operator!!";
        case TokenType::QAND: return "operator&&&";
        case TokenType::QOR: return "operator|||";
        case TokenType::QXOR: return "operator^^";
        case TokenType::COLLAPSE_OR: return "operator|&|";
        case TokenType::COLLAPSE_AND: return "operator&|&";
        case TokenType::XOR: return "operator^";
        case TokenType::INCREMENT: return "operator++";
        case TokenType::DECREMENT: return "operator--";
        case TokenType::BITWISE_NOT: return "operator~";
        case TokenType::RSHIFT: return "operator|>";
        case TokenType::LOGICAL_RSHIFT: return "operator:>";
        case TokenType::R_ROT: return "operator|>>";
        case TokenType::LSHIFT: return "operator<<";
        case TokenType::L_ROT: return "operator<<<";
        case TokenType::BITWISE_XOR: return "operator$";
        case TokenType::PIPE: return "operator|";
        case TokenType::AMPERSAND: return "operator&";
        default: return "";
        }
    }
    std::string getUnaryOperatorMethodName(TokenType op) {
        switch (op) {
        case TokenType::QNOT: return "operator!!";
        case TokenType::NOT: return "operator!";
        case TokenType::MINUS: return "operator-";
        case TokenType::INCREMENT: return "operator++";
        case TokenType::DECREMENT: return "operator--";
        case TokenType::BITWISE_NOT: return "operator~";
        default: return "";
        }
    }
    struct UnionMatchInfo {
        int tagIndex = -1;
        std::string memberTypeStr;
    };

    std::optional<UnionMatchInfo> matchValueToUnionVariant(const std::string& unionName, AnyNode& valueNode, llvm::Value* val) {
        auto typeIt = userTypes.find(unionName);
        if (typeIt == userTypes.end()) return std::nullopt;

        auto& members = typeIt->second.members;

        int tag = findUnionVariantTag(unionName, valueNode, val);

        if (tag < 0 || (size_t)tag >= members.size()) { return std::nullopt; }
        UnionMatchInfo info;
        info.tagIndex = tag;

        const std::string& spec = members[tag].type;
        if (spec.find(':') == std::string::npos) {
            info.memberTypeStr = spec;
        } else {
            info.memberTypeStr.clear();
        }

        return info;
    }

  private:
    llvm::Type* getTypeFromCode(int code) {
        switch (code) {
        case 0: return builder->getInt32Ty();
        case 1: return builder->getFloatTy();
        case 2: return builder->getDoubleTy();
        case 3: return builder->getInt8Ty();
        case 4: return builder->getInt1Ty();
        case 5: return builder->getIntNTy(2);
        case 6: return llvm::PointerType::get(context, 0);
        default: return builder->getInt32Ty();
        }
    }
    int getTypeCode(const std::string& type) {
        if (type == "int") return 0;
        if (type == "float") return 1;
        if (type == "double") return 2;
        if (type == "char") return 3;
        if (type == "bool") return 4;
        if (type == "qbool") return 5;
        if (type == "string") return 6;
        return -1;
    }
    int getTypeCodeFromLLVM(llvm::Type* ty) {
        if (ty->isFloatTy()) return 1;    // float
        if (ty->isDoubleTy()) return 2;   // double
        if (ty->isIntegerTy(8)) return 3; // char
        if (ty->isIntegerTy(1)) return 4; // bool
        if (ty->isIntegerTy(2)) return 5; // qbool
        if (ty->isPointerTy()) return 6;  // string (or any pointer)
        if (ty->isIntegerTy()) return 0;  // int
        return -1;
    }
    llvm::Value* createJaggedArray(AnyNode& literalNode, int elemTypeCode, int depth);
    std::pair<bool, int> checkJagged(AnyNode& node);
    llvm::Value* boolToQBool(llvm::Value* boolVal);
    void addRuntimeToModule();
    llvm::Function* synthesizeProceed(const std::string& proceedName, llvm::Function* nextTargetFunc, StatementsNode* onReturnBody,
                                      const FuncDefNode& fn) {
        llvm::FunctionType* fTy = nextTargetFunc->getFunctionType();
        llvm::Function* proceedFunc = llvm::Function::Create(fTy, llvm::Function::InternalLinkage, proceedName, module);
        enterScope();
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", proceedFunc);
        llvm::BasicBlock* savedInsertBlock = builder->GetInsertBlock();
        builder->SetInsertPoint(entryBB);
        llvm::Function* oldFunction = currentFunction;
        currentFunction = proceedFunc;
        std::vector<llvm::Value*> callArgs;
        unsigned idx = 0;
        auto paramIt = fn.params.begin();
        for (auto& arg : proceedFunc->args()) {
            if (paramIt == fn.params.end()) break;
            auto& param = *paramIt++;
            arg.setName(param.name.value);
            auto* alloca = createEntryAlloca(arg.getName().str(), arg.getType());
            builder->CreateStore(&arg, alloca);
            locals[param.name.value] = alloca;
            varTypes[param.name.value] = param.type.value;
            callArgs.push_back(&arg);
        }
        llvm::Value* retVal = nullptr;
        if (fTy->getReturnType()->isVoidTy()) {
            builder->CreateCall(nextTargetFunc, callArgs);
        } else {
            retVal = builder->CreateCall(nextTargetFunc, callArgs, "raw_return");
        }
        if (onReturnBody) {
            if (retVal != nullptr) {
                llvm::AllocaInst* retAlloc = createEntryAlloca("returns", retVal->getType());
                builder->CreateStore(retVal, retAlloc);
                locals["returns"] = retAlloc;
                varTypes["returns"] = fn.return_types.empty() ? "void" : fn.return_types[0].value;
            } else {
                llvm::AllocaInst* retAlloc = createEntryAlloca("returns", builder->getPtrTy());
                builder->CreateStore(llvm::ConstantPointerNull::get(builder->getPtrTy()), retAlloc);
                locals["returns"] = retAlloc;
                varTypes["returns"] = "void*";
            }
            for (auto& stmt : onReturnBody->statements) { emitStmt(stmt); }
            if (retVal != nullptr) { retVal = builder->CreateLoad(retVal->getType(), locals["returns"]); }
        }
        if (fTy->getReturnType()->isVoidTy()) {
            builder->CreateRetVoid();
        } else {
            builder->CreateRet(retVal);
        }
        if (savedInsertBlock) { builder->SetInsertPoint(savedInsertBlock); }
        currentFunction = oldFunction;
        exitScope();
        return proceedFunc;
    }
    llvm::Function* emitFuncDef(const FuncDefNode& fn);
    llvm::FunctionType* llvmFuncTypeForHelper(const std::vector<Token>& returnTypes, const std::vector<ParamTypeInfo>& params);
    llvm::FunctionType* llvmFuncTypeFor(const std::vector<Token>& retTypes, const std::list<Parameter>& params);
    llvm::FunctionType* llvmFuncTypeFor(const std::vector<Token>& returnTypes, const std::vector<Parameter>& params);
    llvm::Type* llvmTypeFor(std::string qcType);
    std::string lambdaName();
    std::string mangleName(const FuncDefNode& fn);
    std::unordered_map<std::string, llvm::Function*> functions;
    llvm::Function* currentFunction = nullptr;
    llvm::AllocaInst* createEntryAlloca(const std::string& name, llvm::Type* ty);
    llvm::Value* emitExpr(const AnyNode& node);
    [[gnu::noinline]]
    llvm::Value* emitBinOp(BinOpNode* const* bin);
    [[gnu::noinline]]
    llvm::Value* emitVarAssign(VarAssignNode* const* va);
    [[gnu::noinline]]
    llvm::Value* emitVarAccess(VarAccessNode* const* acc);
    [[gnu::noinline]]
    llvm::Value* emitAssignExpr(AssignExprNode* const* asn);
    [[gnu::noinline]]
    llvm::Value* emitUnaryOp(UnaryOpNode* const* unary);
    [[gnu::noinline]]
    llvm::Value* emitMapLit(MapLiteralNode* const* mapLit);
    [[gnu::noinline]]
    llvm::Value* emitArrLit(ArrayLiteralNode* const* arrLit);
    [[gnu::noinline]]
    llvm::Value* emitCall(CallNode* const* callPtr);
    [[gnu::noinline]]
    llvm::Value* emitArrAcc(ArrayAccessNode* arrAcc);
    [[gnu::noinline]]
    llvm::Value* emitPropAcc(PropertyAccessNode* const* propAccess);
    [[gnu::noinline]]
    llvm::Value* emitMthdCall(MethodCallNode* const* methodCall);
    [[gnu::noinline]]
    llvm::Value* emitFieldAssign(FieldAssignNode* const* fieldAssign);
    llvm::Value* extractUnionToBestGuess(llvm::Value* unionVal) {
        std::string unionName;
        if (!isUnionType(unionVal->getType(), &unionName)) { return unionVal; }

        auto utIt = userTypes.find(unionName);
        if (utIt == userTypes.end()) { return nullptr; }

        auto& info = utIt->second;
        auto& members = info.members;
        if (members.empty()) return nullptr;
        std::string targetTypeStr;
        for (auto& m : members) {
            const std::string& t = m.type;
            if (t.empty()) continue;
            if (t.starts_with("\"") || t.starts_with("'") || std::isdigit(t[0]) || t == "true" || t == "false" || t == "qtrue" || t == "qfalse" ||
                t == "none" || t == "both") {
                continue;
            }
            targetTypeStr = t;
            break;
        }

        if (targetTypeStr.empty()) { targetTypeStr = "int"; }

        llvm::Type* targetTy = llvmTypeFor(targetTypeStr);
        llvm::Value* dataPtr = builder->CreateExtractValue(unionVal, 1, "union_data");
        llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
        return builder->CreateLoad(targetTy, typedPtr, "union_unwrapped");
    }
    llvm::Value* normalizeValue(llvm::Value* v, AnyNode& expr) {
        if (!v) return nullptr;
        llvm::Type* ty = v->getType();
        std::string unionName;
        if (isEnumType(ty)) {
            return builder->CreateExtractValue(v, 0, "enum_descriminant");
        }
        bool isUnion = isUnionType(ty, &unionName);
        if (!isUnion) { return v; }
        std::string typeName = unionName;
        auto utIt = userTypes.find(typeName);
        if (utIt == userTypes.end()) return v;

        llvm::Value* tag = builder->CreateExtractValue(v, 0, "tag");
        llvm::Value* payload = builder->CreateExtractValue(v, 1, "payload");

        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "norm_end", currentFunction);

        llvm::Type* voidPtrTy = llvm::PointerType::get(context, 0);
        llvm::AllocaInst* tmp = createEntryAlloca("norm_tmp", voidPtrTy);

        size_t memberCount = utIt->second.members.size();

        llvm::SwitchInst* sw = builder->CreateSwitch(tag, endBB, memberCount);

        for (size_t i = 0; i < memberCount; ++i) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "norm_case_" + std::to_string(i), currentFunction);
            sw->addCase(builder->getInt32(i), caseBB);
            builder->SetInsertPoint(caseBB);

            std::string typeStr;
            if (isUnion) {
                typeStr = utIt->second.members[i].type;
                size_t colonPos = typeStr.find(':');
                if (colonPos != std::string::npos) { typeStr = typeStr.substr(0, colonPos); }
            }
            llvm::Type* memberTy = llvmTypeFor(typeStr);

            llvm::Value* typedPtr = builder->CreateBitCast(payload, llvm::PointerType::get(context, 0));
            llvm::Value* loaded = builder->CreateLoad(memberTy, typedPtr, "member");
            llvm::AllocaInst* memberAlloc = createEntryAlloca("member_tmp", memberTy);
            builder->CreateStore(loaded, memberAlloc);
            llvm::Value* asVoidPtr = builder->CreateBitCast(memberAlloc, voidPtrTy);
            builder->CreateStore(asVoidPtr, tmp);
            builder->CreateBr(endBB);
        }
        builder->SetInsertPoint(endBB);
        return builder->CreateLoad(voidPtrTy, tmp, "normalized");
    }
    [[gnu::noinline]]
    void emitMultiRet(MultiReturnNode *mret);
    [[gnu::noinline]]
    void emitRet(ReturnNode *ret);
    [[gnu::noinline]]
    void emitMultiVar(MultiVarDeclNode *mv);
    [[gnu::noinline]]
    void emitIf(IfNode *if_node);
    [[gnu::noinline]]
    void emitFor(ForNode *for_node);
    [[gnu::noinline]]
    void emitWhile(WhileNode *while_node);
    [[gnu::noinline]]
    void emitSwitch(SwitchNode *switch_node);
    [[gnu::noinline]]
    void emitQSwitch(QSwitchNode *qsw);
    [[gnu::noinline]]
    void emitQIf(QIfNode *qif_node);
    [[gnu::noinline]]
    void emitArrAssign(ArrayAssignNode *arrAssign);
    [[gnu::noinline]]
    void emitArrDecl(ArrayDeclNode *arrDecl);
    [[gnu::noinline]]
    void emitTryCatch(TryCatchNode *trycatch);
    [[gnu::noinline]]
    void emitForeach(ForeachNode *foreach);
    [[gnu::noinline]]
    void emitMatch(MatchNode *match_node);
    void emitStmt(AnyNode node);
};
#endif
#endif
