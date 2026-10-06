#define QC_EXCEPTION_CLASS 0x5143455843455054ULL
#include "compiler.h"
#include "parser.h"
#include "shared_globals.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#ifdef ENABLE_LLVM
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#ifndef __EMSCRIPTEN__
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#endif
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ModRef.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#endif
#if defined(_WIN32) || defined(_WIN64)
#include <print>
#endif
uint64_t invokeCounter = 0;
static bool random_seeded = false;
std::string strip_brace(const std::string& s) {
    std::string r = s;
    while (r.size() >= 2 && r.substr(r.size() - 2) == "[]") r = r.substr(0, r.size() - 2);
    return r;
}
std::set<std::string> inProgressGenerics;
std::string strip(const std::string& s) {
    std::string r = s;
    size_t pos;
    while (r.size() >= 2 && r.substr(r.size() - 2) == "[]") r = r.substr(0, r.size() - 2);
    return r;
}
#ifdef ENABLE_LLVM
llvm::Type* LLVMCompiler::llvmTypeFor(std::string qcType) {
    if (currentGenericTypes.find(qcType) != currentGenericTypes.end()) { return currentGenericTypes[qcType]; }
    if (!currentGenericTypeStrings.empty()) {
        std::string substituted = substituteGenerics(qcType);
        if (substituted != qcType) qcType = substituted;
    }
    std::string type = resolveTypeName(qcType, false);
    type = resolveTypeName(type, false);
    if (type == "...") { return builder->getPtrTy(); }
    if (type.ends_with("[]")) { return llvm::PointerType::get(context, 0); }
    if (type.ends_with("]")) {
        size_t open = type.rfind('[');
        if (open == std::string::npos) {
            cg_error(Position(), "malformed array type", "QC-T012");
            return nullptr;
        }
        std::string sizeStr = type.substr(open + 1, type.size() - open - 2);
        size_t count;
        try {
            count = parseInteger(sizeStr);
        } catch (...) {
            cg_error(Position(), "invalid array size: " + sizeStr, "QC-S108");
            return nullptr;
        }
        std::string elementType = type.substr(0, open);
        llvm::Type* elemTy = llvmTypeFor(elementType);
        return llvm::ArrayType::get(elemTy, count);
    }
    if (type.ends_with("&") || type.ends_with("*")) { return builder->getPtrTy(); }
    if (type == "int") return builder->getInt32Ty();
    if (type == "short int") return builder->getInt16Ty();
    if (type == "long int") return builder->getIntNTy(getPtrSize());
    if (type == "float") return builder->getFloatTy();
    if (type == "double") return builder->getDoubleTy();
    if (type == "long double") return builder->getDoubleTy();
    if (type == "nibble") return builder->getIntNTy(4);
    if (type == "addr_t") return builder->getIntNTy(getPtrSize());
    if (type == "char" || type == "byte") return builder->getInt8Ty();
    if (type == "bool") return builder->getInt1Ty();
    if (type == "qbool") return builder->getIntNTy(2);
    if (type == "string") return llvm::PointerType::get(context, 0);
    if (type == "@nullptr") return builder->getPtrTy();
    if (type.starts_with("(")) {
        std::vector<llvm::Type*> types;
        std::string current_type;
        size_t paren = 0; // please don't.
        size_t angle = 0; // PLEASE DON'T
        for (size_t i = 1; i < type.length() - 1; ++i) {
            char c = type[i];
            if (c == '(') {
                paren++;
                current_type += c;
            } else if (c == ')') {
                paren--;
                current_type += c;
            } else if (c == '<') {
                angle++;
                current_type += c;
            } else if (c == '>') {
                angle--;
                current_type += c;
            } else if (c == ',' && paren == 0 && angle == 0) {
                types.push_back(llvmTypeFor(current_type));
                current_type.clear();
            } else {
                if (c != ' ' || !current_type.empty()) { current_type += c; }
            }
        }
        if (!current_type.empty()) { types.push_back(llvmTypeFor(current_type)); }
        return llvm::StructType::get(context, types);
    }
    if (classTypes.find(type) != classTypes.end() ||
        (genericClasses.find(baseTypeName(type)) != genericClasses.end() && genericClasses[baseTypeName(type)])) {
        return genericiseOrFindClass(resolveTypeName(qcType, false));
    }
    if (structTypes.find(type) != structTypes.end() ||
        (genericStructs.find(baseTypeName(type)) != genericStructs.end() && genericStructs[baseTypeName(type)])) {
        return genericiseOrFindStruct(type);
    }
    if (enumTypes.find(type) != enumTypes.end() ||
        (genericEnums.find(baseTypeName(type)) != genericEnums.end() && genericEnums[baseTypeName(type)])) {
        return genericiseOrFindEnum(type);
    }
    if (unionTypes.find(type) != unionTypes.end() ||
        (genericUnions.find(baseTypeName(type)) != genericUnions.end() && genericUnions[baseTypeName(type)])) {
        genericiseOrFindUnion(type);
        return unionTypes[type];
    }
    if (type == "function" || type == "fn" || type.starts_with("fn (") || type.starts_with("fn(")) { return builder->getPtrTy(); }
    if (type == "void") return builder->getVoidTy();
    return nullptr;
}
bool LLVMCompiler::fulfillsGenericConstraints(std::vector<GenericType> generics, std::vector<std::string> genericParams, Position pos) {
    for (int i = 0; i < generics.size(); i++) {
        GenericType generic = generics[i];
        if (genericParams.size() <= i) {
            if (generic.defaultValue.empty()) {
                cg_error(pos, "too few generic params", "QC-G006");
                cg_note(pos, "expected " + std::to_string(generics.size()) + ", got " + std::to_string(genericParams.size()) + ".");
                return false;
            }
        }
        std::string value;
        if ((!generic.defaultValue.empty()) && genericParams.size() <= i) {
            value = generic.defaultValue;
        } else {
            value = genericParams[i];
        }
        if (!generic.isNonType && !generic.isVariadic) {
            if (generic.constraint == "pointer") {
                if (!(value.ends_with("*"))) {
                    cg_error(pos, "pointer generic constrain " + generic.name + " expectes pointer type, got " + value, "QC-G007");
                    return false;
                }
            } else if (generic.constraint == "numeric") {
                if (!(std::unordered_set<std::string>({"int", "double", "float", "byte", "nibble", "addr_t", "long double", "short int", "long int"})
                          .contains(value))) {
                    cg_error(pos, "numeric generic constrain " + generic.name + " expectes numeric type, got " + value, "QC-G008");
                    return false;
                }
            } else if (generic.constraint == "primitive" || generic.constraint == "usertype") {
                static const std::unordered_set<std::string> native_types = {
                    "int", "double", "float", "addr_t", "long double", "short int", "long int", "char", "bool", "byte", "nibble", "qbool", "string"};
                auto clean_view = value | std::views::filter([](char c) { return c != '*' && c != '&' && c != '[' && c != ']'; });
                if (native_types.contains(std::string(clean_view.begin(), clean_view.end()))) {
                    if (generic.constraint == "usertype") {
                        cg_error(pos, "usertype generic constrain " + generic.name + " expectes usertype type, got " + value, "QC-G009");
                        return false;
                    }
                } else {
                    if (generic.constraint == "primitive") {
                        cg_error(pos, "primitive generic constrain " + generic.name + " expectes primitive type, got " + value, "QC-G010");
                        return false;
                    }
                }
            } else if (!generic.constraint.empty()) {
                auto provesConcept = [&](const std::string& typeName, const std::string& conceptName) -> bool {
                    std::string resolvedConcept = resolveTypeName(conceptName, false);
                    if (!concepts.count(resolvedConcept)) {
                        cg_error(pos, "concept " + conceptName + " is not defined", "QC-C009");
                        addTypeNotes(conceptName, pos, UserTypeKind::Concept);
                        return false;
                    }
                    std::string base = baseTypeName(typeName);
                    if (!userTypes.count(base)) { return false; }
                    return std::ranges::any_of(userTypes[base].provees, [&](const ConceptProvee& proved) {
                        return resolveTypeName(proved.conceptName.value, false) == resolvedConcept;
                    });
                };
                std::vector<std::string> tokens;
                {
                    std::string current;
                    for (size_t i = 0; i < generic.constraint.size(); ++i) {
                        char c = generic.constraint[i];
                        if (std::isspace(static_cast<unsigned char>(c))) {
                            if (!current.empty()) {
                                tokens.push_back(current);
                                current.clear();
                            }
                            continue;
                        }
                        if (c == '!') {
                            if (!current.empty()) {
                                tokens.push_back(current);
                                current.clear();
                            }
                            tokens.push_back("!");
                            continue;
                        }
                        if (c == '&') {
                            if (!current.empty()) {
                                tokens.push_back(current);
                                current.clear();
                            }
                            if (i + 1 < generic.constraint.size() && generic.constraint[i + 1] == '&') {
                                tokens.push_back("&&");
                                ++i;
                            } else {
                                cg_error(pos, "expected '&&' in generic constraint", "QC-C010");
                                return false;
                            }
                            continue;
                        }
                        if (c == '|') {
                            if (!current.empty()) {
                                tokens.push_back(current);
                                current.clear();
                            }
                            if (i + 1 < generic.constraint.size() && generic.constraint[i + 1] == '|') {
                                tokens.push_back("||");
                                ++i;
                            } else {
                                cg_error(pos, "expected '||' in generic constraint", "QC-C011");
                                return false;
                            }
                            continue;
                        }
                        current += c;
                    }
                    if (!current.empty()) { tokens.push_back(current); }
                }
                size_t index = 0;
                std::function<bool()> parseNot;
                std::function<bool()> parseAnd;
                std::function<bool()> parseOr;
                parseNot = [&]() -> bool {
                    if (index < tokens.size() && tokens[index] == "!") {
                        ++index;
                        if (index >= tokens.size()) {
                            cg_error(pos, "expected concept after '!' in generic constraint", "QC-C012");
                            return false;
                        }
                        return !parseNot();
                    }
                    if (index >= tokens.size()) {
                        cg_error(pos, "expected concept in generic constraint", "QC-C013");
                        return false;
                    }
                    std::string conceptName = tokens[index++];
                    if (conceptName == "&&" || conceptName == "||" || conceptName == "!") {
                        cg_error(pos, "expected concept, got '" + conceptName + "'", "QC-C014");
                        return false;
                    }
                    return provesConcept(value, conceptName);
                };
                parseAnd = [&]() -> bool {
                    bool result = parseNot();
                    while (index < tokens.size() && tokens[index] == "&&") {
                        ++index;
                        bool rhs = parseNot();
                        result = result && rhs;
                    }
                    return result;
                };
                parseOr = [&]() -> bool {
                    bool result = parseAnd();
                    while (index < tokens.size() && tokens[index] == "||") {
                        ++index;
                        bool rhs = parseAnd();
                        result = result || rhs;
                    }
                    return result;
                };
                if (tokens.empty()) {
                    cg_error(pos, "empty generic concept constraint", "QC-C015");
                    return false;
                }
                bool satisfiesConstraint = parseOr();
                if (index != tokens.size()) {
                    cg_error(pos, "invalid generic concept constraint: " + generic.constraint, "QC-C016");
                    return false;
                }
                if (!satisfiesConstraint) {
                    cg_error(pos,
                             "concept generic constraint " + generic.name + " expects passed type to satisfy " + generic.constraint + ", got " +
                                 value,
                             "QC-C017");
                    return false;
                }
            }
            if (!generic.subconstraints.empty()) {
                if (generic.negated) {
                    for (std::string subconstraint : generic.subconstraints) {
                        if (value == subconstraint) {
                            cg_error(pos, "generic constrain !" + value + " in generic " + generic.name + " does not except type " + value,
                                     "QC-G011");
                            return false;
                        }
                    }
                } else {
                    bool is_valid = false;
                    for (std::string subconstraint : generic.subconstraints) {
                        if (value == subconstraint) { is_valid = true; }
                    }
                    if (!is_valid) {
                        cg_error(pos, "generic constrait " + generic.name + " does not except type " + value, "QC-G012");
                        return false;
                    }
                }
            }
            currentGenericTypes[generic.name] = llvmTypeFor(value);
            currentGenericTypeStrings[generic.name] = value;
        } else if (generic.isNonType) {
            GenericType generic = generics[i];
            if (genericParams.size() <= i) {
                if (generic.defaultValue.empty()) {
                    cg_error(pos, "too few generic params", "QC-G006");
                    cg_note(pos, "expected " + std::to_string(genericParams.size()) + ", got " + std::to_string(i) + ".");
                    return false;
                }
            }
            std::string gname = generic.name;
            generic.name = genericParams[i];
            currentNonTypeGenericValues[gname] = generic;
        }
    }
    return true;
};
ConceptInfo LLVMCompiler::generateGenericConcept(std::string conceptName, UserTypeInfo conceptInfo, std::vector<std::string> genericParams) {
    std::string mangled_concept_name = conceptName + "<";
    for (int j = 0; j < conceptInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = conceptInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_concept_name += val;
        if (j != conceptInfo.generics.size() - 1) { mangled_concept_name += ","; }
    }
    mangled_concept_name += ">";
    if (inProgressGenerics.count(mangled_concept_name)) return {};
    if (concepts.find(mangled_concept_name) != concepts.end()) { return genericiseOrFindConcept(mangled_concept_name).first; }
    inProgressGenerics.insert(mangled_concept_name);
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(conceptInfo.generics, genericParams, conceptInfo.pos)) {
        this->currentGenericTypes = oldGenericTypes;
        currentGenericTypeStrings = oldGenericTypeStrings;
        currentNonTypeGenericValues = oldNonTypeGenerics;
        return {};
    }
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!conceptInfo.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = conceptInfo.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(conceptInfo.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(conceptInfo.namespace_path.substr(start));
    }
    auto substituteSignature = [&](ConceptInfo::FunctionSignature sig) -> ConceptInfo::FunctionSignature {
        for (auto& retToken : sig.return_types) { retToken.value = substituteGenerics(retToken.value); }
        for (auto& param : sig.params) { param.type.value = substituteGenerics(param.type.value); }
        return sig;
    };
    std::function<ConceptInfo::Block(ConceptInfo::Block)> substituteBlock = [&](ConceptInfo::Block block) -> ConceptInfo::Block {
        for (auto& sig : block.signatures) { sig = substituteSignature(sig); }
        for (auto& reqConceptTok : block.requiredConcepts) { reqConceptTok.value = substituteGenerics(reqConceptTok.value); }
        for (auto& sub : block.subblocks) { sub = substituteBlock(sub); }
        return block;
    };
    auto substituteMethodInfo = [&](ClassMethodInfo method) -> ClassMethodInfo {
        for (auto& param : method.params) { param.type.value = substituteGenerics(param.type.value); }
        for (auto& ret : method.return_types) { ret.value = substituteGenerics(ret.value); }
        return method;
    };
    ConceptInfo specializedConcept;
    for (const auto& [block, optDefault] : conceptInfo.conceptInfo.blocks) {
        ConceptInfo::Block substitutedBlock = substituteBlock(block);
        std::optional<ConceptInfo::DefaultBlock> substitutedDefault = std::nullopt;
        if (optDefault.has_value()) {
            ConceptInfo::DefaultBlock defBlock;
            for (const auto& [modifierTok, defaultMethod] : optDefault->definitions) {
                defBlock.definitions.push_back({modifierTok, substituteMethodInfo(defaultMethod)});
            }
            substitutedDefault = defBlock;
        }
        specializedConcept.blocks.push_back({substitutedBlock, substitutedDefault});
    }
    concepts[mangled_concept_name] = specializedConcept;
    namespaceStack = oldNamespaceStack;
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    inProgressGenerics.erase(mangled_concept_name);
    return {};
}
llvm::StructType* LLVMCompiler::generateGenericClass(std::string className, UserTypeInfo classInfo, std::vector<std::string> genericParams) {
    for (std::string param : genericParams) {
        std::string resolved = resolveTypeName(param, false);
        std::string base = baseTypeName(resolved);
        if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
            generateClass(base, userTypes.at(base));
        } else if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
            generateStruct(base, userTypes.at(base));
        } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
            generateEnum(resolved, userTypes.at(resolved));
        }
    }
    std::string mangled_class_name = className + "<";
    for (int j = 0; j < classInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = classInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_class_name += val;
        if (j != classInfo.generics.size() - 1) { mangled_class_name += ","; }
    }
    mangled_class_name += ">";
    if (inProgressGenerics.count(mangled_class_name)) return nullptr;
    if (classTypes.find(mangled_class_name) != classTypes.end()) { return genericiseOrFindClass(mangled_class_name); }
    llvm::StructType* classTy = getOrCreateStructType(mangled_class_name);
    classTypes[mangled_class_name] = classTy;
    inProgressGenerics.insert(mangled_class_name);
    llvm::BasicBlock* savedBlock = builder->GetInsertBlock();
    llvm::BasicBlock::iterator savedPoint = builder->GetInsertPoint();
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(classInfo.generics, genericParams, classInfo.pos)) {
        this->currentGenericTypes = oldGenericTypes;
        currentGenericTypeStrings = oldGenericTypeStrings;
        currentNonTypeGenericValues = oldNonTypeGenerics;
        return nullptr;
    }
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!classInfo.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = classInfo.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(classInfo.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(classInfo.namespace_path.substr(start));
    }
    std::vector<llvm::Type*> fieldTypes;
    bool hasVptr = false;
    std::function<void(const std::string&, std::unordered_map<std::string, std::string>)> collectFields =
        [&](const std::string& classame, std::unordered_map<std::string, std::string> genericSubs) {
            auto it = userTypes.find(this->baseTypeName(classame));
            if (it == userTypes.end()) { throw std::string("Class not found: ") + classame; }
            auto& classIfo = it->second;
            if (!classIfo.baseClassName.empty()) {
                std::string baseFullName = classIfo.baseClassName;
                for (auto& [gname, gval] : genericSubs) {
                    size_t pos = 0;
                    while ((pos = baseFullName.find(gname, pos)) != std::string::npos) {
                        size_t end = pos + gname.size();
                        bool leftOk = pos == 0 || !(std::isalnum(static_cast<unsigned char>(baseFullName[pos - 1])) || baseFullName[pos - 1] == '_');
                        bool rightOk = end == baseFullName.size() ||
                                       !(std::isalnum(static_cast<unsigned char>(baseFullName[end])) || baseFullName[end] == '_');
                        if (leftOk && rightOk) {
                            baseFullName.replace(pos, gname.size(), gval);
                            pos += gval.size();
                        } else {
                            pos += gname.size();
                        }
                    }
                }
                std::unordered_map<std::string, std::string> baseSubs;
                std::string baseRaw = this->baseTypeName(baseFullName);
                auto baseIt = userTypes.find(baseRaw);
                if (baseIt != userTypes.end()) {
                    auto& baseInfo = baseIt->second;
                    genericiseOrFindClass(baseFullName);
                    auto baseArgs = genericParamsFromName(baseFullName);
                    for (size_t i = 0; i < baseInfo.generics.size() && i < baseArgs.size(); i++) {
                        baseSubs[baseInfo.generics[i].name] = baseArgs[i];
                    }
                }
                collectFields(baseFullName, baseSubs);
            }
            for (auto& field : classIfo.classFields) {
                std::string resolvedType = field.type;
                if (field.name == "__vptr") {
                    if (hasVptr) continue;
                    hasVptr = true;
                    resolvedType = mangled_class_name + "*";
                } else {
                    for (auto& [gname, gval] : genericSubs) {
                        size_t pos = 0;
                        while ((pos = resolvedType.find(gname, pos)) != std::string::npos) {
                            size_t end = pos + gname.size();
                            bool leftOk = pos == 0 ||
                                          !(std::isalnum(static_cast<unsigned char>(resolvedType[pos - 1])) || resolvedType[pos - 1] == '_');
                            bool rightOk = end == resolvedType.size() ||
                                           !(std::isalnum(static_cast<unsigned char>(resolvedType[end])) || resolvedType[end] == '_');
                            if (leftOk && rightOk) {
                                resolvedType.replace(pos, gname.size(), gval);
                                pos += gval.size();
                            } else {
                                pos += gname.size();
                            }
                        }
                    }
                }
                if (!resolveTypeName(resolvedType, false).ends_with("*")) {
                    std::string resolved = resolveTypeName(resolvedType, false);
                    if (!resolved.contains("<")) {
                        if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                            generateStruct(resolved, userTypes.at(resolved));
                        } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                            generateClass(resolved, userTypes.at(resolved));
                        } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                            generateEnum(resolved, userTypes.at(resolved));
                        }
                    }
                }
                if (field.isStatic) {
                    llvm::Type* fieldTy = llvmTypeFor(resolvedType);
                    std::string mangledName = mangled_class_name + "::" + field.name;
                    if (!module->getNamedGlobal(mangledName)) {
                        llvm::Constant* initVal = !std::holds_alternative<std::monostate>(field.defaultValue)
                                                      ? llvm::dyn_cast<llvm::Constant>(emitExpr(field.defaultValue))
                                                      : llvm::Constant::getNullValue(fieldTy);

                        if (!initVal) { initVal = llvm::Constant::getNullValue(fieldTy); }
                        globals[mangledName] = new llvm::GlobalVariable(*module, fieldTy, false, llvm::GlobalValue::ExternalLinkage, initVal,
                                                                        mangledName);
                        varTypes[mangledName] = resolvedType;
                        volatileVars[mangledName] = false;
                    }
                } else {
                    fieldTypes.push_back(llvmTypeFor(resolvedType));
                }
            }
        };
    std::unordered_map<std::string, std::string> rootSubs;
    for (size_t i = 0; i < classInfo.generics.size() && i < genericParams.size(); i++) { rootSubs[classInfo.generics[i].name] = genericParams[i]; }
    collectFields(mangled_class_name, rootSubs);
    if (classTypes[mangled_class_name]->isOpaque()) { classTypes[mangled_class_name]->setBody(fieldTypes); }
    if (!classInfo.baseClassName.empty()) {
        auto base_it = userTypes.find(resolveTypeName(classInfo.baseClassName));
        if (base_it != userTypes.end()) {
            auto& baseInfo = base_it->second;
            for (auto& method : classInfo.classMethods) {
                for (auto& baseMethod : baseInfo.classMethods) {
                    if (baseMethod.name_tok.value == method.name_tok.value && baseMethod.is_final) {
                        cg_error(method.name_tok.pos,
                                 "cannot override final method '" + baseMethod.name_tok.value + "' from base class '" + classInfo.baseClassName + "'",
                                 "QC-S109");
                    }
                }
            }
        }
    }
    std::vector<llvm::Constant*> vtableFuncs;
    std::vector<std::string> slotOrder;
    std::unordered_map<std::string, int> nameCounts;
    for (auto& method : classInfo.classMethods) { nameCounts[method.name_tok.value]++; }
    for (size_t methodIdx = 0; methodIdx < classInfo.classMethods.size(); methodIdx++) {
        auto& method = classInfo.classMethods[methodIdx];
        if (method.is_static) continue;
        if (method.is_constructor && classInfo.is_abstract_class) {
            cg_error(method.name_tok.pos, "cannot make a constructor on a abstract class.", "QC-S110");
            continue;
        }
        if (std::find(genericMethodIndices[baseTypeName(className)].begin(), genericMethodIndices[baseTypeName(className)].end(), methodIdx) !=
            genericMethodIndices[baseTypeName(className)].end()) {
            continue;
        }
        std::string methodName = mangled_class_name + "_" + method.name_tok.value;
        if (nameCounts[method.name_tok.value] > 1) {
            for (auto& param : method.params) { methodName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
        }
        std::vector<llvm::Type*> paramTypes;
        paramTypes.push_back(llvm::PointerType::get(context, 0));
        llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(method.return_types, method.params);
        for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
        llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
        llvm::Function* fn = module->getFunction(methodName);
        if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, methodName, module); }
        llvm::SmallVector<llvm::Metadata*, 4> retTypes;
        for (auto& ret : method.return_types) { retTypes.push_back(llvm::MDString::get(context, ret.value)); }
        fn->setMetadata("qc.return_types", llvm::MDNode::get(context, retTypes));
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
        classMethods[mangled_class_name][method.name_tok.value].push_back(fn);
        vtableFuncs.push_back(fn);
        slotOrder.push_back(methodName);
    }
    for (size_t i = 0; i < slotOrder.size(); i++) { vtableSlotIndex[mangled_class_name][slotOrder[i]] = i; }
    auto* arrTy = llvm::ArrayType::get(llvm::PointerType::get(context, 0), vtableFuncs.size());
    auto* vtableInit = llvm::ConstantArray::get(arrTy, vtableFuncs);
    auto* vtable = getOrCreateVtable(mangled_class_name + "_vtable", arrTy, vtableInit);
    vtables[mangled_class_name] = vtable;
    for (size_t methodIdx = 0; methodIdx < classInfo.classMethods.size(); methodIdx++) {
        auto& method = classInfo.classMethods[methodIdx];
        if (!method.is_static) continue;
        std::string mangledName = mangled_class_name + "::" + method.name_tok.value;
        if (nameCounts[method.name_tok.value] > 1) {
            for (auto& param : method.params) { mangledName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
        }
        llvm::FunctionType* fnTy = llvmFuncTypeFor(method.return_types, method.params);
        llvm::Function* fn = module->getFunction(mangledName);
        if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, module); }
        llvm::SmallVector<llvm::Metadata*, 4> retTypes;
        for (auto& ret : method.return_types) { retTypes.push_back(llvm::MDString::get(context, ret.value)); }
        if (method.is_volatile) {
            fn->addFnAttr(llvm::Attribute::NoInline);
            fn->addFnAttr(llvm::Attribute::OptimizeNone);
            fn->addFnAttr("noipa");
        }
        fn->setMetadata("qc.return_types", llvm::MDNode::get(context, retTypes));
        for (size_t i = 0; i < method.params.size(); i++) {
            if (method.params[i].type.value.starts_with("out ")) {
                fn->addParamAttr(i, llvm::Attribute::WriteOnly);
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            } else if (method.params[i].type.value.starts_with("inout ")) {
                fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
            }
            if (method.params[i].type.value.ends_with("restrict")) { fn->addParamAttr(i, llvm::Attribute::NoAlias); }
        }
        functionDefs[mangledName] = funcDefFromClassMethod(method, mangled_class_name);
    }
    for (size_t methodIdx = 0; methodIdx < classInfo.classMethods.size(); methodIdx++) {
        auto& method = classInfo.classMethods[methodIdx];
        if (method.is_static) continue;
        if (std::find(genericMethodIndices[baseTypeName(className)].begin(), genericMethodIndices[baseTypeName(className)].end(), methodIdx) !=
            genericMethodIndices[baseTypeName(className)].end()) {
            continue;
        }
        llvm::Function* fn = nullptr;
        auto& overloads = classMethods[mangled_class_name][method.name_tok.value];
        for (auto* overload : overloads) {
            if (overload->arg_size() - 1 == method.params.size()) {
                bool matches = true;
                for (size_t i = 0; i < method.params.size(); i++) {
                    auto& param = method.params[i];
                    llvm::Type* expectedType;
                    if (param.signature.has_value()) {
                        expectedType = llvm::PointerType::get(context, 0);
                    } else {
                        std::string resolvedType = resolveTypeName(param.type.value, false);
                        expectedType = llvmTypeFor(resolvedType);
                    }
                    llvm::Type* actualType = overload->getFunctionType()->getParamType(i + 1);
                    if (expectedType != actualType && param.type.value == "...") {
                        matches = false;
                        break;
                    }
                }
                if (matches) {
                    fn = overload;
                    break;
                }
            }
        }
        if (!fn || !fn->empty()) continue;
        std::string methodName = fn->getName().str();
        llvm::Function* currentTarget = fn;
        if (!method.modifiers.empty()) {
            std::string implName = "_impl_" + methodName;
            currentTarget = llvm::Function::Create(fn->getFunctionType(), llvm::Function::InternalLinkage, implName, module);
        }
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", currentTarget);
        builder->SetInsertPoint(entry);
        auto oldThis = currentThis;
        auto oldClassName = currentClassName;
        auto oldFunction = currentFunction;
        enterScope();
        currentThis = currentTarget->getArg(0);
        volatileVars["this"] = false;
        varTypes["this"] = mangled_class_name + "*";
        currentClassName = mangled_class_name;
        currentFunction = currentTarget;
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
        if (method.is_constructor && !classInfo.baseClassName.empty()) {
            if (method.parentConstructorCall) {
                if (auto varAccess = std::get_if<VarAccessNode*>(&method.parentConstructorCall->node_to_call)) {
                    std::string callName = (*varAccess)->var_name_tok.value;
                    if (callName == classInfo.baseClassName) {
                        std::vector<llvm::Value*> parentArgs;
                        for (auto& argNode : method.parentConstructorCall->arg_nodes) {
                            llvm::Value* arg = emitExpr(argNode);
                            if (!arg) continue;
                            parentArgs.push_back(arg);
                        }
                        llvm::Function* parentCtor = findMethodOverload(classInfo.baseClassName, classInfo.baseClassName, parentArgs);
                        if (parentCtor) {
                            std::vector<llvm::Value*> allArgs = {currentThis};
                            allArgs.insert(allArgs.end(), parentArgs.begin(), parentArgs.end());
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                builder->CreateInvoke(parentCtor, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                            } else {
                                builder->CreateCall(parentCtor, allArgs);
                            }
                        } else {
                            Position pos = get_pos(method.parentConstructorCall);
                            cg_error(pos, "parent class `" + classInfo.baseClassName + "` has no matching constructor", "QC-S111");
                            addConstructorNotes(classInfo.baseClassName, parentArgs, pos);
                        }
                    }
                }
            }
        }
        if (method.body) { emitStmt(method.body); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            if (currentTarget->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateRet(llvm::Constant::getNullValue(currentTarget->getReturnType()));
            }
        }
        currentThis = oldThis;
        currentClassName = oldClassName;
        currentFunction = oldFunction;
        exitScope();
        if (!method.modifiers.empty()) {
            llvm::FunctionType* fTy = fn->getFunctionType();
            for (int i = (int)method.modifiers.size() - 1; i >= 0; --i) {
                Token modTok = method.modifiers[i];
                if (!modifiers.count(modTok.value)) {
                    cg_error(modTok.pos, "unknown modifier '" + modTok.value + "'", "QC-S112");
                    addTypeNotes(modTok.value, modTok.pos, UserTypeKind::Modifier);
                    continue;
                }
                ModifierInfo& modInfo = modifiers[modTok.value];
                bool isOutermost = (i == 0);
                std::string layerName = isOutermost ? methodName : ("_mod_" + std::to_string(i) + "_" + methodName);
                auto linkage = isOutermost ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
                std::string proceedName = "_proceed_" + std::to_string(i) + "_" + methodName;
                llvm::Function* proceedFunc = synthesizeProceed(proceedName, currentTarget, modInfo.onReturn,
                                                                *funcDefFromClassMethod(method, mangled_class_name, "_"));
                llvm::Function* layerFunc = isOutermost ? fn : module->getFunction(layerName);
                if (!layerFunc) { layerFunc = llvm::Function::Create(fTy, linkage, layerName, module); }
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
                varTypes["this"] = mangled_class_name + "*";
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
                    if (fTy->getReturnType()->isVoidTy()) {
                        builder->CreateCall(proceedFunc, forwardArgs);
                        builder->CreateRetVoid();
                    } else {
                        llvm::Value* retVal = builder->CreateCall(proceedFunc, forwardArgs);
                        builder->CreateRet(retVal);
                    }
                }
                if (!builder->GetInsertBlock()->getTerminator()) {
                    if (fTy->getReturnType()->isVoidTy()) {
                        builder->CreateRetVoid();
                    } else {
                        builder->CreateRet(llvm::ConstantAggregateZero::get(fTy->getReturnType()));
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
    }
    proveConceptsForTypeInfo(mangled_class_name, classInfo);
    namespaceStack = oldNamespaceStack;
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    if (savedBlock) { builder->SetInsertPoint(savedBlock, savedPoint); }
    inProgressGenerics.erase(mangled_class_name);
    return classTy;
}
llvm::StructType* LLVMCompiler::generateGenericStruct(std::string structName, UserTypeInfo structInfo, std::vector<std::string> genericParams) {
    for (std::string param : genericParams) {
        std::string resolved = resolveTypeName(param, false);
        std::string base = baseTypeName(resolved);
        if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
            generateClass(base, userTypes.at(base));
        } else if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
            generateStruct(base, userTypes.at(base));
        } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
            generateEnum(resolved, userTypes.at(resolved));
        }
    }
    std::string mangled_struct_name = structName + "<";
    for (int j = 0; j < structInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = structInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_struct_name += val;
        if (j != structInfo.generics.size() - 1) { mangled_struct_name += ","; }
    }
    mangled_struct_name += ">";
    if (inProgressGenerics.count(mangled_struct_name)) return nullptr;
    if (structTypes.find(mangled_struct_name) != structTypes.end()) { return genericiseOrFindStruct(mangled_struct_name); }
    llvm::StructType* structTy = getOrCreateStructType(mangled_struct_name);
    structTypes[mangled_struct_name] = structTy;
    inProgressGenerics.insert(mangled_struct_name);
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(structInfo.generics, genericParams, structInfo.pos)) {
        this->currentGenericTypes = oldGenericTypes;
        currentGenericTypeStrings = oldGenericTypeStrings;
        currentNonTypeGenericValues = oldNonTypeGenerics;
        return nullptr;
    }
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!structInfo.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = structInfo.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(structInfo.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(structInfo.namespace_path.substr(start));
    }
    std::vector<llvm::Type*> fieldTypes;
    for (auto& field : structInfo.fields) {
        if (!resolveTypeName(field.type, false).ends_with("*")) {
            std::string resolved = resolveTypeName(field.type, false);
            if (!resolved.contains("<")) {
                if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                    generateStruct(resolved, userTypes.at(resolved));
                } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                    generateClass(resolved, userTypes.at(resolved));
                } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                    generateEnum(resolved, userTypes.at(resolved));
                }
            }
        }
        llvm::Type* ty = llvmTypeFor(field.type);
        fieldTypes.push_back(ty);
    }
    proveConceptsForTypeInfo(mangled_struct_name, structInfo);
    if (structTypes[mangled_struct_name]->isOpaque()) { structTypes[mangled_struct_name]->setBody(fieldTypes); }
    namespaceStack = oldNamespaceStack;
    inProgressGenerics.erase(mangled_struct_name);
    if (this->config.use_runtime) { generateStructReprFunction(mangled_struct_name, structInfo); }
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    return structTy;
}
llvm::StructType* LLVMCompiler::generateGenericEnum(std::string enumName, UserTypeInfo enumInfo, std::vector<std::string> genericParams) {
    std::string mangled_enum_name = enumName + "<";
    for (int j = 0; j < enumInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = enumInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_enum_name += val;
        if (j != enumInfo.generics.size() - 1) { mangled_enum_name += ","; }
    }
    mangled_enum_name += ">";
    if (inProgressGenerics.count(mangled_enum_name)) return {};
    if (enumTypes.find(mangled_enum_name) != enumTypes.end()) { return genericiseOrFindEnum(mangled_enum_name); }
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!enumInfo.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = enumInfo.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(enumInfo.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(enumInfo.namespace_path.substr(start));
    }
    llvm::StructType* enumTy = getOrCreateStructType(mangled_enum_name);
    inProgressGenerics.insert(mangled_enum_name);
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(enumInfo.generics, genericParams, enumInfo.pos)) return nullptr;
    enumTy->setBody(getLargestDiscriminantType(enumInfo));
    enumTypes[mangled_enum_name] = enumTy;
    for (size_t i = 0; i < enumInfo.enumEntries.size(); i++) {
        auto entry = enumInfo.enumEntries[i];
        std::string fullName = mangled_enum_name + "." + entry.memberName;
        for (std::string& tag : entry.tags) { tag = resolveTypeName(substituteGenerics(tag), false); }
        enumMemberInfo[fullName] = std::make_pair(entry.value, entry.tags);
    }
    proveConceptsForTypeInfo(mangled_enum_name, enumInfo);
    namespaceStack = oldNamespaceStack;
    inProgressGenerics.erase(mangled_enum_name);
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    return enumTy;
}
UserTypeInfo LLVMCompiler::generateGenericUnion(std::string unionName, UserTypeInfo unionInfo, std::vector<std::string> genericParams) {
    std::string mangled_union_name = unionName + "<";
    for (int j = 0; j < unionInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = unionInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_union_name += val;
        if (j != unionInfo.generics.size() - 1) { mangled_union_name += ","; }
    }
    mangled_union_name += ">";
    if (inProgressGenerics.count(mangled_union_name)) return {};
    if (unionTypes.find(mangled_union_name) != unionTypes.end()) { return genericiseOrFindUnion(mangled_union_name); }
    std::vector<llvm::Type*> fields = {builder->getInt32Ty(), llvm::PointerType::get(context, 0)};
    llvm::StructType* unionTy = getOrCreateStructType(fields, mangled_union_name);
    unionTypes[mangled_union_name] = unionTy;
    inProgressGenerics.insert(mangled_union_name);
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(unionInfo.generics, genericParams, unionInfo.pos)) return {};
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!unionInfo.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = unionInfo.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(unionInfo.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(unionInfo.namespace_path.substr(start));
    }
    UserTypeInfo genericisedInfo;
    genericisedInfo.kind = UserTypeKind::Union;
    for (UnionMember member : unionInfo.members) {
        if (currentNonTypeGenericValues.count(member.type)) {
            member.type = std::find_if(unionInfo.generics.begin(), unionInfo.generics.end(), [&](const auto& p) { return p.name == member.type; })
                              ->nonTypeKind +
                          ":" + currentNonTypeGenericValues[member.type].name;
        }
        genericisedInfo.members.push_back({substituteGenerics(resolveTypeName(substituteGenerics(member.type)))});
    }
    proveConceptsForTypeInfo(mangled_union_name, unionInfo);
    substitutedUnions[mangled_union_name] = genericisedInfo;
    namespaceStack = oldNamespaceStack;
    inProgressGenerics.erase(mangled_union_name);
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    return genericisedInfo;
}
std::string LLVMCompiler::generateGenericAlias(std::string aliasName, UserTypeInfo aliasInfo, std::vector<std::string> genericParams) {
    std::string mangled_alias_name = aliasName + "<";
    for (int j = 0; j < aliasInfo.generics.size(); j++) {
        std::string val;
        if (genericParams.size() <= j) {
            val = aliasInfo.generics[j].defaultValue;
        } else {
            val = genericParams[j];
        }
        mangled_alias_name += val;
        if (j != aliasInfo.generics.size() - 1) { mangled_alias_name += ","; }
    }
    mangled_alias_name += ">";
    if (inProgressGenerics.count(mangled_alias_name)) return "";
    if (typeAliases.count(mangled_alias_name)) return typeAliases[mangled_alias_name];
    inProgressGenerics.insert(mangled_alias_name);
    auto oldGenericTypes = this->currentGenericTypes;
    auto oldGenericTypeStrings = currentGenericTypeStrings;
    auto oldNonTypeGenerics = currentNonTypeGenericValues;
    if (!fulfillsGenericConstraints(aliasInfo.generics, genericParams, aliasInfo.pos)) return "";
    proveConceptsForTypeInfo(mangled_alias_name, aliasInfo);
    std::string substituted = substituteGenerics(aliasInfo.aliasTarget);
    typeAliases[mangled_alias_name] = substituted;
    inProgressGenerics.erase(mangled_alias_name);
    this->currentGenericTypes = oldGenericTypes;
    this->currentGenericTypeStrings = oldGenericTypeStrings;
    currentNonTypeGenericValues = oldNonTypeGenerics;
    return substituted;
}
void LLVMCompiler::generateStruct(const std::string& mapKey, const UserTypeInfo& info) {
    if (!info.generics.empty()) return;
    if (!structTypes.count(mapKey) || !structTypes[mapKey]->isOpaque()) return;
    std::vector<llvm::Type*> fieldTypes;
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!info.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = info.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(info.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(info.namespace_path.substr(start));
    }
    for (auto& field : info.fields) {
        if (!resolveTypeName(field.type, false).ends_with("*")) {
            std::string resolved = resolveTypeName(field.type, false);
            if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                generateStruct(resolved, userTypes.at(resolved));
            } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                generateClass(resolved, userTypes.at(resolved));
            } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                generateEnum(resolved, userTypes.at(resolved));
            }
        }
        llvm::Type* ty = llvmTypeFor(field.type);
        fieldTypes.push_back(ty);
    }
    if (structTypes[mapKey]->isOpaque()) { structTypes[mapKey]->setBody(fieldTypes); }
    namespaceStack = oldNamespaceStack;
}
void LLVMCompiler::generateClass(const std::string& mapKey, const UserTypeInfo& info) {
    if (!info.generics.empty()) return;
    if (!classTypes.count(mapKey) || !classTypes[mapKey]->isOpaque()) return;
    if (!info.baseClassName.empty()) {
        auto base_it = userTypes.find(baseTypeName(info.baseClassName));
        if (base_it != userTypes.end() && base_it->second.is_final_class) {
            cg_error(info.pos, "cannot inherit from final class '" + info.baseClassName + "'", "QC-S113");
            cg_note(base_it->second.pos, "declared here", true);
            return;
        }
    }
    bool noMethods = false;
    bool noAccess = false;
    if (info.classMethods.empty()) noMethods = true;
    auto oldNamespaceStack = namespaceStack;
    namespaceStack.clear();
    if (!info.namespace_path.empty()) {
        size_t start = 0;
        size_t pos;
        while ((pos = info.namespace_path.find("::", start)) != std::string::npos) {
            namespaceStack.push_back(info.namespace_path.substr(start, pos - start));
            start = pos + 2;
        }
        namespaceStack.push_back(info.namespace_path.substr(start));
    }
    bool noParent = info.baseClassName.empty();
    std::vector<llvm::Type*> fieldTypes;
    bool hasVptr = false;
    std::function<void(const std::string&, std::unordered_map<std::string, std::string>)> collectFields =
        [&](const std::string& cname, std::unordered_map<std::string, std::string> genericSubs) {
            auto it = userTypes.find(baseTypeName(cname));
            if (it == userTypes.end()) { throw std::string("Class not found: ") + cname; }
            auto& classIfo = it->second;
            if (!classIfo.baseClassName.empty()) {
                std::string baseFullName = classIfo.baseClassName;
                for (auto& [gname, gval] : genericSubs) {
                    size_t pos = 0;
                    while ((pos = baseFullName.find(gname, pos)) != std::string::npos) {
                        size_t end = pos + gname.size();
                        bool leftOk = pos == 0 || !(std::isalnum(static_cast<unsigned char>(baseFullName[pos - 1])) || baseFullName[pos - 1] == '_');
                        bool rightOk = end == baseFullName.size() ||
                                       !(std::isalnum(static_cast<unsigned char>(baseFullName[end])) || baseFullName[end] == '_');
                        if (leftOk && rightOk) {
                            baseFullName.replace(pos, gname.size(), gval);
                            pos += gval.size();
                        } else {
                            pos += gname.size();
                        }
                    }
                }
                std::unordered_map<std::string, std::string> baseSubs;
                auto baseIt = userTypes.find(baseTypeName(baseFullName));
                if (baseIt != userTypes.end()) {
                    genericiseOrFindClass(baseFullName);
                    auto baseArgs = genericParamsFromName(baseFullName);
                    for (size_t i = 0; i < baseIt->second.generics.size() && i < baseArgs.size(); i++) {
                        baseSubs[baseIt->second.generics[i].name] = baseArgs[i];
                    }
                }
                collectFields(baseFullName, baseSubs);
            }
            bool noAccessFields = true;
            for (auto& field : classIfo.classFields) {
                std::string resolvedType = field.type;
                if (field.name == "__vptr") {
                    if (hasVptr) continue;
                    hasVptr = true;
                    resolvedType = cname + "*";
                } else {
                    if (field.access != "public") noAccessFields = false;
                    for (auto& [gname, gval] : genericSubs) {
                        size_t pos = 0;
                        while ((pos = resolvedType.find(gname, pos)) != std::string::npos) {
                            size_t end = pos + gname.size();
                            bool leftOk = pos == 0 ||
                                          !(std::isalnum(static_cast<unsigned char>(resolvedType[pos - 1])) || resolvedType[pos - 1] == '_');
                            bool rightOk = end == resolvedType.size() ||
                                           !(std::isalnum(static_cast<unsigned char>(resolvedType[end])) || resolvedType[end] == '_');
                            if (leftOk && rightOk) {
                                resolvedType.replace(pos, gname.size(), gval);
                                pos += gval.size();
                            } else {
                                pos += gname.size();
                            }
                        }
                    }
                }
                if (!resolveTypeName(resolvedType, false).ends_with("*")) {
                    std::string resolved = resolveTypeName(resolvedType, false);
                    if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                        generateStruct(resolved, userTypes.at(resolved));
                    } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                        generateClass(resolved, userTypes.at(resolved));
                    } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                        generateEnum(resolved, userTypes.at(resolved));
                    }
                }
                if (field.isStatic) {
                    llvm::Type* fieldTy = llvmTypeFor(resolvedType);
                    std::string mangledName = cname + "::" + field.name;
                    if (!module->getNamedGlobal(mangledName)) {
                        llvm::Constant* initVal = !std::holds_alternative<std::monostate>(field.defaultValue)
                                                      ? llvm::dyn_cast<llvm::Constant>(emitExpr(field.defaultValue))
                                                      : llvm::Constant::getNullValue(fieldTy);
                        if (!initVal) { initVal = llvm::Constant::getNullValue(fieldTy); }

                        globals[mangledName] = new llvm::GlobalVariable(*module, fieldTy, false, llvm::GlobalValue::ExternalLinkage, initVal,
                                                                        mangledName);
                        varTypes[mangledName] = resolvedType;
                        volatileVars[mangledName] = false;
                    }
                } else {
                    if (!std::holds_alternative<std::monostate>(field.defaultValue)) {
                        warn("default-member", get_pos(field.defaultValue), "default values do not exist on non-static members", "W003");
                    }
                    fieldTypes.push_back(llvmTypeFor(resolvedType));
                }
            }
            noAccess = noAccessFields;
        };
    collectFields(mapKey, {});
    if (fieldTypes.empty()) { fieldTypes.push_back(builder->getInt8Ty()); }
    if (noParent && noAccess && noMethods) {
        warn("struct-like-class", info.pos, "class has no access control, parent classes, or methods", "QC-W020");
        if (getWarningLevel("struct-like-class") != run::WarningLevel::Disabled && getWarningLevel("struct-like-class") != run::WarningLevel::None) {
            cg_help(info.pos, "consider using a struct instead");
            cg_insight("to keep code sensical and avoid unnecessary data or abstraction, C^4 structs remain POD and C^4 classes should only be used "
                       "when they require non-POD behavior.");
        }
    }
    if (classTypes[mapKey]->isOpaque()) { classTypes[mapKey]->setBody(fieldTypes); }
    namespaceStack = oldNamespaceStack;
}
void LLVMCompiler::createUserTypes() {
    auto getFullName = [](const std::string& name, const UserTypeInfo& info) {
        if (info.namespace_path.empty()) { return name; }
        return info.namespace_path + "::" + name;
    };
    for (auto& [mapKey, info] : userTypes) { add_type_warning(mapKey, info.pos); }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Concept) {
            if (!info.generics.empty()) {
                genericConcepts[mapKey] = true;
                continue;
            }
            genericConcepts[mapKey] = false;
            concepts[mapKey] = info.conceptInfo;
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Enum) {
            if (!info.generics.empty()) {
                genericEnums[mapKey] = true;
                continue;
            }
            genericEnums[mapKey] = false;
            enumTypes[mapKey] = getOrCreateStructType(mapKey);
            for (size_t i = 0; i < info.enumEntries.size(); i++) {
                auto entry = info.enumEntries[i];
                for (std::string& tag : entry.tags) { tag = resolveTypeName(substituteGenerics(tag), false); }
                std::string fullName = mapKey + "." + entry.memberName;
                enumMemberInfo[fullName] = std::make_pair(entry.value, entry.tags);
            }
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Struct) {
            if (!info.generics.empty()) {
                genericStructs[mapKey] = true;
                continue;
            }
            genericStructs[mapKey] = false;
            llvm::StructType* structTy = getOrCreateStructType(mapKey);
            structTypes[mapKey] = structTy;
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Class) {
            if (!info.baseClassName.empty()) {
                auto parentIfo = userTypes.find(baseTypeName(resolveTypeName(info.baseClassName)));
                if (parentIfo != userTypes.end()) {
                    UserTypeInfo parentInfo = parentIfo->second;
                    std::unordered_set<std::string> parentFields;
                    for (const auto& field : parentInfo.classFields) { parentFields.insert(field.name); }
                    for (const auto& field : info.classFields) {
                        if (!field.isStatic && field.name != "__vptr" && field.name != "__vptr" && parentFields.contains(field.name)) {
                            cg_error(info.pos, "Parent field " + field.name + " redeclared in child class.", "QC-CS02");
                        }
                    }
                }
            }
            if (!info.generics.empty()) {
                genericClasses[mapKey] = true;
                continue;
            }
            genericClasses[mapKey] = false;
            llvm::StructType* classTy = getOrCreateStructType(mapKey);
            classTypes[mapKey] = classTy;
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Class) { generateClass(mapKey, info); }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Enum) { generateEnum(mapKey, info); }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Union) {
            if (!info.generics.empty()) {
                genericUnions[mapKey] = true;
                continue;
            }
            genericUnions[mapKey] = false;
            std::vector<llvm::Type*> fields = {builder->getInt32Ty(), llvm::PointerType::get(context, 0)};
            llvm::StructType* unionTy = getOrCreateStructType(fields, mapKey);
            unionTypes[mapKey] = unionTy;
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Alias) {
            if (!info.generics.empty()) {
                genericAliases[mapKey] = true;
                continue;
            }
            genericAliases[mapKey] = false;
            typeAliases[mapKey] = info.aliasTarget;
        }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind != UserTypeKind::Class) continue;
        if (!info.generics.empty()) continue;
        if (!info.baseClassName.empty()) {
            auto base_it = userTypes.find(info.baseClassName);
            if (base_it != userTypes.end()) {
                auto& baseInfo = base_it->second;
                for (auto& method : info.classMethods) {
                    for (auto& baseMethod : baseInfo.classMethods) {
                        if (baseMethod.name_tok.value == method.name_tok.value && baseMethod.is_final) {
                            cg_error(method.name_tok.pos,
                                     "Cannot override final method '" + baseMethod.name_tok.value + "' from base class '" + info.baseClassName + "'",
                                     "QC-S097");
                        }
                    }
                }
            }
        }
        auto oldNamespaceStack = namespaceStack;
        namespaceStack.clear();
        if (!info.namespace_path.empty()) {
            size_t start = 0;
            size_t pos;
            while ((pos = info.namespace_path.find("::", start)) != std::string::npos) {
                namespaceStack.push_back(info.namespace_path.substr(start, pos - start));
                start = pos + 2;
            }
            namespaceStack.push_back(info.namespace_path.substr(start));
        }
        llvm::StructType* classTy = genericiseOrFindClass(mapKey);
        std::vector<llvm::Constant*> vtableFuncs;
        std::vector<std::string> slotOrder;
        std::unordered_map<std::string, int> nameCounts;
        for (auto& method : info.classMethods) { nameCounts[method.name_tok.value]++; }
        for (auto& [mapKey, info] : userTypes) {
            if (info.kind != UserTypeKind::Class || !info.generics.empty()) continue;
            for (size_t methodIdx = 0; methodIdx < info.classMethods.size(); methodIdx++) {
                auto& method = info.classMethods[methodIdx];
                if (!method.is_static) continue;
                std::string mangledName = mapKey + "::" + method.name_tok.value;
                if (nameCounts[method.name_tok.value] > 1) {
                    for (auto& param : method.params) { mangledName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
                }
                llvm::FunctionType* fnTy = llvmFuncTypeFor(method.return_types, method.params);
                llvm::Function* fn = module->getFunction(mangledName);
                if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, module); }
                llvm::SmallVector<llvm::Metadata*, 4> retTypes;
                for (auto& ret : method.return_types) { retTypes.push_back(llvm::MDString::get(context, ret.value)); }
                if (method.is_volatile) {
                    fn->addFnAttr(llvm::Attribute::NoInline);
                    fn->addFnAttr(llvm::Attribute::OptimizeNone);
                    fn->addFnAttr("noipa");
                }
                fn->setMetadata("qc.return_types", llvm::MDNode::get(context, retTypes));
                for (size_t i = 0; i < method.params.size(); i++) {
                    if (method.params[i].type.value.starts_with("out ")) {
                        fn->addParamAttr(i, llvm::Attribute::WriteOnly);
                        fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
                    } else if (method.params[i].type.value.starts_with("inout ")) {
                        fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
                    }
                    if (method.params[i].type.value.ends_with("restrict")) { fn->addParamAttr(i, llvm::Attribute::NoAlias); }
                }
                functionDefs[mangledName] = funcDefFromClassMethod(method, mapKey);
            }
        }
        for (size_t methodIdx = 0; methodIdx < info.classMethods.size(); methodIdx++) {
            auto& method = info.classMethods[methodIdx];
            if (method.is_static) continue;
            if (method.is_constructor && info.is_abstract_class) {
                cg_error(method.name_tok.pos, "cannot make a constructor on a abstract class.", "QC-S110");
                continue;
            }
            if (std::find(genericMethodIndices[mapKey].begin(), genericMethodIndices[mapKey].end(), methodIdx) !=
                genericMethodIndices[mapKey].end()) {
                continue;
            }
            std::string methodName = mapKey + "_" + method.name_tok.value;
            if (nameCounts[method.name_tok.value] > 1) {
                for (auto& param : method.params) { methodName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
            }
            std::vector<llvm::Type*> paramTypes;
            paramTypes.push_back(llvm::PointerType::get(context, 0));
            llvm::FunctionType* baseFuncTy = llvmFuncTypeFor(method.return_types, method.params);
            for (auto* paramTy : baseFuncTy->params()) { paramTypes.push_back(paramTy); }
            llvm::FunctionType* fnTy = llvm::FunctionType::get(baseFuncTy->getReturnType(), paramTypes, false);
            llvm::Function* fn = module->getFunction(methodName);
            if (!fn) { fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, methodName, module); }
            llvm::SmallVector<llvm::Metadata*, 4> retTypes;
            for (auto& ret : method.return_types) { retTypes.push_back(llvm::MDString::get(context, ret.value)); }
            if (method.is_volatile) {
                fn->addFnAttr(llvm::Attribute::NoInline);
                fn->addFnAttr(llvm::Attribute::OptimizeNone);
                fn->addFnAttr("noipa");
            }
            fn->setMetadata("qc.return_types", llvm::MDNode::get(context, retTypes));
            for (int i = 1; i < fnTy->getNumParams(); i++) {
                if (method.params[i - 1].type.value.starts_with("out ")) {
                    fn->addParamAttr(i, llvm::Attribute::WriteOnly);
                    fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
                } else if (method.params[i - 1].type.value.starts_with("inout ")) {
                    fn->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
                }
                if (method.params[i - 1].type.value.ends_with("restrict")) { fn->addParamAttr(i, llvm::Attribute::NoAlias); }
            }
            classMethods[mapKey][method.name_tok.value].push_back(fn);
            vtableFuncs.push_back(fn);
            slotOrder.push_back(methodName);
        }
        for (size_t i = 0; i < slotOrder.size(); i++) { vtableSlotIndex[mapKey][slotOrder[i]] = i; }
        auto* arrTy = llvm::ArrayType::get(llvm::PointerType::get(context, 0), vtableFuncs.size());
        auto* vtableInit = llvm::ConstantArray::get(arrTy, vtableFuncs);
        auto* vtable = getOrCreateVtable(mapKey + "_vtable", arrTy, vtableInit);
        vtables[mapKey] = vtable;
        namespaceStack = oldNamespaceStack;
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind == UserTypeKind::Struct) { generateStruct(mapKey, info); }
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind != UserTypeKind::Concept || !info.generics.empty()) continue;
        proversFromConceptInfo(mapKey, info);
    }
    for (auto& [mapKey, info] : userTypes) {
        if (info.kind != UserTypeKind::Modifier) continue;
        ModifierInfo res;
        for (auto& [hookTok, bodyNode] : info.modifierInfo.handlers) {
            if (hookTok.value == "on_call") {
                res.onCall = bodyNode;
            } else if (hookTok.value == "on_return") {
                res.onReturn = bodyNode;
            } else if (hookTok.value == "on_use") {
                res.onUse = bodyNode;
            } else {
                cg_error(hookTok.pos, "unknown modifier handler '" + hookTok.value + "'", "QC-S114");
                cg_note(hookTok.pos, "valid modifier handlers are on_call, on_return and on_use");
                return;
            }
        }
        modifiers[mapKey] = res;
    }
    if (this->config.use_runtime) { generateStructReprFunctions(); }
}
ParamTypeInfo toTypeInfo(const Parameter& p) {
    ParamTypeInfo out;

    out.type = p.type;
    out.name = p.name;
    if (p.signature.has_value()) {
        ParamTypeInfo::FunctionSignature sig;

        sig.return_types = p.signature->return_types;

        sig.params.reserve(p.signature->params.size());
        for (const auto& sub : p.signature->params) { sig.params.push_back(toTypeInfo(sub)); }

        out.signature = sig;
    }

    return out;
}
llvm::FunctionType* LLVMCompiler::llvmFuncTypeForHelper(const std::vector<Token>& returnTypes, const std::vector<ParamTypeInfo>& params) {
    std::vector<llvm::Type*> paramTypes;
    bool is_c_varargs = false;
    for (auto& p : params) {
        if (p.signature.has_value()) {
            paramTypes.push_back(llvm::PointerType::getUnqual(context));
        } else {
            if (p.type.type == TokenType::VARADIC) {
                if (p.name.value == "<varadic>") {
                    is_c_varargs = true;
                } else {
                    paramTypes.push_back(llvmTypeFor(p.type.value));
                }
                break;
            } else {
                std::string toType = p.type.value;
                while (toType.starts_with("out ") || toType.starts_with("inout ")) { toType.erase(0, toType.find(' ') + 1); }
                if (toType.ends_with("restrict")) { toType = toType.substr(0, toType.length() - 8); }
                paramTypes.push_back(llvmTypeFor(toType));
                if (module->getDataLayout().getTypeAllocSize(paramTypes.back()) > 16) {
                    warn("large-by-value", p.type.pos, "large type " + toType + " passed by value", "QC-W019");
                }
            }
        }
    }
    if (returnTypes.empty()) {
        llvm::FunctionType* funcTy = llvm::FunctionType::get(builder->getVoidTy(), paramTypes, is_c_varargs);
        return funcTy;
    }

    if (returnTypes.size() == 1) {
        llvm::Type* retTy = llvmTypeFor(returnTypes[0].value);
        if (!retTy) {
            cg_error(returnTypes[0].pos, "Failed to get return-type");
            return nullptr;
        }
        if (retTy->isArrayTy()) { retTy = llvm::PointerType::get(context, 0); }

        return llvm::FunctionType::get(retTy, paramTypes, is_c_varargs);
    }

    std::vector<llvm::Type*> retTypes;
    for (auto& rt : returnTypes) {
        llvm::Type* ty = llvmTypeFor(rt.value);
        if (ty->isArrayTy()) { ty = llvm::PointerType::get(context, 0); }

        retTypes.push_back(ty);
    }
    llvm::StructType* structTy = llvm::StructType::get(context, retTypes);
    return llvm::FunctionType::get(structTy, paramTypes, is_c_varargs);
}
llvm::FunctionType* LLVMCompiler::llvmFuncTypeFor(const std::vector<Token>& returnTypes, const std::list<Parameter>& params) {
    std::vector<ParamTypeInfo> converted;

    for (const auto& p : params) { converted.push_back(toTypeInfo(p)); }

    return llvmFuncTypeForHelper(returnTypes, converted);
}
llvm::FunctionType* LLVMCompiler::llvmFuncTypeFor(const std::vector<Token>& returnTypes, const std::vector<Parameter>& params) {
    std::vector<ParamTypeInfo> converted;

    for (const auto& p : params) { converted.push_back(toTypeInfo(p)); }

    return llvmFuncTypeForHelper(returnTypes, converted);
}
void LLVMCompiler::cg_error(const Position& pos, const std::string& msg, std::string code) {
    errors.emplace_back(msg, pos, false, code);
}
void LLVMCompiler::cg_warn(const Position& pos, const std::string& msg, std::string code) {
    errors.emplace_back(msg, pos, true, code);
}
LLVMCompiler::LLVMCompiler(std::unordered_map<std::string, UserTypeInfo>& userTys, llvm::Module* mod, llvm::LLVMContext& ctx, bool is_main)
    : userTypes(userTys), context(ctx) {
    module = (mod == nullptr ? new llvm::Module("qc_module", context) : mod);
    builder = new llvm::IRBuilder<>(context);
    pointerSizeBits = module->getDataLayout().getPointerSizeInBits();
    arrayTypeStringsStack.push_back({});
    arrayLengthsStack.push_back({});
    this->is_main = is_main;
}

llvm::Value* LLVMCompiler::boolToQBool(llvm::Value* boolVal) {
    llvm::Value* ext = builder->CreateZExt(boolVal, builder->getInt8Ty());
    llvm::Value* tripled = builder->CreateMul(ext, builder->getInt8(3));
    return builder->CreateTrunc(tripled, builder->getIntNTy(2));
}
llvm::Value* LLVMCompiler::emitMethodCall(llvm::Function* method, llvm::Value* thisPtr, const std::vector<llvm::Value*>& args,
                                          const std::string& name) {
    auto callArgs = reconcileArgs(method, thisPtr, args);
    bool returnsVoid = method->getReturnType()->isVoidTy();
    if (insideTry()) {
        auto* contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
        auto* invoke = builder->CreateInvoke(method, contBB, currentLandingPad(), callArgs);
        if (!returnsVoid) { invoke->setName(name + "_result"); }
        builder->SetInsertPoint(contBB);
        return returnsVoid ? nullptr : invoke;
    }
    auto* call = builder->CreateCall(method, callArgs);
    if (!returnsVoid) { call->setName(name + "_result"); }
    return returnsVoid ? nullptr : call;
}
llvm::Value* LLVMCompiler::emitBinOp(BinOpNode* const* bin) {
    TokenType op = (*bin)->op_tok.type;
    if (op == TokenType::KEYWORD) {
        if ((*bin)->op_tok.value == "proved_by") {
            std::string lType = getExpressionType((*bin)->left_node);
            std::string rType = getExpressionType((*bin)->right_node);
            if (!userTypes.count(lType) || userTypes[lType].kind != UserTypeKind::Concept) {
                cg_error(get_pos((*bin)->left_node), "No such concept `" + lType + "`", "QC-C018");
                if (!userTypes.count(lType)) addTypeNotes(lType, get_pos((*bin)->left_node), UserTypeKind::Concept);
                return nullptr;
            }
            return builder->getInt1(std::ranges::any_of(userTypes[lType].provees, [&](const ConceptProvee& provedConcept) {
                return resolveTypeName(provedConcept.conceptName.value, false) == resolveTypeName(lType, false);
            }));
        }
    }
    if (op == TokenType::AS) {
        llvm::Value* value = emitExpr((*bin)->left_node);
        if (!value) return nullptr;
        value = normalizeValue(value, (*bin)->left_node);
        auto* typeNode = std::get_if<TypeValueNode>(&(*bin)->right_node);
        if (!typeNode) return nullptr;
        llvm::Type* dstTy = llvmTypeFor(typeNode->tok.value);
        if (!dstTy) return nullptr;
        llvm::Type* srcTy = value->getType();
        if (srcTy == dstTy) return value;
        bool srcSigned = std::unordered_set<std::string>({"addr_t", "byte", "nibble"}).contains(getExpressionType((*bin)->left_node));
        bool dstSigned = std::unordered_set<std::string>({"addr_t", "byte", "nibble"}).contains(typeNode->tok.value);
        if (srcTy->isIntegerTy() && dstTy->isIntegerTy()) {
            unsigned srcBits = srcTy->getIntegerBitWidth();
            unsigned dstBits = dstTy->getIntegerBitWidth();
            if (dstBits > srcBits) return srcSigned ? builder->CreateSExt(value, dstTy) : builder->CreateZExt(value, dstTy);
            if (dstBits < srcBits) return builder->CreateTrunc(value, dstTy);
            return value;
        }
        if (srcTy->isIntegerTy() && dstTy->isFloatingPointTy())
            return srcSigned ? builder->CreateSIToFP(value, dstTy) : builder->CreateUIToFP(value, dstTy);
        if (srcTy->isFloatingPointTy() && dstTy->isIntegerTy())
            return dstSigned ? builder->CreateFPToSI(value, dstTy) : builder->CreateFPToUI(value, dstTy);
        if (srcTy->isFloatingPointTy() && dstTy->isFloatingPointTy())
            return dstTy->getPrimitiveSizeInBits() > srcTy->getPrimitiveSizeInBits() ? builder->CreateFPExt(value, dstTy)
                                                                                     : builder->CreateFPTrunc(value, dstTy);
        if (srcTy->isPointerTy() && dstTy->isPointerTy()) return builder->CreateBitCast(value, dstTy);
        if (srcTy->isPointerTy() && dstTy->isIntegerTy()) return builder->CreatePtrToInt(value, dstTy);
        if (srcTy->isIntegerTy() && dstTy->isPointerTy()) return builder->CreateIntToPtr(value, dstTy);
        return nullptr;
    }
    if (op == TokenType::RSHIFT) {
        llvm::Value* leftResult = nullptr;
        if (startsWithQIn((*bin)->left_node)) {
            if (auto leftBin = std::get_if<BinOpNode*>(&(*bin)->left_node)) {
                if ((*leftBin)->op_tok.type == TokenType::RSHIFT) {
                    while (true) {}
                    leftResult = emitExpr((*bin)->left_node);
                }
            }
            llvm::Function* qinFn = module->getFunction("qc_qin");
            if (!qinFn) {
                auto* fnTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {}, false);
                qinFn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qin", module);
            }

            llvm::Value* input = builder->CreateCall(qinFn, {}, "qin_input");

            if (auto varAccess = std::get_if<VarAccessNode*>(&(*bin)->right_node)) {
                std::string varName = (*varAccess)->var_name_tok.value;
                llvm::Value* alloc = getVarAddress(varName);
                if (!alloc) {
                    Position pos = get_pos(*varAccess);
                    cg_error(pos, "qin: variable not declared: " + varName, "QC-S117");
                    auto suggestions = getVisibleVariables();
                    std::vector<std::pair<int, std::string>> matches;
                    if (varName.size() >= 3) {
                        for (auto& name : suggestions) {
                            int distance = levenshteinDistance(varName, name);
                            if (distance <= 2) { matches.push_back({distance, name}); }
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
                    return nullptr;
                }
                llvm::Type* varTy = getPointeeType(varName);
                llvm::Value* converted = input;
                if (varTy->isIntegerTy(32)) {
                    llvm::Function* fn = module->getFunction("qc_to_int_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getInt32Ty(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_int_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(16)) {
                    llvm::Function* fn = module->getFunction("qc_to_short_int_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getInt16Ty(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_short_int_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(64)) {
                    llvm::Function* fn = module->getFunction("qc_to_long_int_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getIntNTy(getPtrSize()), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_long_int_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isFloatTy()) {
                    llvm::Function* fn = module->getFunction("qc_to_float_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getFloatTy(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_float_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isDoubleTy()) {
                    llvm::Function* fn = module->getFunction("qc_to_double_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getDoubleTy(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_double_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(8)) {
                    llvm::Function* fn = module->getFunction("qc_to_char_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getInt8Ty(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_char_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(1)) {
                    llvm::Function* fn = module->getFunction("qc_to_bool_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_bool_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(2)) {
                    llvm::Function* fn = module->getFunction("qc_to_qbool_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getIntNTy(2), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_qbool_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                } else if (varTy->isIntegerTy(4)) {
                    llvm::Function* fn = module->getFunction("qc_to_nibble_from_string");
                    if (!fn) {
                        auto* fnTy = llvm::FunctionType::get(builder->getIntNTy(4), {llvm::PointerType::get(context, 0)}, false);
                        fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_to_nibble_from_string", module);
                    }
                    converted = builder->CreateCall(fn, {input});
                }

                builder->CreateStore(converted, alloc);

                return builder->getInt32(0);
            }

            cg_error(get_pos((*bin)->right_node), "qin: right side must be a variable", "QC-S118");
            return nullptr;
        }
    }
    llvm::Value* L = emitExpr((*bin)->left_node);
    if (!L) return nullptr;
    if (op == TokenType::AND || op == TokenType::OR) {
        L = toTruthiness(L, get_pos((*bin)->left_node));
        if (L) {
            llvm::BasicBlock* lhsBB = builder->GetInsertBlock();
            llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(context, op == TokenType::AND ? "and.rhs" : "or.rhs", currentFunction);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, op == TokenType::AND ? "and.end" : "or.end", currentFunction);
            if (op == TokenType::AND) {
                builder->CreateCondBr(L, rhsBB, endBB);
            } else {
                builder->CreateCondBr(L, endBB, rhsBB);
            }
            builder->SetInsertPoint(rhsBB);
            llvm::Value* R = emitExpr((*bin)->right_node);
            if (R) {
                R = toTruthiness(R, Position(Position::INVALID_FILE_ID, 0, 0, 0));
                if (R) {
                    llvm::BasicBlock* rhsEndBB = builder->GetInsertBlock();
                    builder->CreateBr(endBB);
                    builder->SetInsertPoint(endBB);
                    llvm::PHINode* result = builder->CreatePHI(builder->getInt1Ty(), 2, op == TokenType::AND ? "and" : "or");
                    if (op == TokenType::AND) {
                        result->addIncoming(builder->getFalse(), lhsBB);
                        result->addIncoming(R, rhsEndBB);
                    } else {
                        result->addIncoming(builder->getTrue(), lhsBB);
                        result->addIncoming(R, rhsEndBB);
                    }
                    return result;
                }
            }
        }
    }
    llvm::Value* R = emitExpr((*bin)->right_node);
    if (!R) return nullptr;
    llvm::Type* lty = L->getType();
    llvm::Type* rty = R->getType();
    if (L->getType()->isPointerTy()) {
        llvm::Type* allocTy = llvmTypeFor(getExpressionType((*bin)->left_node));
        if (allocTy)
            if (auto structTy = llvm::dyn_cast<llvm::StructType>(allocTy)) {
                if (structTy->hasName()) {
                    std::string className = structTy->getName().str();
                    if (classTypes.find(className) != classTypes.end()) {
                        std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);
                        if (!opMethodName.empty()) {
                            std::vector<llvm::Value*> args = {R};
                            llvm::Function* opMethod = findMethodOverload(className, opMethodName, args);
                            if (opMethod) {
                                std::vector<llvm::Value*> allArgs = {L, R};
                                if (insideTry()) {
                                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++),
                                                                           currentFunction);
                                    auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                    builder->SetInsertPoint(contBB);
                                    return invk;
                                }
                                return builder->CreateCall(opMethod, allArgs, "op_result");
                            }
                        }
                    } else if (auto it = userTypes.find(className);
                               it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                        std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);
                        if (!opMethodName.empty()) {
                            auto fit = functions.find(className + "_" + opMethodName);
                            if (fit != functions.end()) {
                                llvm::Function* opMethod = fit->second;
                                std::vector<llvm::Value*> allArgs = {L, R};
                                if (insideTry()) {
                                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++),
                                                                           currentFunction);
                                    auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                    builder->SetInsertPoint(contBB);
                                    return invk;
                                }
                                return builder->CreateCall(opMethod, allArgs, "op_result");
                            }
                        }
                    }
                }
            }
    }
    if (R->getType()->isPointerTy()) {
        llvm::Type* allocTy = llvmTypeFor(getExpressionType((*bin)->right_node));
        if (auto structTy = llvm::dyn_cast<llvm::StructType>(allocTy)) {
            if (structTy->hasName()) {
                std::string className = structTy->getName().str();
                if (classTypes.find(className) != classTypes.end()) {
                    std::string opMethodName = getRoperatorMethodName((*bin)->op_tok.type);
                    if (!opMethodName.empty()) {
                        std::vector<llvm::Value*> args = {L};
                        llvm::Function* opMethod = findMethodOverload(className, opMethodName, args);
                        if (opMethod) {
                            std::vector<llvm::Value*> allArgs = {R, L};
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                                return invk;
                            }
                            return builder->CreateCall(opMethod, allArgs, "op_result");
                        }
                    }
                } else if (auto it = userTypes.find(className);
                           it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                    std::string opMethodName = getRoperatorMethodName((*bin)->op_tok.type);
                    if (!opMethodName.empty()) {
                        auto fit = functions.find(className + "_" + opMethodName);
                        if (fit != functions.end()) {
                            llvm::Function* opMethod = fit->second;
                            std::vector<llvm::Value*> allArgs = {L, R};
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                                return invk;
                            }
                            return builder->CreateCall(opMethod, allArgs, "op_result");
                        }
                    }
                }
            }
        }
    }
    bool isEq = (*bin)->op_tok.type == TokenType::EQ_TO;
    bool isNe = (*bin)->op_tok.type == TokenType::NOT_EQ;

    if (isEq || isNe) {
        llvm::Type* lTy = L->getType();
        llvm::Type* rTy = R->getType();

        std::string lUnionName, rUnionName;
        bool lIsUnion = isUnionType(lTy, &lUnionName);
        bool rIsUnion = isUnionType(rTy, &rUnionName);
        if (lIsUnion && !rIsUnion) {
            auto match = matchValueToUnionVariant(lUnionName, (*bin)->right_node, R);

            if (!match) {
                llvm::Value* res = builder->getFalse();
                if (isNe) res = builder->CreateNot(res);
                return res;
            }

            auto info = *match;
            llvm::Value* tag = builder->CreateExtractValue(L, 0, "union_tag");
            llvm::Value* dataPtr = builder->CreateExtractValue(L, 1, "union_data");

            llvm::Value* tagMatch = builder->CreateICmpEQ(tag, builder->getInt32(info.tagIndex), "union_tag_match");
            llvm::BasicBlock* matchBB = llvm::BasicBlock::Create(context, "tag_matches", currentFunction);
            llvm::BasicBlock* mismatchBB = llvm::BasicBlock::Create(context, "tag_mismatch", currentFunction);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "cmp_end", currentFunction);

            builder->CreateCondBr(tagMatch, matchBB, mismatchBB);

            builder->SetInsertPoint(matchBB);
            llvm::Value* payloadMatch = nullptr;

            if (!info.memberTypeStr.empty()) {
                llvm::Type* memberTy = llvmTypeFor(info.memberTypeStr);

                if (memberTy->isPointerTy()) {
                    llvm::Value* payload = builder->CreateBitCast(dataPtr, memberTy);

                    llvm::Function* strcmp_fn = module->getFunction("qc_string_eq");
                    if (!strcmp_fn) {
                        auto* i8Ptr = llvm::PointerType::get(context, 0);
                        auto* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {i8Ptr, i8Ptr}, false);
                        strcmp_fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_string_eq", module);
                    }
                    payloadMatch = builder->CreateCall(strcmp_fn, {payload, R}, "payload_str_eq");
                } else {
                    llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                    llvm::Value* payload = builder->CreateLoad(memberTy, typedPtr, "union_payload");

                    if (memberTy->isIntegerTy()) {
                        payloadMatch = builder->CreateICmpEQ(payload, R, "union_int_eq");
                    } else if (memberTy->isFloatingPointTy()) {
                        payloadMatch = builder->CreateFCmpOEQ(payload, R, "union_fp_eq");
                    }
                }
            }

            llvm::Value* fullMatch = payloadMatch ? payloadMatch : builder->getTrue();
            if (fullMatch->getType() != builder->getInt1Ty()) { fullMatch = builder->CreateTrunc(fullMatch, builder->getInt1Ty()); }
            builder->CreateBr(endBB);

            builder->SetInsertPoint(mismatchBB);
            builder->CreateBr(endBB);

            builder->SetInsertPoint(endBB);
            llvm::PHINode* phi = builder->CreatePHI(builder->getInt1Ty(), 2, "cmp_result");
            phi->addIncoming(fullMatch, matchBB);
            phi->addIncoming(builder->getFalse(), mismatchBB);

            llvm::Value* result = phi;
            if (isNe) { result = builder->CreateNot(result); }
            return result;
        }
        if (!lIsUnion && rIsUnion) {
            auto match = matchValueToUnionVariant(rUnionName, (*bin)->left_node, L);
            if (!match) {
                llvm::Value* res = builder->getFalse();
                if (isNe) res = builder->CreateNot(res);
                return res;
            }

            auto info = *match;
            llvm::Value* tag = builder->CreateExtractValue(R, 0, "union_tag");
            llvm::Value* dataPtr = builder->CreateExtractValue(R, 1, "union_data");

            llvm::Value* tagMatch = builder->CreateICmpEQ(tag, builder->getInt32(info.tagIndex), "union_tag_match");

            llvm::BasicBlock* matchBB = llvm::BasicBlock::Create(context, "tag_matches", currentFunction);
            llvm::BasicBlock* mismatchBB = llvm::BasicBlock::Create(context, "tag_mismatch", currentFunction);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "cmp_end", currentFunction);

            builder->CreateCondBr(tagMatch, matchBB, mismatchBB);

            builder->SetInsertPoint(matchBB);
            llvm::Value* payloadMatch = nullptr;

            if (!info.memberTypeStr.empty()) {
                llvm::Type* memberTy = llvmTypeFor(info.memberTypeStr);

                if (memberTy->isPointerTy()) {
                    llvm::Value* payload = builder->CreateBitCast(dataPtr, memberTy);

                    llvm::Function* strcmp_fn = module->getFunction("qc_string_eq");
                    if (!strcmp_fn) {
                        auto* i8Ptr = llvm::PointerType::get(context, 0);
                        auto* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {i8Ptr, i8Ptr}, false);
                        strcmp_fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_string_eq", module);
                    }
                    payloadMatch = builder->CreateCall(strcmp_fn, {L, payload}, "payload_str_eq");
                } else {
                    llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                    llvm::Value* payload = builder->CreateLoad(memberTy, typedPtr, "union_payload");

                    if (memberTy->isIntegerTy()) {
                        payloadMatch = builder->CreateICmpEQ(L, payload, "union_int_eq");
                    } else if (memberTy->isFloatingPointTy()) {
                        payloadMatch = builder->CreateFCmpOEQ(L, payload, "union_fp_eq");
                    }
                }
            }

            llvm::Value* fullMatch = payloadMatch ? payloadMatch : builder->getTrue();
            if (fullMatch->getType() != builder->getInt1Ty()) { fullMatch = builder->CreateTrunc(fullMatch, builder->getInt1Ty()); }
            builder->CreateBr(endBB);

            builder->SetInsertPoint(mismatchBB);
            builder->CreateBr(endBB);

            builder->SetInsertPoint(endBB);
            llvm::PHINode* phi = builder->CreatePHI(builder->getInt1Ty(), 2, "cmp_result");
            phi->addIncoming(fullMatch, matchBB);
            phi->addIncoming(builder->getFalse(), mismatchBB);

            llvm::Value* result = phi;
            if (isNe) { result = builder->CreateNot(result); }
            return result;
        }
        if (lIsUnion && rIsUnion) {
            llvm::Value* lhsTag = builder->CreateExtractValue(L, 0, "lhs_tag");
            llvm::Value* rhsTag = builder->CreateExtractValue(R, 0, "rhs_tag");
            llvm::Value* tagsEqual = builder->CreateICmpEQ(lhsTag, rhsTag, "tags_equal");

            llvm::BasicBlock* tagMatchBB = llvm::BasicBlock::Create(context, "tags_match", currentFunction);
            llvm::BasicBlock* tagMismatchBB = llvm::BasicBlock::Create(context, "tags_mismatch", currentFunction);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "union_cmp_end", currentFunction);

            builder->CreateCondBr(tagsEqual, tagMatchBB, tagMismatchBB);
            builder->SetInsertPoint(tagMatchBB);

            llvm::Value* lhsPayload = builder->CreateExtractValue(L, 1, "lhs_payload");
            llvm::Value* rhsPayload = builder->CreateExtractValue(R, 1, "rhs_payload");

            auto& members = userTypes.at(baseTypeName(lUnionName)).members;
            llvm::BasicBlock* payloadEndBB = llvm::BasicBlock::Create(context, "payload_cmp_end", currentFunction);
            llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(context, "cmp_default", currentFunction);
            llvm::SwitchInst* sw = builder->CreateSwitch(lhsTag, defaultBB, members.size());
            std::vector<std::pair<llvm::BasicBlock*, llvm::Value*>> caseResults;

            for (size_t i = 0; i < members.size(); i++) {
                llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "cmp_case_" + std::to_string(i), currentFunction);
                sw->addCase(builder->getInt32(i), caseBB);
                builder->SetInsertPoint(caseBB);

                std::string typeStr = members[i].type;
                size_t colonPos = typeStr.find(':');
                if (colonPos != std::string::npos) { typeStr = typeStr.substr(0, colonPos); }

                llvm::Type* memberTy = llvmTypeFor(typeStr);

                llvm::Value *lhsVal, *rhsVal;

                if (memberTy->isPointerTy()) {
                    lhsVal = builder->CreateBitCast(lhsPayload, memberTy);
                    rhsVal = builder->CreateBitCast(rhsPayload, memberTy);
                } else {
                    llvm::Value* lhsTyped = builder->CreateBitCast(lhsPayload, llvm::PointerType::get(context, 0));
                    llvm::Value* rhsTyped = builder->CreateBitCast(rhsPayload, llvm::PointerType::get(context, 0));
                    lhsVal = builder->CreateLoad(memberTy, lhsTyped);
                    rhsVal = builder->CreateLoad(memberTy, rhsTyped);
                }
                llvm::Value* cmp;
                if (memberTy->isIntegerTy()) {
                    cmp = builder->CreateICmpEQ(lhsVal, rhsVal);
                } else if (memberTy->isFloatingPointTy()) {
                    cmp = builder->CreateFCmpOEQ(lhsVal, rhsVal);
                } else if (memberTy->isPointerTy()) {
                    llvm::Function* strcmp_fn = module->getFunction("qc_string_eq");
                    cmp = builder->CreateCall(strcmp_fn, {lhsVal, rhsVal});
                    cmp = builder->CreateTrunc(cmp, builder->getInt1Ty());
                } else {
                    cmp = builder->getTrue();
                }

                caseResults.push_back({caseBB, cmp});
                builder->CreateBr(payloadEndBB);
            }
            builder->SetInsertPoint(defaultBB);
            builder->CreateBr(payloadEndBB);
            builder->SetInsertPoint(payloadEndBB);
            llvm::PHINode* payloadPhi = builder->CreatePHI(builder->getInt1Ty(), caseResults.size());
            for (auto& [bb, val] : caseResults) { payloadPhi->addIncoming(val, bb); }
            payloadPhi->addIncoming(builder->getFalse(), defaultBB);
            builder->CreateBr(endBB);

            builder->SetInsertPoint(tagMismatchBB);
            builder->CreateBr(endBB);

            builder->SetInsertPoint(endBB);
            llvm::PHINode* finalPhi = builder->CreatePHI(builder->getInt1Ty(), 2);
            finalPhi->addIncoming(payloadPhi, payloadEndBB);
            finalPhi->addIncoming(builder->getFalse(), tagMismatchBB);

            llvm::Value* result = finalPhi;
            if (isNe) { result = builder->CreateNot(result); }
            return result;
        }
    }
    if ((*bin)->is_f) {
        llvm::Value* lStr = convertToString(L, (*bin)->left_node, (*bin)->op_tok.pos);
        llvm::Value* rStr = convertToString(R, (*bin)->right_node, (*bin)->op_tok.pos);
        if (!lStr || !rStr) return nullptr;
        llvm::Function* concatFn = module->getFunction("qc_string_concat");
        if (!concatFn) {
            auto* i8Ptr = llvm::PointerType::get(context, 0);
            std::vector<llvm::Type*> argTypes = {i8Ptr, i8Ptr};
            auto* fnTy = llvm::FunctionType::get(i8Ptr, argTypes, false);
            concatFn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_string_concat", module);
        }

        return builder->CreateCall(concatFn, {lStr, rStr}, "fstr_concat");
    }
    std::string lUnion, rUnion;
    bool lIsUnion = isUnionType(L->getType(), &lUnion);
    bool rIsUnion = isUnionType(R->getType(), &rUnion);
    if (lIsUnion && rIsUnion) {
        auto& members = userTypes.at(baseTypeName(lUnion)).members;
        llvm::Value* lTag = builder->CreateExtractValue(L, 0, "ltag");
        llvm::Value* rTag = builder->CreateExtractValue(R, 0, "rtag");
        llvm::Value* lPayload = builder->CreateExtractValue(L, 1, "lpayload");
        llvm::Value* rPayload = builder->CreateExtractValue(R, 1, "rpayload");
        const bool isBooleanResult = op == TokenType::EQ_TO || op == TokenType::NOT_EQ || op == TokenType::LESS || op == TokenType::MORE ||
                                     op == TokenType::LESS_EQ || op == TokenType::MORE_EQ || op == TokenType::AND || op == TokenType::OR ||
                                     op == TokenType::XOR;
        llvm::Type* resultTy = isBooleanResult ? builder->getInt1Ty() : builder->getDoubleTy();
        llvm::AllocaInst* resultAlloc = createEntryAlloca("union_op_result", resultTy);
        llvm::BasicBlock* dispatchBB = llvm::BasicBlock::Create(context, "union_op_dispatch", currentFunction);
        llvm::BasicBlock* badTagBB = llvm::BasicBlock::Create(context, "union_op_bad_tag", currentFunction);
        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "union_op_end", currentFunction);
        llvm::Value* sameTag = builder->CreateICmpEQ(lTag, rTag, "union_same_tag");
        builder->CreateCondBr(sameTag, dispatchBB, badTagBB);
        builder->SetInsertPoint(badTagBB);
        builder->CreateStore(isBooleanResult ? static_cast<llvm::Value*>(builder->getFalse())
                                             : static_cast<llvm::Value*>(llvm::ConstantFP::get(builder->getDoubleTy(), 0.0)),
                             resultAlloc);
        builder->CreateBr(endBB);
        builder->SetInsertPoint(dispatchBB);
        llvm::SwitchInst* sw = builder->CreateSwitch(lTag, badTagBB, members.size());
        for (size_t i = 0; i < members.size(); ++i) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "union_op_case_" + std::to_string(i), currentFunction);
            sw->addCase(builder->getInt32(i), caseBB);
            builder->SetInsertPoint(caseBB);
            std::string ts = members[i].type;
            size_t colon = ts.find(':');
            if (colon != std::string::npos) ts = ts.substr(0, colon);
            llvm::Type* memberTy = llvmTypeFor(ts);
            if (memberTy->isPointerTy() || memberTy->isArrayTy()) {
                cg_error((*bin)->op_tok.pos, "pointer arithmetic is not allowed on unions", "QC-S119");
                return nullptr;
            }
            llvm::Value* lhsVal = builder->CreateLoad(memberTy, lPayload, "lmember");
            llvm::Value* rhsVal = builder->CreateLoad(memberTy, rPayload, "rmember");
            llvm::Value* res = nullptr;
            if (auto* classTy = llvm::dyn_cast<llvm::StructType>(memberTy); classTy && classTy->hasName()) {
                std::string className = classTy->getName().str();
                std::string methodName = getOperatorMethodName(op);
                if (classTypes.contains(className) && !methodName.empty()) {
                    llvm::Function* method = findMethodOverload(className, methodName, {rhsVal});
                    if (method) {
                        llvm::AllocaInst* self = createEntryAlloca("union_op_self", memberTy);
                        builder->CreateStore(lhsVal, self);
                        std::vector<llvm::Value*> args = {self, rhsVal};
                        if (insideTry()) {
                            auto* contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            res = builder->CreateInvoke(method, contBB, currentLandingPad(), args);
                            builder->SetInsertPoint(contBB);
                        } else {
                            res = builder->CreateCall(method, args, "op_result");
                        }
                    }
                } else if (auto it = userTypes.find(className);
                           it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                    auto fit = functions.find(className + "_" + methodName);
                    if (fit != functions.end()) {
                        llvm::Function* opMethod = fit->second;
                        std::vector<llvm::Value*> allArgs = {L, R};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            }
            if (!res && ts == "char" && (op == TokenType::PLUS || op == TokenType::MINUS)) {
                llvm::Value* pl = builder->CreateSExtOrTrunc(lhsVal, builder->getInt32Ty(), "char_lhs");
                llvm::Value* pr = builder->CreateSExtOrTrunc(rhsVal, builder->getInt32Ty(), "char_rhs");

                res = op == TokenType::PLUS ? builder->CreateAdd(pl, pr, "char_add") : builder->CreateSub(pl, pr, "char_sub");
            }
            if (!res) {
                const bool isFP = memberTy->isFloatingPointTy();
                switch (op) {
                case TokenType::AND:
                case TokenType::OR:
                case TokenType::XOR:
                    lhsVal = toTruthiness(lhsVal, get_pos((*bin)->left_node));
                    rhsVal = toTruthiness(rhsVal, get_pos((*bin)->right_node));
                    if (op == TokenType::AND)
                        res = builder->CreateAnd(lhsVal, rhsVal, "and");
                    else if (op == TokenType::OR)
                        res = builder->CreateOr(lhsVal, rhsVal, "or");
                    else
                        res = builder->CreateXor(lhsVal, rhsVal, "xor");
                    break;
                case TokenType::EQ_TO: res = isFP ? builder->CreateFCmpOEQ(lhsVal, rhsVal) : builder->CreateICmpEQ(lhsVal, rhsVal); break;
                case TokenType::NOT_EQ: res = isFP ? builder->CreateFCmpONE(lhsVal, rhsVal) : builder->CreateICmpNE(lhsVal, rhsVal); break;
                case TokenType::LESS: res = isFP ? builder->CreateFCmpOLT(lhsVal, rhsVal) : builder->CreateICmpSLT(lhsVal, rhsVal); break;
                case TokenType::MORE: res = isFP ? builder->CreateFCmpOGT(lhsVal, rhsVal) : builder->CreateICmpSGT(lhsVal, rhsVal); break;
                case TokenType::LESS_EQ: res = isFP ? builder->CreateFCmpOLE(lhsVal, rhsVal) : builder->CreateICmpSLE(lhsVal, rhsVal); break;
                case TokenType::MORE_EQ: res = isFP ? builder->CreateFCmpOGE(lhsVal, rhsVal) : builder->CreateICmpSGE(lhsVal, rhsVal); break;
                case TokenType::PLUS: res = isFP ? builder->CreateFAdd(lhsVal, rhsVal) : builder->CreateAdd(lhsVal, rhsVal); break;
                case TokenType::MINUS: res = isFP ? builder->CreateFSub(lhsVal, rhsVal) : builder->CreateSub(lhsVal, rhsVal); break;
                case TokenType::MUL: res = isFP ? builder->CreateFMul(lhsVal, rhsVal) : builder->CreateMul(lhsVal, rhsVal); break;
                case TokenType::DIV: res = isFP ? builder->CreateFDiv(lhsVal, rhsVal) : builder->CreateSDiv(lhsVal, rhsVal); break;
                case TokenType::MOD: res = isFP ? builder->CreateFRem(lhsVal, rhsVal) : builder->CreateSRem(lhsVal, rhsVal); break;
                case TokenType::AMPERSAND:
                case TokenType::PIPE:
                case TokenType::BITWISE_XOR:
                    if (isFP) {
                        cg_error((*bin)->op_tok.pos,
                                 "bitwise operation is not allowed "
                                 "on floating-point union members",
                                 "QC-S120");
                        return nullptr;
                    }

                    if (op == TokenType::AMPERSAND)
                        res = builder->CreateAnd(lhsVal, rhsVal);
                    else if (op == TokenType::PIPE)
                        res = builder->CreateOr(lhsVal, rhsVal);
                    else
                        res = builder->CreateXor(lhsVal, rhsVal);
                    break;
                case TokenType::LSHIFT:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                    if (isFP) {
                        cg_error((*bin)->op_tok.pos,
                                 "shift operation is not allowed on "
                                 "floating-point union members",
                                 "QC-S121");
                        return nullptr;
                    }
                    if (op == TokenType::LSHIFT)
                        res = builder->CreateShl(lhsVal, rhsVal);
                    else if (op == TokenType::RSHIFT)
                        res = builder->CreateAShr(lhsVal, rhsVal);
                    else
                        res = builder->CreateLShr(lhsVal, rhsVal);
                    break;
                case TokenType::L_ROT:
                case TokenType::R_ROT: {
                    if (isFP) {
                        cg_error((*bin)->op_tok.pos,
                                 "rotation is not allowed on "
                                 "floating-point union members",
                                 "QC-S122");
                        return nullptr;
                    }
                    llvm::Intrinsic::ID id = op == TokenType::L_ROT ? llvm::Intrinsic::fshl : llvm::Intrinsic::fshr;
                    llvm::Function* rotation = llvm::Intrinsic::getOrInsertDeclaration(module, id, {memberTy});
                    res = builder->CreateCall(rotation, {lhsVal, lhsVal, rhsVal}, "union_rotate");
                    break;
                }
                default: cg_error((*bin)->op_tok.pos, "unsupported operator for union member " + ts, "QC-S123"); return nullptr;
                }
            }
            if (res->getType() != resultTy) {
                if (resultTy->isDoubleTy()) {
                    if (res->getType()->isIntegerTy()) {
                        res = builder->CreateSIToFP(res, resultTy, "union_to_double");
                    } else if (res->getType()->isFloatTy()) {
                        res = builder->CreateFPExt(res, resultTy, "union_to_double");
                    } else {
                        cg_error((*bin)->op_tok.pos,
                                 "union operator must return a numeric "
                                 "value",
                                 "QC-S124");
                        return nullptr;
                    }
                } else {
                    cg_error((*bin)->op_tok.pos, "union boolean operator must return bool", "QC-S125");
                    return nullptr;
                }
            }
            builder->CreateStore(res, resultAlloc);
            builder->CreateBr(endBB);
        }
        builder->SetInsertPoint(endBB);
        return builder->CreateLoad(resultTy, resultAlloc, "union_op_result");
    }
    if (lIsUnion || rIsUnion) {
        std::string unionName = lIsUnion ? lUnion : rUnion;
        auto& members = userTypes.at(baseTypeName(unionName)).members;
        llvm::Value* unionVal = lIsUnion ? L : R;
        llvm::Value* otherVal = lIsUnion ? R : L;

        llvm::Value* tag = builder->CreateExtractValue(unionVal, 0, "tag");
        llvm::Value* payload = builder->CreateExtractValue(unionVal, 1, "payload");

        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "union_op_end", currentFunction);
        bool isComparison = op == TokenType::EQ_TO || op == TokenType::NOT_EQ || op == TokenType::LESS || op == TokenType::MORE ||
                            op == TokenType::LESS_EQ || op == TokenType::MORE_EQ || op == TokenType::AND || op == TokenType::OR ||
                            op == TokenType::XOR;
        llvm::Type* resultTy = isComparison ? builder->getInt1Ty() : builder->getDoubleTy();
        llvm::AllocaInst* resultAlloc = createEntryAlloca("union_op_result", resultTy);

        llvm::SwitchInst* sw = builder->CreateSwitch(tag, endBB, members.size());
        std::vector<llvm::BasicBlock*> caseBBs;

        for (size_t i = 0; i < members.size(); i++) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "union_op_case_" + std::to_string(i), currentFunction);
            sw->addCase(builder->getInt32(i), caseBB);
            builder->SetInsertPoint(caseBB);

            std::string ts = members[i].type;
            size_t c = ts.find(':');
            if (c != std::string::npos) ts = ts.substr(0, c);
            llvm::Type* memberTy = llvmTypeFor(ts);
            bool lhsChar = lIsUnion ? ts == "char" : getExpressionType((*bin)->left_node) == "char";
            bool rhsChar = rIsUnion ? ts == "char" : getExpressionType((*bin)->right_node) == "char";
            llvm::Value* typedPtr = builder->CreateBitCast(payload, llvm::PointerType::get(context, 0));
            llvm::Value* memberVal = builder->CreateLoad(memberTy, typedPtr, "member");

            llvm::Value* lhsVal = lIsUnion ? memberVal : otherVal;
            llvm::Value* rhsVal = lIsUnion ? otherVal : memberVal;
            llvm::StructType* classTy = llvm::dyn_cast<llvm::StructType>(lhsVal->getType());
            llvm::Value* res = nullptr;

            if (classTy && classTy->hasName()) {
                std::string className = classTy->getName().str();
                std::string methodName = getOperatorMethodName(op);

                if (classTypes.contains(className) && !methodName.empty()) {
                    std::vector<llvm::Value*> args = {rhsVal};
                    llvm::Function* method = findMethodOverload(className, methodName, args);
                    if (method) {
                        llvm::AllocaInst* self = createEntryAlloca("union_op_self", lhsVal->getType());
                        builder->CreateStore(lhsVal, self);

                        std::vector<llvm::Value*> callArgs = {self, rhsVal};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            res = builder->CreateInvoke(method, contBB, currentLandingPad(), callArgs);
                            builder->SetInsertPoint(contBB);
                        }
                        res = builder->CreateCall(method, callArgs, "op_result");
                    }
                } else if (auto it = userTypes.find(className);
                           it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                    auto fit = functions.find(className + "_" + methodName);
                    if (fit != functions.end()) {
                        llvm::Function* opMethod = fit->second;
                        std::vector<llvm::Value*> allArgs = {L, R};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            } else {
                classTy = llvm::dyn_cast<llvm::StructType>(rhsVal->getType());
                if (classTy && classTy->hasName()) {
                    std::string className = classTy->getName().str();
                    std::string methodName = getRoperatorMethodName(op);

                    if (classTypes.contains(className) && !methodName.empty()) {
                        std::vector<llvm::Value*> args = {rhsVal};
                        llvm::Function* method = findMethodOverload(className, methodName, args);
                        if (method) {
                            llvm::AllocaInst* self = createEntryAlloca("union_op_self", rhsVal->getType());
                            builder->CreateStore(rhsVal, self);
                            std::vector<llvm::Value*> callArgs = {self, lhsVal};
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                res = builder->CreateInvoke(method, contBB, currentLandingPad(), callArgs);
                                builder->SetInsertPoint(contBB);
                            }
                            res = builder->CreateCall(method, callArgs, "op_result");
                        }
                    }
                } else if (classTy && classTy->hasName()) {
                    std::string className = classTy->getName().str();
                    if (auto it = userTypes.find(className);
                        it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                        std::string opMethodName = getRoperatorMethodName((*bin)->op_tok.type);
                        if (!opMethodName.empty()) {
                            std::vector<llvm::Value*> args = {R};
                            auto fit = functions.find(className + "_" + opMethodName);
                            if (fit != functions.end()) {
                                llvm::Function* opMethod = fit->second;
                                std::vector<llvm::Value*> allArgs = {L, R};
                                if (insideTry()) {
                                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++),
                                                                           currentFunction);
                                    auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                    builder->SetInsertPoint(contBB);
                                    return invk;
                                }
                                return builder->CreateCall(opMethod, allArgs, "op_result");
                            }
                        }
                    }
                }
            }
            llvm::Type* lTy = lhsVal->getType();
            llvm::Type* rTy = rhsVal->getType();
            if ((op == TokenType::PLUS || op == TokenType::MINUS) && (lhsChar || rhsChar)) {
                if (!lhsVal->getType()->isIntegerTy() || !rhsVal->getType()->isIntegerTy()) {
                    cg_error((*bin)->op_tok.pos, "char arithmetic requires integer operands", "QC-S126");
                    return nullptr;
                }
                unsigned width = std::max(32u, std::max(lhsVal->getType()->getIntegerBitWidth(), rhsVal->getType()->getIntegerBitWidth()));
                llvm::Type* promotedTy = builder->getIntNTy(width);
                lhsVal = builder->CreateSExtOrTrunc(lhsVal, promotedTy);
                rhsVal = builder->CreateSExtOrTrunc(rhsVal, promotedTy);
                res = op == TokenType::PLUS ? builder->CreateAdd(lhsVal, rhsVal) : builder->CreateSub(lhsVal, rhsVal);
            }
            bool lhsPtr = lhsVal->getType()->isPointerTy();
            bool rhsPtr = rhsVal->getType()->isPointerTy();
            if (lhsPtr || rhsPtr) {
                cg_error(get_pos(*bin), "Pointer arithmetic is not allow on unions. Consider extracting the value first.", "QC-S127");
                return nullptr;
            }
            if (lTy != rTy) {
                if (lTy->isDoubleTy() || rTy->isDoubleTy()) {
                    if (!lTy->isDoubleTy())
                        lhsVal = lTy->isFloatTy() ? builder->CreateFPExt(lhsVal, builder->getDoubleTy())
                                                  : builder->CreateSIToFP(lhsVal, builder->getDoubleTy());
                    if (!rTy->isDoubleTy())
                        rhsVal = rTy->isFloatTy() ? builder->CreateFPExt(rhsVal, builder->getDoubleTy())
                                                  : builder->CreateSIToFP(rhsVal, builder->getDoubleTy());
                } else if (lTy->isFloatTy() || rTy->isFloatTy()) {
                    if (!lTy->isFloatTy()) lhsVal = builder->CreateSIToFP(lhsVal, builder->getFloatTy());
                    if (!rTy->isFloatTy()) rhsVal = builder->CreateSIToFP(rhsVal, builder->getFloatTy());
                } else if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
                    unsigned lBits = lTy->getIntegerBitWidth();
                    unsigned rBits = rTy->getIntegerBitWidth();
                    if (lBits < rBits)
                        lhsVal = builder->CreateSExt(lhsVal, rTy);
                    else
                        rhsVal = builder->CreateSExt(rhsVal, lTy);
                } else if (lTy->isIntegerTy() && rTy->isFloatingPointTy()) {
                    lhsVal = builder->CreateSIToFP(lhsVal, rTy);
                } else if (lTy->isFloatingPointTy() && rTy->isIntegerTy()) {
                    rhsVal = builder->CreateSIToFP(rhsVal, lTy);
                }
            }
            bool isFP = lhsVal->getType()->isFloatingPointTy();
            if (res == nullptr) {
                switch (op) {
                case TokenType::AND:
                case TokenType::OR:
                case TokenType::XOR:
                    lhsVal = toTruthiness(lhsVal, get_pos((*bin)->left_node));
                    rhsVal = toTruthiness(rhsVal, get_pos((*bin)->right_node));
                    if (op == TokenType::AND)
                        res = builder->CreateAnd(lhsVal, rhsVal);
                    else if (op == TokenType::OR)
                        res = builder->CreateOr(lhsVal, rhsVal);
                    else
                        res = builder->CreateXor(lhsVal, rhsVal);
                    break;
                case TokenType::NOT_EQ: res = isFP ? builder->CreateFCmpONE(lhsVal, rhsVal) : builder->CreateICmpNE(lhsVal, rhsVal); break;
                case TokenType::EQ: res = isFP ? builder->CreateFCmpOEQ(lhsVal, rhsVal) : builder->CreateICmpEQ(lhsVal, rhsVal); break;
                case TokenType::MOD: res = isFP ? builder->CreateFRem(lhsVal, rhsVal) : builder->CreateSRem(lhsVal, rhsVal); break;
                case TokenType::LESS: res = isFP ? builder->CreateFCmpOLT(lhsVal, rhsVal) : builder->CreateICmpSLT(lhsVal, rhsVal); break;
                case TokenType::MORE: res = isFP ? builder->CreateFCmpOGT(lhsVal, rhsVal) : builder->CreateICmpSGT(lhsVal, rhsVal); break;
                case TokenType::LESS_EQ: res = isFP ? builder->CreateFCmpOLE(lhsVal, rhsVal) : builder->CreateICmpSLE(lhsVal, rhsVal); break;
                case TokenType::MORE_EQ: res = isFP ? builder->CreateFCmpOGE(lhsVal, rhsVal) : builder->CreateICmpSGE(lhsVal, rhsVal); break;
                case TokenType::PLUS: res = isFP ? builder->CreateFAdd(lhsVal, rhsVal) : builder->CreateAdd(lhsVal, rhsVal); break;
                case TokenType::MINUS: res = isFP ? builder->CreateFSub(lhsVal, rhsVal) : builder->CreateSub(lhsVal, rhsVal); break;
                case TokenType::MUL: res = isFP ? builder->CreateFMul(lhsVal, rhsVal) : builder->CreateMul(lhsVal, rhsVal); break;
                case TokenType::DIV: res = isFP ? builder->CreateFDiv(lhsVal, rhsVal) : builder->CreateSDiv(lhsVal, rhsVal); break;
                case TokenType::AMPERSAND:
                case TokenType::PIPE:
                case TokenType::BITWISE_XOR:
                case TokenType::LSHIFT:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                    if (isFP) {
                        cg_error((*bin)->op_tok.pos,
                                 "bitwise operations not allowed on "
                                 "floating-point union members",
                                 "QC-S128");
                        return nullptr;
                    }
                    if (op == TokenType::AMPERSAND)
                        res = builder->CreateAnd(lhsVal, rhsVal);
                    else if (op == TokenType::PIPE)
                        res = builder->CreateOr(lhsVal, rhsVal);
                    else if (op == TokenType::BITWISE_XOR)
                        res = builder->CreateXor(lhsVal, rhsVal);
                    else if (op == TokenType::LSHIFT)
                        res = builder->CreateShl(lhsVal, rhsVal);
                    else if (op == TokenType::RSHIFT)
                        res = builder->CreateAShr(lhsVal, rhsVal);
                    else
                        res = builder->CreateLShr(lhsVal, rhsVal);
                    break;
                case TokenType::L_ROT:
                case TokenType::R_ROT: {
                    if (isFP) {
                        cg_error((*bin)->op_tok.pos,
                                 "rotation not allowed on floating-point union "
                                 "members",
                                 "QC-S129");
                        return nullptr;
                    }
                    llvm::Intrinsic::ID id = (op == TokenType::L_ROT) ? llvm::Intrinsic::fshl : llvm::Intrinsic::fshr;
                    llvm::Function* rotFunc = llvm::Intrinsic::getOrInsertDeclaration(module, id, {lhsVal->getType()});
                    res = builder->CreateCall(rotFunc, {lhsVal, lhsVal, rhsVal});
                    break;
                }
                default: res = memberVal; break;
                }
            }
            bool isCharArithmetic = (lhsChar || rhsChar) && (op == TokenType::PLUS || op == TokenType::MINUS);
            if (isCharArithmetic) {
                if (!lhsVal->getType()->isIntegerTy() || !rhsVal->getType()->isIntegerTy()) {
                    cg_error((*bin)->op_tok.pos, "char arithmetic requires integer operands", "QC-S126");
                    return nullptr;
                }
                bool mixedChar = lhsChar != rhsChar;
                llvm::Type* promotedTy = builder->getInt32Ty();
                llvm::Value* promotedL = builder->CreateSExtOrTrunc(lhsVal, promotedTy, "char_lhs");
                llvm::Value* promotedR = builder->CreateSExtOrTrunc(rhsVal, promotedTy, "char_rhs");
                switch (op) {
                case TokenType::PLUS: res = builder->CreateAdd(promotedL, promotedR, "char_add"); break;
                case TokenType::MINUS: res = builder->CreateSub(promotedL, promotedR, "char_sub"); break;
                default: break;
                }
                if (mixedChar) { res = builder->CreateTrunc(res, builder->getInt8Ty(), "truncate_to_char"); }
            }
            llvm::Type* allocTy = resultAlloc->getAllocatedType();
            if (res->getType() != allocTy) {
                if (allocTy->isDoubleTy() && res->getType()->isIntegerTy())
                    res = builder->CreateSIToFP(res, allocTy);
                else if (allocTy->isDoubleTy() && res->getType()->isFloatTy())
                    res = builder->CreateFPExt(res, allocTy);
            }
            builder->CreateStore(res, resultAlloc);
            builder->CreateBr(endBB);
            caseBBs.push_back(caseBB);
        }

        builder->SetInsertPoint(endBB);
        return builder->CreateLoad(resultAlloc->getAllocatedType(), resultAlloc, "union_op_result");
    }
    std::string lTyStr = getExpressionType((*bin)->left_node);
    std::string rTyStr = getExpressionType((*bin)->right_node);
    L = normalizeValue(L, (*bin)->left_node);
    R = normalizeValue(R, (*bin)->right_node);
    lty = L->getType();
    rty = R->getType();
    if (auto lStructTy = llvm::dyn_cast<llvm::StructType>(lty)) {
        if (lStructTy->hasName()) {
            std::string className = lStructTy->getName().str();

            if (classTypes.find(className) != classTypes.end()) {
                std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);

                if (!opMethodName.empty()) {
                    std::vector<llvm::Value*> args = {R};
                    llvm::Function* opMethod = findMethodOverload(className, opMethodName, args);

                    if (opMethod) {
                        llvm::AllocaInst* temp = createEntryAlloca("temp_op_lhs", lty);
                        builder->CreateStore(L, temp);

                        std::vector<llvm::Value*> allArgs = {temp, R};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            } else if (auto it = userTypes.find(className);
                       it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);
                if (!opMethodName.empty()) {
                    auto fit = functions.find(className + "_" + opMethodName);
                    if (fit != functions.end()) {
                        llvm::Function* opMethod = fit->second;
                        std::vector<llvm::Value*> allArgs = {L, R};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            }
        }
    }
    if (auto rStructTy = llvm::dyn_cast<llvm::StructType>(rty)) {
        if (rStructTy->hasName()) {
            std::string className = rStructTy->getName().str();
            if (classTypes.find(className) != classTypes.end()) {
                std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);
                if (!opMethodName.empty()) {
                    std::vector<llvm::Value*> args = {L};
                    llvm::Function* opMethod = findMethodOverload(className, opMethodName, args);
                    if (opMethod) {
                        llvm::AllocaInst* temp = createEntryAlloca("temp_op_rhs", rty);
                        builder->CreateStore(R, temp);
                        std::vector<llvm::Value*> allArgs = {temp, L};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            } else if (auto it = userTypes.find(className);
                       it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                std::string opMethodName = getOperatorMethodName((*bin)->op_tok.type);
                if (!opMethodName.empty()) {
                    auto fit = functions.find(className + "_" + opMethodName);
                    if (fit != functions.end()) {
                        llvm::Function* opMethod = fit->second;
                        std::vector<llvm::Value*> allArgs = {L, R};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            }
        }
    }
    if (lty != rty) {
        if (lty->isPointerTy()) {
            if (llvm::Type* lt = llvmTypeFor(lTyStr); !lt->isPointerTy()) {
                lty = lt;
                L = builder->CreateLoad(lt, L, "strip_ref");
            }
        }
        if (rty->isPointerTy()) {
            if (llvm::Type* rt = llvmTypeFor(rTyStr); !rt->isPointerTy()) {
                rty = rt;
                R = builder->CreateLoad(rt, R, "strip_ref");
            }
        }
    }
    bool isCharOperation = false;
    if ((lTyStr == "char" || rTyStr == "char") && (lty->isIntegerTy() && rty->isIntegerTy()) && (op == TokenType::PLUS || op == TokenType::MINUS)) {
        bool lIsChar = lTyStr == "char";
        bool rIsChar = rTyStr == "char";
        if (lIsChar && rIsChar) {
            L = builder->CreateSExt(L, builder->getInt32Ty(), "char_promote");
            R = builder->CreateSExt(R, builder->getInt32Ty(), "char_promote");
            lty = builder->getInt32Ty();
            rty = builder->getInt32Ty();
        } else if (lIsChar) {
            L = builder->CreateSExtOrTrunc(L, rty, "char_set");
            lty = rty;
            isCharOperation = true;
        } else if (rIsChar) {
            R = builder->CreateSExtOrTrunc(R, lty, "char_set");
            rty = lty;
            isCharOperation = true;
        }
    }
    if (lty != rty) {
        if (lty->isFloatTy() && rty->isDoubleTy()) {
            L = builder->CreateFPExt(L, rty, "promote_to_double");
            lty = rty;
        } else if (rty->isFloatTy() && lty->isDoubleTy()) {
            R = builder->CreateFPExt(R, lty, "promote_to_double");
            rty = lty;
        } else if (lty->isIntegerTy() && rty->isIntegerTy()) {
            unsigned lBits = lty->getIntegerBitWidth();
            unsigned rBits = rty->getIntegerBitWidth();
            if (lBits == 1 || rBits == 1 || lBits == 2 || rBits == 2) {
            } else {
                if (lBits < rBits) {
                    L = builder->CreateSExt(L, rty, "promote_int");
                    lty = rty;
                } else if (rBits < lBits) {
                    R = builder->CreateSExt(R, lty, "promote_int");
                    rty = lty;
                }
            }
        } else if (lty->isIntegerTy() && rty->isFloatingPointTy()) {
            L = builder->CreateSIToFP(L, rty, "int_to_float");
            lty = rty;
        } else if (rty->isIntegerTy() && lty->isFloatingPointTy()) {
            R = builder->CreateSIToFP(R, lty, "int_to_float");
            rty = lty;
        }
    }
    bool isFloatTy = lty->isFloatingPointTy();
    switch ((*bin)->op_tok.type) {
    case TokenType::PLUS:
        if (lty->isPointerTy() || rty->isPointerTy() || lty->isArrayTy()) {
            std::string lType = getExpressionType((*bin)->left_node);
            std::string rType = getExpressionType((*bin)->right_node);

            if ((lType == "string" || lType == "char*" || lType == "char[]") && (rType == "string" || rType == "char*" || rType == "[]")) {
                if (lType == "char[]") { L = decayArrayToPointer(L); }
                if (rType == "char[]") { R = decayArrayToPointer(R); }
                llvm::Function* concatFn = module->getFunction("qc_string_concat");
                if (!concatFn) {
                    llvm::Type* i8PtrTy = llvm::PointerType::get(context, 0);
                    std::vector<llvm::Type*> argTypes = {i8PtrTy, i8PtrTy};
                    llvm::FunctionType* fnTy = llvm::FunctionType::get(i8PtrTy, argTypes, false);
                    concatFn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_string_concat", module);
                }
                return builder->CreateCall(concatFn, {L, R}, "str_concat");
            } else if (lType.ends_with("*") || lType == "@nullptr" || lType == "string" && rType != "string" || lType.ends_with("]")) {
                if (lType == "void*") {
                    cg_error((*bin)->op_tok.pos,
                             "pointer arithmetic cannot be preformed on "
                             "void pointers",
                             "QC-S130");
                    return nullptr;
                }
                if (lType.ends_with("]")) {
                    L = decayArrayToPointer(L);
                    size_t start_pos = lType.rfind("[");
                    if (start_pos != std::string::npos) { lType.replace(start_pos, lType.size() - start_pos, "*"); }
                }
                if (!llvmTypeFor(rType)->isIntegerTy()) {
                    cg_error((*bin)->op_tok.pos,
                             "pointer arithmetic may only be preformed on "
                             "ptr lhs and "
                             "int rhs, got " +
                                 lType + " and " + rType,
                             "QC-S131");
                    return nullptr;
                }
                if (lType == "string")
                    lType = "char";
                else
                    lType.pop_back();
                return builder->CreateGEP(llvmTypeFor(lType), L, R, "ptr_arith_plus");
            }
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on types " + lType + " + " + rType, "QC-T014");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        return isFloatTy         ? builder->CreateFAdd(L, R, "fadd")
               : isCharOperation ? builder->CreateTrunc(builder->CreateAdd(L, R, "add"), builder->getInt8Ty(), "trunc_char")
                                 : builder->CreateAdd(L, R, "add");
    case TokenType::MINUS:
        if (lty->isPointerTy() || rty->isPointerTy() || lty->isArrayTy() || rty->isArrayTy()) {
            std::string lType = getExpressionType((*bin)->left_node);
            std::string rType = getExpressionType((*bin)->right_node);

            if ((lType.ends_with("]") || lType.ends_with("*") || lType == "@nullptr") &&
                (rType.ends_with("]") || rType.ends_with("*") || rType == "@nullptr")) {
                if (lType == "void*") {
                    cg_error((*bin)->op_tok.pos,
                             "pointer arithmetic cannot be preformed on "
                             "void pointers",
                             "QC-S130");
                    return nullptr;
                }
                if (rType == "void*") {
                    cg_error((*bin)->op_tok.pos,
                             "pointer arithmetic cannot be preformed on "
                             "void pointers",
                             "QC-S130");
                    return nullptr;
                }
                if (remove_last_ptr(lType) != remove_last_ptr(rType)) {
                    cg_error((*bin)->op_tok.pos,
                             "pointer arithmetic may only be preformed on "
                             "the same lhs "
                             "and rhs type, got " +
                                 lType + " and " + rType,
                             "QC-T017");
                    return nullptr;
                }
                std::string baseType = (lType == "@nullptr") ? rType : lType;
                if (baseType == "@nullptr") { return builder->getInt32(0); }
                if (baseType.ends_with("]")) {
                    auto pos = baseType.rfind("[");
                    if (pos != std::string::npos) baseType.erase(pos);
                    baseType += "*";
                }
                baseType.pop_back();
                llvm::Value* diff = builder->CreatePtrDiff(llvmTypeFor(baseType), L, R, "ptr_diff");
                return builder->CreateTrunc(diff, builder->getInt32Ty());
            } else if (lType.ends_with("*") && rType == "int") {
                std::string baseType = lType;
                baseType.pop_back();
                llvm::Value* negR = builder->CreateNeg(R, "neg_offset");
                return builder->CreateGEP(llvmTypeFor(baseType), L, {negR}, "ptr_arith_minus");
            }

            cg_error((*bin)->op_tok.pos, "invalid pointer subtraction: " + lType + " - " + rType, "QC-S132");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        return isFloatTy         ? builder->CreateFSub(L, R, "fsub")
               : isCharOperation ? builder->CreateTrunc(builder->CreateSub(L, R, "sub"), builder->getInt8Ty(), "trunc_char")
                                 : builder->CreateSub(L, R, "sub");
    case TokenType::MUL:
        if (lTyStr == "char" || rTyStr == "char") {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on char types", "QC-T018");
            return nullptr;
        }
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on string types", "QC-T019");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        return isFloatTy ? builder->CreateFMul(L, R, "fmul") : builder->CreateMul(L, R, "mul");
    case TokenType::DIV:
        if (lTyStr == "char" || rTyStr == "char") {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on char types", "QC-T018");
            return nullptr;
        }
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on string types", "QC-T019");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        return isFloatTy ? builder->CreateFDiv(L, R, "fdiv") : builder->CreateSDiv(L, R, "sdiv");
    case TokenType::MOD:
        if (lTyStr == "char" || rTyStr == "char") {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on char types", "QC-T018");
            return nullptr;
        }
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on string types", "QC-T019");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        return isFloatTy ? builder->CreateFRem(L, R, "frem") : builder->CreateSRem(L, R, "srem");
    case TokenType::AMPERSAND:
    case TokenType::PIPE:
    case TokenType::BITWISE_XOR:
        if (isFloatTy) {
            cg_error((*bin)->op_tok.pos, "cannot perform bitwise operations on float/double types", "QC-T020");
            return nullptr;
        }
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform bitwise operations on string types", "QC-T021");
            return nullptr;
        }
        if ((*bin)->op_tok.type == TokenType::AMPERSAND) return builder->CreateAnd(L, R, "andtmp");
        if ((*bin)->op_tok.type == TokenType::PIPE) return builder->CreateOr(L, R, "ortmp");
        return builder->CreateXor(L, R, "xortmp");
    case TokenType::RSHIFT:
    case TokenType::LSHIFT:
    case TokenType::LOGICAL_RSHIFT:
        if (isFloatTy) {
            cg_error((*bin)->op_tok.pos, "cannot perform shifts on float/double types", "QC-T022");
            return nullptr;
        }
        if ((*bin)->op_tok.type == TokenType::LSHIFT) return builder->CreateShl(L, R, "shltmp");
        if ((*bin)->op_tok.type == TokenType::RSHIFT) return builder->CreateAShr(L, R, "ashrtmp");
        return builder->CreateLShr(L, R, "lshrtmp");
    case TokenType::L_ROT:
    case TokenType::R_ROT:
        if (isFloatTy) {
            cg_error((*bin)->op_tok.pos, "cannot perform rotations on float/double types", "QC-T023");
            return nullptr;
        }
        {
            llvm::Intrinsic::ID id = ((*bin)->op_tok.type == TokenType::L_ROT) ? llvm::Intrinsic::fshl : llvm::Intrinsic::fshr;
            llvm::Function* rotFunc = llvm::Intrinsic::getOrInsertDeclaration(module, id, {lty});
            return builder->CreateCall(rotFunc, {L, L, R}, "rottmp");
        }
    case TokenType::POWER: {
        if (lTyStr == "char" || rTyStr == "char") {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on char types", "QC-T018");
            return nullptr;
        }
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on string types", "QC-T019");
            return nullptr;
        }
        if (lty == builder->getInt1Ty() || rty == builder->getInt1Ty()) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on bool types", "QC-T015");
            return nullptr;
        }

        if (lty == builder->getIntNTy(2) || rty == builder->getIntNTy(2)) {
            cg_error((*bin)->op_tok.pos, "cannot perform arithmetic on qbool types", "QC-T016");
            return nullptr;
        }
        llvm::Type* ty = L->getType();

        if (ty->isIntegerTy()) {
            llvm::Function* qc_powi = module->getFunction("qc_powi_i32");
            if (!qc_powi) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt32Ty(), {builder->getInt32Ty(), builder->getInt32Ty()}, false);
                qc_powi = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_powi_i32", module);
            }
            return builder->CreateCall(qc_powi, {L, R}, "powi");
        } else if (ty->isFloatTy() || ty->isDoubleTy()) {
            llvm::Function* powFn = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::pow, {ty});
            return builder->CreateCall(powFn, {L, R}, "pow");
        } else {
            cg_error((*bin)->op_tok.pos, "pOWER not supported for this type", "QC-T024");
            return nullptr;
        }
    }
    case TokenType::EQ_TO:
    case TokenType::NOT_EQ: {
        if (lty->isIntegerTy() && rty->isIntegerTy()) {
            unsigned lBits = lty->getIntegerBitWidth();
            unsigned rBits = rty->getIntegerBitWidth();
            if (lBits == 1 || rBits == 1 || lBits == 2 || rBits == 2) {
                if (lBits == rBits) {
                    return (op == TokenType::EQ_TO) ? builder->CreateICmpEQ(L, R, "icmpeq") : builder->CreateICmpNE(L, R, "icmpne");
                } else {
                    return builder->getInt1(op == TokenType::NOT_EQ ? 1 : 0);
                }
            }
            if (lBits < rBits) {
                L = builder->CreateSExt(L, rty, "promote");
            } else if (rBits < lBits) {
                R = builder->CreateSExt(R, lty, "promote");
            }

            return (op == TokenType::EQ_TO) ? builder->CreateICmpEQ(L, R, "icmpeq") : builder->CreateICmpNE(L, R, "icmpne");
        }
        if ((lty->isFloatingPointTy() && rty->isFloatingPointTy())) {
            warn("float-equal", get_pos(*bin), "comparisons on floating-point-types can produce unexpected results due to rounding errors",
                 "QC-W014");
            return (op == TokenType::EQ_TO) ? builder->CreateFCmpOEQ(L, R, "fcmpeq") : builder->CreateFCmpONE(L, R, "fcmpne");
        }
        auto isStringLike = [](const std::string& type) { return type == "string" || type == "char*" || type == "char[]"; };
        if (lty->isPointerTy() && rty->isPointerTy()) {
            std::string lType = getExpressionType((*bin)->left_node);
            std::string rType = getExpressionType((*bin)->right_node);
            if (isStringLike(lType) && isStringLike(rType)) {
                if (lType == "char[]") L = decayArrayToPointer(L);
                if (rType == "char[]") R = decayArrayToPointer(R);
                llvm::Function* stringEq = module->getFunction("qc_string_eq");
                if (!stringEq) {
                    auto* ptrTy = llvm::PointerType::get(context, 0);
                    auto* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {ptrTy, ptrTy}, false);
                    stringEq = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_string_eq", module);
                }
                llvm::Value* equal = builder->CreateCall(stringEq, {L, R}, "str_eq");
                return op == TokenType::NOT_EQ ? builder->CreateNot(equal, "str_ne") : equal;
            }
            return op == TokenType::EQ_TO ? builder->CreateICmpEQ(L, R, "ptr_eq") : builder->CreateICmpNE(L, R, "ptr_ne");
        }
        if (op == TokenType::EQ_TO) {
            return builder->getInt1(0);
        } else {
            return builder->getInt1(1);
        }
    }
    case TokenType::QEQEQ:
    case TokenType::QNEQ: {
        llvm::Value* boolResult = nullptr;

        if (lty->isIntegerTy() && rty->isIntegerTy()) {
            unsigned lBits = lty->getIntegerBitWidth();
            unsigned rBits = rty->getIntegerBitWidth();

            if (lBits == 1 || rBits == 1 || lBits == 2 || rBits == 2) {
                if (lBits == rBits) {
                    boolResult = (op == TokenType::QEQEQ) ? builder->CreateICmpEQ(L, R, "qicmpeq") : builder->CreateICmpNE(L, R, "qicmpne");
                } else {
                    return builder->getIntN(2, 0);
                }
            } else {
                if (lBits < rBits) {
                    L = builder->CreateSExt(L, rty, "promote");
                } else if (rBits < lBits) {
                    R = builder->CreateSExt(R, lty, "promote");
                }

                boolResult = (op == TokenType::QEQEQ) ? builder->CreateICmpEQ(L, R, "qicmpeq") : builder->CreateICmpNE(L, R, "qicmpne");
            }
        } else if (lty->isFloatingPointTy() && rty->isFloatingPointTy()) {
            boolResult = (op == TokenType::QEQEQ) ? builder->CreateFCmpOEQ(L, R, "qfcmpeq") : builder->CreateFCmpONE(L, R, "qfcmpne");
        } else if (lty->isPointerTy() && rty->isPointerTy()) {
            llvm::Function* strcmp_fn = module->getFunction("qc_string_eq");
            llvm::Value* cmp = builder->CreateCall(strcmp_fn, {L, R});

            if (op == TokenType::QNEQ) { cmp = builder->CreateNot(cmp); }

            boolResult = builder->CreateTrunc(cmp, builder->getInt1Ty());
        } else {
            return builder->getIntN(2, 0);
        }
        llvm::Value* ext = builder->CreateZExt(boolResult, builder->getInt8Ty());
        llvm::Value* tripled = builder->CreateMul(ext, builder->getInt8(3));
        return builder->CreateTrunc(tripled, builder->getIntNTy(2));
    }
    case TokenType::LESS:
    case TokenType::MORE:
    case TokenType::LESS_EQ:
    case TokenType::MORE_EQ: {
        bool isFloatTy = false;
        if (lty->isPointerTy() || rty->isPointerTy()) {
            cg_error((*bin)->op_tok.pos, "cannot perform this operation on string types", "QC-T019");
            return nullptr;
        }
        if (lty->isIntegerTy() && rty->isIntegerTy()) {
            unsigned lBits = lty->getIntegerBitWidth();
            unsigned rBits = rty->getIntegerBitWidth();
            if (lBits == 1 || lBits == 2 || rBits == 1 || rBits == 2) {
                cg_error((*bin)->op_tok.pos, "cannot use comparison operators on bool/qbool", "QC-S133");
                return nullptr;
            }
            if (lBits < rBits) {
                L = builder->CreateSExt(L, R->getType());
                lty = L->getType();
            } else if (rBits < lBits) {
                R = builder->CreateSExt(R, L->getType());
                rty = R->getType();
            }
        } else if (lty->isFloatingPointTy() && rty->isFloatingPointTy()) {
            isFloatTy = true;
            if (lty->isFloatTy() && rty->isDoubleTy()) {
                L = builder->CreateFPExt(L, rty);
                lty = rty;
            } else if (lty->isDoubleTy() && rty->isFloatTy()) {
                R = builder->CreateFPExt(R, lty);
                rty = lty;
            }
        } else {
            cg_error((*bin)->op_tok.pos, "cannot compare non-numeric types with <, >, <=, >=", "QC-T025");
            return nullptr;
        }
        switch (op) {
        case TokenType::LESS: return isFloatTy ? builder->CreateFCmpOLT(L, R, "fcmplt") : builder->CreateICmpSLT(L, R, "icmplt");
        case TokenType::MORE: return isFloatTy ? builder->CreateFCmpOGT(L, R, "fcmpgt") : builder->CreateICmpSGT(L, R, "icmpgt");
        case TokenType::LESS_EQ: return isFloatTy ? builder->CreateFCmpOLE(L, R, "fcmple") : builder->CreateICmpSLE(L, R, "icmple");
        case TokenType::MORE_EQ: return isFloatTy ? builder->CreateFCmpOGE(L, R, "fcmpge") : builder->CreateICmpSGE(L, R, "icmpge");
        default: break;
        }
    }
    case TokenType::AND:
        L = toTruthiness(L, get_pos((*bin)->left_node));
        R = toTruthiness(R, get_pos((*bin)->right_node));
        return builder->CreateAnd(L, R, "and");
    case TokenType::OR:
        L = toTruthiness(L, get_pos((*bin)->left_node));
        R = toTruthiness(R, get_pos((*bin)->right_node));
        return builder->CreateOr(L, R, "or");
    case TokenType::XOR:
        L = toTruthiness(L, get_pos((*bin)->left_node));
        R = toTruthiness(R, get_pos((*bin)->right_node));
        return builder->CreateXor(L, R, "xor");
    case TokenType::QAND:
        if (lty == builder->getIntNTy(2) && rty == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qand");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt8Ty(), {builder->getInt8Ty(), builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qand", module);
            }
            llvm::Value* L8 = builder->CreateZExt(L, builder->getInt8Ty());
            llvm::Value* R8 = builder->CreateZExt(R, builder->getInt8Ty());
            llvm::Value* result8 = builder->CreateCall(fn, {L8, R8});
            return builder->CreateTrunc(result8, builder->getIntNTy(2));
        }
        cg_error((*bin)->op_tok.pos, "&&& requires qbool operands", "QC-S134");
        return nullptr;
    case TokenType::QOR:
        if (lty == builder->getIntNTy(2) && rty == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qor");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt8Ty(), {builder->getInt8Ty(), builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qor", module);
            }
            llvm::Value* L8 = builder->CreateZExt(L, builder->getInt8Ty());
            llvm::Value* R8 = builder->CreateZExt(R, builder->getInt8Ty());
            llvm::Value* result8 = builder->CreateCall(fn, {L8, R8});
            return builder->CreateTrunc(result8, builder->getIntNTy(2));
        }
        cg_error((*bin)->op_tok.pos, "||| requires qbool operands", "QC-S135");
        return nullptr;

    case TokenType::QXOR:
        if (lty == builder->getIntNTy(2) && rty == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qxor");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt8Ty(), {builder->getInt8Ty(), builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qxor", module);
            }
            llvm::Value* L8 = builder->CreateZExt(L, builder->getInt8Ty());
            llvm::Value* R8 = builder->CreateZExt(R, builder->getInt8Ty());
            llvm::Value* result8 = builder->CreateCall(fn, {L8, R8});
            return builder->CreateTrunc(result8, builder->getIntNTy(2));
        }
        cg_error((*bin)->op_tok.pos, "^^ requires qbool operands", "QC-S136");
        return nullptr;
    case TokenType::COLLAPSE_AND:
        if (lty == builder->getIntNTy(2) && rty == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qand_collapse");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {builder->getInt8Ty(), builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qand_collapse", module);
            }
            llvm::Value* L8 = builder->CreateZExt(L, builder->getInt8Ty());
            llvm::Value* R8 = builder->CreateZExt(R, builder->getInt8Ty());
            return builder->CreateCall(fn, {L8, R8});
        }
        cg_error((*bin)->op_tok.pos, "&|& requires qbool operands", "QC-S137");
        return nullptr;
    case TokenType::COLLAPSE_OR:
        if (lty == builder->getIntNTy(2) && rty == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qor_collapse");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt1Ty(), {builder->getInt8Ty(), builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qor_collapse", module);
            }
            llvm::Value* L8 = builder->CreateZExt(L, builder->getInt8Ty());
            llvm::Value* R8 = builder->CreateZExt(R, builder->getInt8Ty());
            return builder->CreateCall(fn, {L8, R8});
        }
        cg_error((*bin)->op_tok.pos, "|&| requires qbool operands", "QC-S138");
        return nullptr;
    default: break;
    }
    return nullptr;
}
llvm::Value* LLVMCompiler::emitVarAssign(VarAssignNode* const* va) {
    std::string name = (*va)->var_name_tok.value;
    add_var_warning(name, (*va)->var_name_tok.pos, (*va)->is_const);
    std::string qcType = (*va)->type_tok.value;
    bool isVolatile = false;
    if (qcType.starts_with("volatile ")) {
        isVolatile = true;
        qcType = qcType.substr(9, qcType.length() - 9);
    }
    if (qcType == "auto") {
        warn("auto", get_pos(*va), "used implicit variable type", "QC-W009");
        llvm::Value* rhs = emitExpr((*va)->value_node);
        if (!rhs) {
            cg_error((*va)->var_name_tok.pos, "cannot infer type from invalid expression", "QC-T026");
            return nullptr;
        }

        llvm::Type* inferredTy = rhs->getType();
        llvm::AllocaInst* alloc = createEntryAlloca(name, inferredTy);
        builder->CreateStore(rhs, alloc, isVolatile);
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = alloc;
        if (inferredTy->isArrayTy()) {
            llvm::Type* elemTy = inferredTy;
            while (elemTy->isArrayTy()) { elemTy = elemTy->getArrayElementType(); }
            if (elemTy->isIntegerTy(32))
                arrayTypeStrings[name] = "int";
            else if (elemTy->isIntegerTy(16))
                arrayTypeStrings[name] = "short int";
            else if (elemTy->isIntegerTy(64))
                arrayTypeStrings[name] = "long int";
            else if (elemTy->isFloatTy())
                arrayTypeStrings[name] = "float";
            else if (elemTy->isDoubleTy())
                arrayTypeStrings[name] = "double";
            else if (elemTy->isIntegerTy(8))
                arrayTypeStrings[name] = "char";
            else if (elemTy->isIntegerTy(1))
                arrayTypeStrings[name] = "bool";
            else if (elemTy->isIntegerTy(4))
                arrayTypeStrings[name] = "nibble";
            else if (elemTy->isIntegerTy(2))
                arrayTypeStrings[name] = "qbool";
            else if (elemTy->isPointerTy())
                arrayTypeStrings[name] = "string";
        } else {
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
        }

        return nullptr;
    }
    if (qcType == "auto[]" || qcType.starts_with("auto[")) {
        warn("auto", get_pos(*va), "used implicit variable type", "QC-W009");
        llvm::Value* rhs = emitExpr((*va)->value_node);
        if (!rhs) {
            cg_error((*va)->var_name_tok.pos, "cannot infer array type", "QC-T027");
            return nullptr;
        }

        llvm::Type* rhsTy = rhs->getType();

        if (!rhsTy->isArrayTy()) {
            cg_error((*va)->var_name_tok.pos, "auto[] requires array literal", "QC-S139");
            return nullptr;
        }

        llvm::AllocaInst* alloc = createEntryAlloca(name, rhsTy);
        builder->CreateStore(rhs, alloc, isVolatile);
        name = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[name] = alloc;
        llvm::Type* elemTy = rhsTy->getArrayElementType();
        if (elemTy->isIntegerTy(32))
            arrayTypeStrings[name] = "int";
        else if (elemTy->isIntegerTy(16))
            arrayTypeStrings[name] = "short int";
        else if (elemTy->isIntegerTy(64))
            arrayTypeStrings[name] = "long int";
        else if (elemTy->isFloatTy())
            arrayTypeStrings[name] = "float";
        else if (elemTy->isDoubleTy())
            arrayTypeStrings[name] = "double";
        else if (elemTy->isIntegerTy(8))
            arrayTypeStrings[name] = "char";
        else if (elemTy->isIntegerTy(4))
            arrayTypeStrings[name] = "nibble";
        else if (elemTy->isIntegerTy(1))
            arrayTypeStrings[name] = "bool";
        else if (elemTy->isIntegerTy(2))
            arrayTypeStrings[name] = "qbool";
        else if (elemTy->isPointerTy())
            arrayTypeStrings[name] = "string";

        arrayLengths[name] = rhsTy->getArrayNumElements();
        volatileVars[name] = isVolatile;
        return nullptr;
    }

    std::string saved_qc_type = qcType;
    qcType = resolveTypeName(qcType);
    if ((*va)->is_foreign) {
        llvm::GlobalVariable* global = module->getGlobalVariable(name);
        if (!global) {
            global = new llvm::GlobalVariable(*module, llvmTypeFor(qcType), false, llvm::GlobalValue::ExternalLinkage, nullptr, name);
            globals[name] = global;
            varTypes[name] = qcType;
            volatileVars[name] = isVolatile;
        }
        return nullptr;
    }
    if (genericClasses.count(qcType) && genericClasses[qcType]) {
        std::string savedest_qc_type = saved_qc_type;
        std::string inner = saved_qc_type.substr(saved_qc_type.find('<') + 1, saved_qc_type.size() - saved_qc_type.find('<') - 2);
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
        auto userTypeIt = userTypes.find(qcType);
        llvm::StructType* classTy = generateGenericClass(qcType, userTypeIt->second, genericParams);
        if (classTy == nullptr) {
            cg_error((*va)->var_name_tok.pos, "failed to generate generic subset for class " + qcType, "QC-G013");
            return nullptr;
        }
        llvm::AllocaInst* instance = createEntryAlloca(name, classTy);
        if ((*va)->value_node.index() == 0) {
            llvm::Value* zeroVal = llvm::ConstantAggregateZero::get(classTy);
            builder->CreateStore(zeroVal, instance, isVolatile);
            auto vtableIt = vtables.find(qcType);
            if (vtableIt != vtables.end()) {
                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                builder->CreateStore(vtableIt->second, vptrField);
            }
            for (auto& method : userTypeIt->second.classMethods) {
                if (method.is_constructor && method.params.empty()) {
                    llvm::Function* ctor = findMethodOverload(qcType, method.name_tok.value, {});
                    if (ctor) {
                        std::vector<llvm::Value*> args = {instance};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            builder->CreateInvoke(ctor, contBB, currentLandingPad(), args);
                            builder->SetInsertPoint(contBB);
                        } else {
                            builder->CreateCall(ctor, args);
                        }
                    }
                    break;
                }
            }
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = instance;
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        if (auto call = std::get_if<CallNode*>(&(*va)->value_node)) {
            bool handled = false;
            if (auto varAccess = std::get_if<VarAccessNode*>(&(*call)->node_to_call)) {
                std::string calledName = (*varAccess)->var_name_tok.value;
                if (calledName == buildMangledName(qcType, genericParams)) {
                    std::string ctorMethodName = "";
                    for (auto& method : userTypeIt->second.classMethods) {
                        if (method.is_constructor) {
                            ctorMethodName = method.name_tok.value;
                            break;
                        }
                    }

                    if (!ctorMethodName.empty()) {
                        std::vector<llvm::Value*> args;
                        for (auto& argNode : (*call)->arg_nodes) {
                            llvm::Value* arg = emitExpr(argNode);
                            if (!arg) return nullptr;
                            args.push_back(arg);
                        }
                        std::string mangledName = buildMangledName(qcType, genericParams);
                        llvm::Function* ctor = findMethodOverload(mangledName, ctorMethodName, args);
                        if (ctor) {
                            std::vector<llvm::Value*> allArgs = {instance};
                            allArgs.insert(allArgs.end(), args.begin(), args.end());
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                builder->CreateInvoke(ctor, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                            } else {
                                builder->CreateCall(ctor, allArgs);
                            }
                            std::string mangledName = buildMangledName(qcType, genericParams);
                            auto vtableIt = vtables.find(mangledName);
                            if (vtableIt != vtables.end()) {
                                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                                builder->CreateStore(vtableIt->second, vptrField);
                            }
                            handled = true;
                        }
                    }
                }
            }
            if (!handled) {
                llvm::Value* rhs = emitExpr((*va)->value_node);
                if (!rhs) {
                    warn("uninitialized", get_pos(*va), "declared variable without initializer", "QC-W010");
                    return nullptr;
                }
                if (rhs->getType()->isPointerTy()) {
                    if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(classTy)) {
                        rhs = builder->CreateLoad(classTy, rhs, "strip_ref");
                    }
                }
                if (rhs->getType() != classTy) {
                    cg_error((*va)->var_name_tok.pos, "cannot initialize " + qcType + " from class of different type.", "QC-T028");
                    return nullptr;
                }
                builder->CreateStore(rhs, instance, isVolatile);
            }
        } else if (auto arrLit = std::get_if<ArrayLiteralNode*>(&(*va)->value_node)) {
            llvm::Value* rhsVal = emitExpr(*arrLit);
            llvm::Value* len = builder->getInt32((*arrLit)->elements.size());
            rhsVal = decayArrayToPointer(rhsVal);
            if (rhsVal == nullptr) { return nullptr; }
            llvm::Function* opMethod = findMethodOverload(buildMangledName(qcType, genericParams), "operator[]=", {rhsVal, len});
            std::string mangledName = buildMangledName(qcType, genericParams);
            if (opMethod) {
                emitMethodCall(opMethod, instance, {rhsVal, len}, "operator[]=");
                std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
                locals[fullName] = instance;
                varTypes[fullName] = mangledName;
                volatileVars[fullName] = isVolatile;
            } else {
                cg_error((*va)->var_name_tok.pos, "no valid operator[]= method found on class " + qcType, "QC-S140");
                return nullptr;
            }
            auto vtableIt = vtables.find(mangledName);
            if (vtableIt != vtables.end()) {
                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                builder->CreateStore(vtableIt->second, vptrField);
            }
            return nullptr;
        } else {
            llvm::Value* rhs = emitExpr((*va)->value_node);
            if (!rhs) {
                warn("uninitialized", get_pos(*va), "declared variable without initializer", "QC-W010");
                return nullptr;
            }
            if (rhs->getType()->isPointerTy()) {
                if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(classTy)) {
                    rhs = builder->CreateLoad(classTy, rhs, "strip_ref");
                }
            }
            if (rhs->getType() != classTy) {
                cg_error((*va)->var_name_tok.pos, "cannot initialize " + qcType + " from class of different type.", "QC-T028");
                return nullptr;
            }
            builder->CreateStore(rhs, instance, isVolatile);
        }
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = instance;
        volatileVars[fullName] = isVolatile;
        varTypes[fullName] = buildMangledName(qcType, genericParams);
        return nullptr;
    }
    auto userTypeIt = userTypes.find(qcType);
    if (genericStructs.count(qcType) && genericStructs[qcType]) {
        std::string savedest_qc_type = saved_qc_type;
        std::string inner = saved_qc_type.substr(saved_qc_type.find('<') + 1, saved_qc_type.size() - saved_qc_type.find('<') - 2);
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
        auto userTypeIt = userTypes.find(qcType);
        llvm::StructType* structTy = generateGenericStruct(qcType, userTypeIt->second, genericParams);
        if (structTy == nullptr) {
            cg_error((*va)->var_name_tok.pos, "failed to generate generic subset for struct " + qcType, "QC-G014");
            return nullptr;
        }
        if ((*va)->value_node.index() == 0) {
            llvm::AllocaInst* structAlloc = createEntryAlloca(name, structTy);
            llvm::Value* zeroVal = llvm::ConstantAggregateZero::get(structTy);
            builder->CreateStore(zeroVal, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = structAlloc;
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        if (auto arrLit = std::get_if<ArrayLiteralNode*>(&(*va)->value_node)) {
            llvm::StructType* structTy = genericiseOrFindStruct(buildMangledName(qcType, genericParams));
            if (!(*arrLit)->type.empty()) {
                if (buildMangledName(qcType, genericParams) != fixMangling(resolveTypeName((*arrLit)->type, false))) {
                    cg_error(get_pos(*va), "cannot initialize struct with literal of different struct type", "QC-T029");
                    cg_note(get_pos(*arrLit),
                            "got type " + fixMangling(resolveTypeName((*arrLit)->type, false)) + ", expected " +
                                buildMangledName(qcType, genericParams),
                            true);
                    return nullptr;
                }
            }
            llvm::Value* structVal = llvm::ConstantAggregateZero::get(structTy);
            auto& structInfo = userTypeIt->second;
            for (size_t i = 0; i < (*arrLit)->elements.size(); i++) {
                std::string fieldType = structInfo.fields[i].type;
                auto fieldTypeIt = userTypes.find(fieldType);
                llvm::Value* val;
                if (fieldTypeIt != userTypes.end() && fieldTypeIt->second.kind == UserTypeKind::Struct) {
                    if (auto nestedArrLit = std::get_if<ArrayLiteralNode*>(&(*arrLit)->elements[i])) {
                        llvm::StructType* nestedStructTy = genericiseOrFindStruct(fieldType);
                        llvm::Value* nestedStruct = llvm::ConstantAggregateZero::get(nestedStructTy);
                        for (size_t j = 0; j < (*nestedArrLit)->elements.size(); j++) {
                            llvm::Value* fieldVal = emitExpr((*nestedArrLit)->elements[j]);
                            if (!fieldVal) return nullptr;
                            nestedStruct = builder->CreateInsertValue(nestedStruct, fieldVal, j);
                        }
                        val = nestedStruct;
                    } else {
                        val = emitExpr((*arrLit)->elements[i]);
                        if (!val) return nullptr;
                    }
                } else {
                    val = emitExpr((*arrLit)->elements[i]);
                    if (!val) return nullptr;
                }
                structVal = builder->CreateInsertValue(structVal, val, i);
            }
            llvm::AllocaInst* structAlloc = createEntryAlloca(name, structTy);
            builder->CreateStore(structVal, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = structAlloc;
            varTypes[fullName] = buildMangledName(qcType, genericParams);
            volatileVars[fullName] = isVolatile;
            return nullptr;
        } else if (auto mapLit = std::get_if<MapLiteralNode*>(&(*va)->value_node)) {
            llvm::StructType* structTy = genericiseOrFindStruct(buildMangledName(qcType, genericParams));
            if (!(*mapLit)->struct_type.empty()) {
                if (buildMangledName(qcType, genericParams) != fixMangling(resolveTypeName((*mapLit)->struct_type, false))) {
                    cg_error(get_pos(*va), "cannot initialize struct with literal of different struct type", "QC-T029");
                    cg_note(get_pos(*mapLit),
                            "got type " + fixMangling(resolveTypeName((*mapLit)->struct_type, false)) + ", expected " +
                                buildMangledName(qcType, genericParams),
                            true);
                    return nullptr;
                }
            }
            llvm::Value* structVal = llvm::ConstantAggregateZero::get(structTy);
            auto& structInfo = userTypeIt->second;
            for (auto& [keyNode, valueNode] : (*mapLit)->pairs) {
                std::string fieldName;
                if (auto key = std::get_if<VarAccessNode*>(&keyNode)) {
                    fieldName = (*key)->var_name_tok.value;
                } else if (auto key = std::get_if<StringNode>(&keyNode)) {
                    fieldName = key->tok.value;
                } else {
                    cg_error((*mapLit)->pos, "struct field name must be an identifier", "QC-S141");
                    return nullptr;
                }
                int fieldIndex = -1;
                for (size_t i = 0; i < structInfo.fields.size(); i++) {
                    if (structInfo.fields[i].name == fieldName) {
                        fieldIndex = i;
                        break;
                    }
                }
                if (fieldIndex == -1) {
                    cg_error((*mapLit)->pos, "unknown field '" + fieldName + "' in struct " + qcType, "QC-S142");
                    return nullptr;
                }
                llvm::Value* fieldValue = emitExpr(valueNode);
                if (!fieldValue) return nullptr;
                structVal = builder->CreateInsertValue(structVal, fieldValue, fieldIndex);
            }
            llvm::AllocaInst* structAlloc = createEntryAlloca(name, structTy);
            builder->CreateStore(structVal, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = structAlloc;
            varTypes[fullName] = buildMangledName(qcType, genericParams);
            volatileVars[fullName] = isVolatile;
            return nullptr;
        } else {
            llvm::StructType* structTy = genericiseOrFindStruct(buildMangledName(qcType, genericParams));
            llvm::Value* rhs = emitExpr((*va)->value_node);
            if (!rhs) return nullptr;
            if (rhs->getType()->isPointerTy()) {
                if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(structTy)) {
                    rhs = builder->CreateLoad(structTy, rhs, "strip_ref");
                }
            }
            if (rhs->getType() != structTy) {
                cg_error((*va)->var_name_tok.pos, "cannot initialize " + buildMangledName(qcType, genericParams) + " from struct of different type.",
                         "QC-T028");
                return nullptr;
            }
            llvm::Value* structAlloc = getVarAddress(name);
            if (!structAlloc) structAlloc = createEntryAlloca(name, structTy);
            builder->CreateStore(rhs, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = llvm::cast<llvm::AllocaInst>(structAlloc);
            varTypes[fullName] = buildMangledName(qcType, genericParams);
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        return nullptr;
    }
    if (genericUnions.count(qcType) && genericUnions[qcType]) {
        UserTypeInfo info = genericiseOrFindUnion(saved_qc_type);
        llvm::StructType* unionTy = unionTypes[resolveTypeName(saved_qc_type, false)];
        llvm::AllocaInst* unionAlloc = createEntryAlloca(name, unionTy);
        llvm::Value* rhs = emitExpr((*va)->value_node);
        if (!rhs) return nullptr;
        if (rhs->getType() == unionTy) {
            builder->CreateStore(rhs, unionAlloc);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = unionAlloc;
            return nullptr;
        }
        int tag = findUnionVariantTag(qcType, (*va)->value_node, rhs);

        if (tag == -1) {
            cg_error((*va)->var_name_tok.pos, "value does not match any variant of union " + qcType, "QC-S143");
            return nullptr;
        }
        auto& member = info.members[tag];
        bool isLiteral = member.type.find(':') != std::string::npos;
        llvm::Type* rhsTy = rhs->getType();
        std::string baseType = isLiteral ? member.type.substr(0, member.type.find(':')) : member.type;
        llvm::Type* memberTy = llvmTypeFor(baseType);
        if (!rhsTy->isPointerTy() && rhsTy != memberTy) {
            cg_error((*va)->var_name_tok.pos, "union literal variant type mismatch", "QC-T030");
            return nullptr;
        }
        llvm::Value* unionVal = llvm::ConstantAggregateZero::get(unionTy);
        unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);
        llvm::Value* dataPtr = storeAndGetPointer(rhs);
        unionVal = builder->CreateInsertValue(unionVal, dataPtr, 1);
        builder->CreateStore(unionVal, unionAlloc, isVolatile);
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = unionAlloc;
        varTypes[fullName] = fixMangling(saved_qc_type);
        volatileVars[fullName] = isVolatile;
        return nullptr;
    }
    if (userTypeIt != userTypes.end() && userTypeIt->second.kind == UserTypeKind::Struct) {
        if ((*va)->value_node.index() == 0) {
            llvm::StructType* structTy = genericiseOrFindStruct(qcType);
            llvm::AllocaInst* structAlloc = createEntryAlloca(name, structTy);
            llvm::Value* zeroVal = llvm::ConstantAggregateZero::get(structTy);
            builder->CreateStore(zeroVal, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = structAlloc;
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        if (auto arrLit = std::get_if<ArrayLiteralNode*>(&(*va)->value_node)) {
            llvm::StructType* structTy = genericiseOrFindStruct(qcType);
            if (!(*arrLit)->type.empty()) {
                if (qcType != fixMangling(resolveTypeName((*arrLit)->type, true))) {
                    cg_error(get_pos(*va), "cannot initialize struct with literal of different struct type", "QC-T029");
                    cg_note(get_pos(*arrLit), "got type " + fixMangling(resolveTypeName((*arrLit)->type, true)) + ", expected " + qcType, true);
                    return nullptr;
                }
            }
            llvm::Value* structVal = llvm::Constant::getNullValue(structTy);
            auto& structInfo = userTypeIt->second;
            for (size_t i = 0; i < (*arrLit)->elements.size(); i++) {
                std::string fieldType = structInfo.fields[i].type;
                auto fieldTypeIt = userTypes.find(fieldType);
                llvm::Value* val;

                if (fieldTypeIt != userTypes.end() && fieldTypeIt->second.kind == UserTypeKind::Struct) {
                    if (auto nestedArrLit = std::get_if<ArrayLiteralNode*>(&(*arrLit)->elements[i])) {
                        llvm::StructType* nestedStructTy = genericiseOrFindStruct(fieldType);
                        llvm::Value* nestedStruct = llvm::Constant::getNullValue(nestedStructTy);
                        for (size_t j = 0; j < (*nestedArrLit)->elements.size(); j++) {
                            llvm::Value* fieldVal = emitExpr((*nestedArrLit)->elements[j]);
                            if (!fieldVal) return nullptr;
                            nestedStruct = builder->CreateInsertValue(nestedStruct, fieldVal, j);
                        }

                        val = nestedStruct;
                    } else {
                        val = emitExpr((*arrLit)->elements[i]);
                        if (!val) return nullptr;
                    }
                } else {
                    val = emitExpr((*arrLit)->elements[i]);
                    if (!val) return nullptr;
                }
                structVal = builder->CreateInsertValue(structVal, val, i);
            }
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            llvm::Value* structAlloc = getVarAddress(name);
            if (auto* gv = llvm::dyn_cast_or_null<llvm::GlobalVariable>(structAlloc)) {
                auto* constant = llvm::dyn_cast<llvm::Constant>(structVal);
                if (!constant) {
                    cg_error((*va)->var_name_tok.pos, "global struct initializer must be constant", "QC-S144");
                    return nullptr;
                }
                gv->setInitializer(constant);
                return nullptr;
            } else {
                if (!structAlloc) structAlloc = createEntryAlloca(name, structTy);

                builder->CreateStore(structVal, structAlloc, isVolatile);
            }
            locals[fullName] = llvm::cast<llvm::AllocaInst>(structAlloc);
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        } else if (auto mapLit = std::get_if<MapLiteralNode*>(&(*va)->value_node)) {
            llvm::StructType* structTy = genericiseOrFindStruct(qcType);
            if (!(*mapLit)->struct_type.empty()) {
                if (qcType != fixMangling(resolveTypeName((*mapLit)->struct_type, true))) {
                    cg_error(get_pos(*va), "cannot initialize struct with literal of different struct type", "QC-T029");
                    cg_note(get_pos(*mapLit), "got type " + fixMangling(resolveTypeName((*mapLit)->struct_type, true)) + ", expected " + qcType,
                            true);
                    return nullptr;
                }
            }
            llvm::Value* structVal = llvm::ConstantAggregateZero::get(structTy);

            auto& structInfo = userTypeIt->second;

            for (auto& [keyNode, valueNode] : (*mapLit)->pairs) {
                std::string fieldName;

                if (auto key = std::get_if<VarAccessNode*>(&keyNode)) {
                    fieldName = (*key)->var_name_tok.value;
                } else if (auto key = std::get_if<StringNode>(&keyNode)) {
                    fieldName = key->tok.value;
                } else {
                    cg_error((*mapLit)->pos, "struct field name must be an identifier", "QC-S141");
                    return nullptr;
                }

                int fieldIndex = -1;

                for (size_t i = 0; i < structInfo.fields.size(); i++) {
                    if (structInfo.fields[i].name == fieldName) {
                        fieldIndex = i;
                        break;
                    }
                }

                if (fieldIndex == -1) {
                    cg_error((*mapLit)->pos, "unknown field '" + fieldName + "' in struct " + qcType, "QC-S142");
                    return nullptr;
                }

                llvm::Value* fieldValue = emitExpr(valueNode);

                if (!fieldValue) return nullptr;

                structVal = builder->CreateInsertValue(structVal, fieldValue, fieldIndex);
            }
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            llvm::Value* structAlloc = getVarAddress(name);
            if (auto* gv = llvm::dyn_cast_or_null<llvm::GlobalVariable>(structAlloc)) {
                auto* constant = llvm::dyn_cast<llvm::Constant>(structVal);
                if (!constant) {
                    cg_error((*va)->var_name_tok.pos, "global struct initializer must be constant", "QC-S144");
                    return nullptr;
                }
                gv->setInitializer(constant);
                return nullptr;
            } else {
                if (!structAlloc) structAlloc = createEntryAlloca(name, structTy);

                builder->CreateStore(structVal, structAlloc, isVolatile);
            }
            locals[fullName] = llvm::cast<llvm::AllocaInst>(structAlloc);
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        } else {
            llvm::StructType* structTy = genericiseOrFindStruct(qcType);
            llvm::Value* rhs = emitExpr((*va)->value_node);
            if (!rhs) return nullptr;
            if (rhs->getType()->isPointerTy()) {
                if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(structTy)) {
                    rhs = builder->CreateLoad(structTy, rhs, "strip_ref");
                }
            }
            if (rhs->getType() != structTy) {
                cg_error((*va)->var_name_tok.pos, "cannot initialize " + qcType + " from struct of different type.", "QC-T028");
                return nullptr;
            }
            llvm::Value* structAlloc = getVarAddress(name);
            if (!structAlloc) structAlloc = createEntryAlloca(name, structTy);
            builder->CreateStore(rhs, structAlloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = llvm::cast<llvm::AllocaInst>(structAlloc);
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
    }
    if (userTypeIt != userTypes.end() && userTypeIt->second.kind == UserTypeKind::Class) {
        llvm::StructType* classTy = genericiseOrFindClass(qcType);
        llvm::AllocaInst* instance = createEntryAlloca(name, classTy);
        if ((*va)->value_node.index() == 0) {
            llvm::Value* zeroVal = llvm::ConstantAggregateZero::get(classTy);
            builder->CreateStore(zeroVal, instance, isVolatile);
            auto vtableIt = vtables.find(qcType);
            if (vtableIt != vtables.end()) {
                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                builder->CreateStore(vtableIt->second, vptrField);
            }
            for (auto& method : userTypeIt->second.classMethods) {
                if (method.is_constructor && method.params.empty()) {
                    llvm::Function* ctor = findMethodOverload(qcType, method.name_tok.value, {});
                    if (ctor) {
                        std::vector<llvm::Value*> args = {instance};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            builder->CreateInvoke(ctor, contBB, currentLandingPad(), args);
                            builder->SetInsertPoint(contBB);
                        } else {
                            builder->CreateCall(ctor, args);
                        }
                    }
                    break;
                }
            }
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = instance;
            varTypes[fullName] = qcType;
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        if (auto call = std::get_if<CallNode*>(&(*va)->value_node)) {
            bool handled = false;
            if (auto varAccess = std::get_if<VarAccessNode*>(&(*call)->node_to_call)) {
                std::string calledName = (*varAccess)->var_name_tok.value;

                if (calledName == qcType) {
                    std::string ctorMethodName = "";
                    for (auto& method : userTypeIt->second.classMethods) {
                        if (method.is_constructor) {
                            ctorMethodName = method.name_tok.value;
                            break;
                        }
                    }

                    if (!ctorMethodName.empty()) {
                        std::vector<llvm::Value*> args;
                        for (auto& argNode : (*call)->arg_nodes) {
                            llvm::Value* arg = emitExpr(argNode);
                            if (!arg) return nullptr;
                            args.push_back(arg);
                        }
                        llvm::Function* ctor = findMethodOverload(qcType, ctorMethodName, args);

                        if (ctor) {
                            std::vector<llvm::Value*> allArgs = {instance};
                            allArgs.insert(allArgs.end(), args.begin(), args.end());
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                builder->CreateInvoke(ctor, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                            } else {
                                builder->CreateCall(ctor, allArgs);
                            }
                            auto vtableIt = vtables.find(qcType);
                            if (vtableIt != vtables.end()) {
                                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                                builder->CreateStore(vtableIt->second, vptrField);
                            }
                            handled = true;
                        }
                    }
                }
            }
            if (!handled) {
                llvm::Value* rhs = emitExpr((*va)->value_node);
                if (!rhs) return nullptr;
                if (rhs->getType()->isPointerTy()) {
                    if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(classTy)) {
                        rhs = builder->CreateLoad(classTy, rhs, "strip_ref");
                    }
                }
                if (rhs->getType() != classTy) {
                    cg_error((*va)->var_name_tok.pos, "cannot initialize " + qcType + " from class of different type.", "QC-T028");
                    return nullptr;
                }
                builder->CreateStore(rhs, instance, isVolatile);
            }
        } else if (auto arrLit = std::get_if<ArrayLiteralNode*>(&(*va)->value_node)) {
            llvm::Value* rhsVal = emitExpr(*arrLit);
            llvm::Value* len = builder->getInt32((*arrLit)->elements.size());
            rhsVal = decayArrayToPointer(rhsVal);
            if (rhsVal == nullptr) { return nullptr; }
            llvm::Function* opMethod = findMethodOverload(qcType, "operator[]=", {rhsVal, len});
            if (opMethod) {
                emitMethodCall(opMethod, instance, {rhsVal, len}, "operator[]=");
                std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
                locals[fullName] = instance;
                varTypes[fullName] = qcType;
                volatileVars[fullName] = isVolatile;
            } else {
                cg_error((*va)->var_name_tok.pos, "no valid operator[]= method found on class " + qcType, "QC-S140");
                addMethodNotes(qcType, "operator[]=", {len}, (*va)->var_name_tok.pos);
                return nullptr;
            }
            auto vtableIt = vtables.find(qcType);
            if (vtableIt != vtables.end()) {
                llvm::Value* vptrField = builder->CreateStructGEP(classTy, instance, 0, "vptr_field");
                builder->CreateStore(vtableIt->second, vptrField);
            }
            return nullptr;
        } else {
            llvm::Value* rhs = emitExpr((*va)->value_node);
            if (!rhs) return nullptr;
            if (rhs->getType()->isPointerTy()) {
                if (llvmTypeFor(getExpressionType((*va)->value_node)) == llvm::dyn_cast_or_null<llvm::Type>(classTy)) {
                    rhs = builder->CreateLoad(classTy, rhs, "strip_ref");
                }
            }
            if (rhs->getType() != classTy) {
                cg_error((*va)->var_name_tok.pos, "cannot initialize " + qcType + " from class of different type.", "QC-T028");
                return nullptr;
            }
            builder->CreateStore(rhs, instance, isVolatile);
        }
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = instance;
        varTypes[fullName] = qcType;
        volatileVars[fullName] = isVolatile;
        return nullptr;
    }
    if (userTypeIt != userTypes.end() && userTypeIt->second.kind == UserTypeKind::Union) {
        llvm::StructType* unionTy = unionTypes[qcType];
        llvm::Value* unionAlloc = getVarAddress(name);
        if (!unionAlloc) unionAlloc = createEntryAlloca(name, unionTy);
        llvm::Value* rhs = emitExpr((*va)->value_node);
        if (!rhs) return nullptr;
        auto storeUnion = [&](llvm::Value* value) {
            if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(unionAlloc)) {
                auto* constant = llvm::dyn_cast<llvm::Constant>(value);
                if (!constant) {
                    cg_error((*va)->var_name_tok.pos, "global union initializer must be constant", "QC-S145");
                    return false;
                }
                gv->setInitializer(constant);
                return false;
            } else {
                builder->CreateStore(value, unionAlloc, isVolatile);
            }
            return true;
        };
        if (rhs->getType() == unionTy) {
            if (!storeUnion(rhs)) return nullptr;
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = llvm::cast<llvm::AllocaInst>(unionAlloc);
            volatileVars[fullName] = isVolatile;
            return nullptr;
        }
        int tag = findUnionVariantTag(qcType, (*va)->value_node, rhs);
        if (tag == -1) {
            cg_error((*va)->var_name_tok.pos, "value does not match any variant of union " + qcType, "QC-S143");
            return nullptr;
        }
        auto& member = userTypes.at(baseTypeName(qcType)).members[tag];
        bool isLiteral = member.type.find(':') != std::string::npos;
        llvm::Type* rhsTy = rhs->getType();
        std::string baseType = isLiteral ? member.type.substr(0, member.type.find(':')) : member.type;
        llvm::Type* memberTy = llvmTypeFor(baseType);
        if (!rhsTy->isPointerTy() && rhsTy != memberTy) {
            cg_error((*va)->var_name_tok.pos, "union literal variant type mismatch", "QC-T030");
            return nullptr;
        }
        llvm::Value* unionVal = llvm::ConstantAggregateZero::get(unionTy);
        unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);
        llvm::Value* dataPtr = storeAndGetPointer(rhs);
        unionVal = builder->CreateInsertValue(unionVal, dataPtr, 1);
        if (!storeUnion(unionVal)) return nullptr;
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = llvm::cast<llvm::AllocaInst>(unionAlloc);
        varTypes[fullName] = qcType;
        volatileVars[fullName] = isVolatile;
        return nullptr;
    }
    if (userTypeIt != userTypes.end() && userTypeIt->second.kind == UserTypeKind::Enum) {
        llvm::Type* enumTy = genericiseOrFindEnum(resolveTypeName(saved_qc_type, false));
        if (!enumTy) return nullptr;
        llvm::Value* enumAlloc = getVarAddress(name);
        if (!enumAlloc) enumAlloc = createEntryAlloca(name, enumTy);
        llvm::Value* rhs = emitExpr((*va)->value_node);
        if (!rhs) return nullptr;
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(enumAlloc)) {
            auto* constant = llvm::dyn_cast<llvm::Constant>(rhs);
            if (!constant) {
                cg_error((*va)->var_name_tok.pos, "global enum initializer must be constant", "QC-S146");
                return nullptr;
            }
            gv->setInitializer(constant);
            return nullptr;
        } else {
            builder->CreateStore(rhs, enumAlloc, isVolatile);
        }
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = llvm::cast<llvm::AllocaInst>(enumAlloc);
        varTypes[fullName] = fixMangling(resolveTypeName(saved_qc_type, false));
        volatileVars[fullName] = isVolatile;
        return nullptr;
    }
    if (qcType.find("[]") != std::string::npos) {
        std::string baseType = qcType;
        while (baseType.ends_with("[]")) { baseType = baseType.substr(0, baseType.length() - 2); }
        arrayTypeStrings[name] = baseType;
    }
    llvm::AllocaInst* alloc = nullptr;
    if ((*va)->type_tok.value == "function" || (*va)->type_tok.value.starts_with("fn(") || (*va)->type_tok.value.starts_with("fn (") ||
        (*va)->type_tok.value == "auto" && std::holds_alternative<FuncDefNode*>((*va)->value_node)) {
        auto fnPtr = std::get<FuncDefNode*>((*va)->value_node);
        llvm::Function* f = emitFuncDef(*fnPtr);
        name = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        lambdaTypes[name] = f->getFunctionType();
        llvm::Type* funcPtrTy = llvm::PointerType::get(context, 0);
        alloc = createEntryAlloca(name, funcPtrTy);

        locals[name] = alloc;
        volatileVars[name] = isVolatile;
        builder->CreateStore(f, alloc, isVolatile);
        return nullptr;
    }
    llvm::Value* existingAlloc = getVarAddress(name);
    std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
    if (!existingAlloc) {
        llvm::Type* ty = llvmTypeFor(qcType);
        if (!ty) {
            cg_error((*va)->var_name_tok.pos, "unknown type: " + qcType, "QC-T031");
            return nullptr;
        }
        alloc = createEntryAlloca(fullName, ty);
        locals[fullName] = alloc;
        varTypes[fullName] = qcType;
        volatileVars[fullName] = isVolatile;
    } else {
        if (auto* existingLocal = llvm::dyn_cast<llvm::AllocaInst>(existingAlloc)) {
            llvm::Type* existingTy = existingLocal->getAllocatedType();
            llvm::Type* newTy = llvmTypeFor(qcType);
            if (existingTy != newTy) {
                warn("shadow", get_pos(*va), "declaration of '" + fullName + "' shadows a previous declaration of '" + fullName + "'", "QC-W006");
                static int shadowId = 0;
                std::string uniqueName = fullName + ".shadow." + std::to_string(shadowId++);
                alloc = createEntryAlloca(uniqueName, newTy);
                locals[fullName] = alloc;
                varTypes[fullName] = qcType;
                volatileVars[fullName] = isVolatile;
            } else {
                alloc = existingLocal;
            }
        } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(existingAlloc)) {
            llvm::Value* rhs = emitExpr((*va)->value_node);
            if (rhs) {
                if (auto* constantRHS = llvm::dyn_cast<llvm::Constant>(rhs)) {
                    gv->setInitializer(constantRHS);
                } else {
                    builder->CreateStore(rhs, gv);
                }
            }
            return nullptr;
        }
    }
    llvm::Type* destTy = getPointeeType(fullName);
    llvm::Value* rhs = emitExpr((*va)->value_node);
    if (!rhs) {
        cg_error((*va)->var_name_tok.pos, "failed to compile initializer for '" + name + "'", "QC-S147");
        return nullptr;
    }

    llvm::Type* srcTy = rhs->getType();
    if (isUnionType(srcTy) && !isUnionType(destTy)) {
        llvm::Value* dataPtr = builder->CreateExtractValue(rhs, 1, "union_data");

        if (destTy->isPointerTy()) {
            rhs = builder->CreateBitCast(dataPtr, destTy);
        } else {
            llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
            rhs = builder->CreateLoad(destTy, typedPtr);
        }

        srcTy = destTy;
    }
    if (srcTy != destTy) {
        if (srcTy->isPointerTy() && !destTy->isPointerTy()) {
            llvm::Type* newbase = llvmTypeFor(getExpressionType((*va)->value_node));
            if (!newbase->isPointerTy()) {
                rhs = builder->CreateLoad(newbase, rhs);
                srcTy = newbase;
            }
        }
        if (srcTy->isFloatTy() && destTy->isDoubleTy()) {
            warn("implicit-extend", get_pos(*va), "implicit extension in assignment", "QC-W012");
            rhs = builder->CreateFPExt(rhs, destTy, "f2d");
        } else if (srcTy->isArrayTy() && destTy->isPointerTy()) {
            rhs = this->decayArrayToPointer(rhs);
        } else if (srcTy->isPointerTy() && destTy->isArrayTy()) {
            auto* arr_alloca = builder->CreateAlloca(srcTy);
            builder->CreateStore(rhs, arr_alloca);
            rhs = builder->CreateGEP(srcTy, arr_alloca, {builder->getInt32(0), builder->getInt32(0)});
        } else if (srcTy->isArrayTy() && destTy->isArrayTy()) {
            auto* srcArrTy = llvm::cast<llvm::ArrayType>(srcTy);
            auto* destArrTy = llvm::cast<llvm::ArrayType>(destTy);
            if (srcArrTy->getElementType() != destArrTy->getElementType()) {
                cg_error((*va)->var_name_tok.pos, "array element type mismatch in assignment", "QC-T032");
                return nullptr;
            }
            uint64_t srcLen = srcArrTy->getNumElements();
            uint64_t destLen = destArrTy->getNumElements();
            if (srcLen > destLen) {
                cg_error((*va)->var_name_tok.pos, "source array is larger than destination array", "QC-S148");
                return nullptr;
            }
            if (!rhs->getType()->isPointerTy()) {
                auto* tmp = createEntryAlloca("src_array_tmp", srcArrTy);
                builder->CreateStore(rhs, tmp);
                rhs = tmp;
            }
            llvm::AllocaInst* newArr = createEntryAlloca("array_copy", destArrTy);
            uint64_t bytes = srcLen * srcArrTy->getElementType()->getPrimitiveSizeInBits() / 8;
            builder->CreateMemCpy(newArr, llvm::MaybeAlign(), rhs, llvm::MaybeAlign(), bytes);
            if (destLen > srcLen) {
                llvm::Value* zeroStart = builder->CreateGEP(destArrTy, newArr, {builder->getInt32(0), builder->getInt32(srcLen)});
                uint64_t zeroBytes = (destLen - srcLen) * srcArrTy->getElementType()->getPrimitiveSizeInBits() / 8;
                if (config.use_runtime) builder->CreateMemSet(zeroStart, builder->getInt8(0), zeroBytes, llvm::MaybeAlign());
            }
            rhs = newArr;
        } else if (srcTy->isDoubleTy() && destTy->isFloatTy()) {
            cg_error((*va)->var_name_tok.pos, "cannot assign double to float (loses percision)", "QC-S149");
            return nullptr;
        } else if (srcTy->isIntegerTy() && destTy->isIntegerTy()) {
            unsigned srcBits = srcTy->getIntegerBitWidth();
            unsigned destBits = destTy->getIntegerBitWidth();
            if (srcBits > destBits) {
                warn("truncation", get_pos(*va), "implicit truncation in assignment", "QC-W011");
                rhs = builder->CreateTrunc(rhs, destTy, "trunc");
            } else if (srcBits < destBits) {
                warn("implicit-extend", get_pos(*va), "implicit extension in assignment", "QC-W012");
                rhs = builder->CreateSExt(rhs, destTy, "sext");
            }
        } else if (srcTy->isIntegerTy() && destTy->isFloatingPointTy()) {
            warn("implicit-int-float", get_pos(*va), "implicit cast between integer and decimal type in assignment", "QC-W013");
            rhs = builder->CreateSIToFP(rhs, destTy, "i2f");
        } else {
            cg_error((*va)->var_name_tok.pos, "type mismatch in assignment in compiled mode", "QC-T033");
            return nullptr;
        }
    }
    if (llvm::isa<llvm::ConstantAggregateZero>(rhs) && srcTy->isArrayTy()) {
        uint64_t bytes = module->getDataLayout().getTypeAllocSize(srcTy);
        if (config.use_runtime) builder->CreateMemSet(alloc, builder->getInt8(0), bytes, llvm::MaybeAlign(), isVolatile);
    } else {
        builder->CreateStore(rhs, alloc, isVolatile);
    }
    return nullptr;
}
llvm::Value* LLVMCompiler::emitVarAccess(VarAccessNode* const* acc) {
    std::string name = (*acc)->var_name_tok.value;
    if (name == "this") {
        if (currentThis) {
            return currentThis;
        } else {
            cg_error((*acc)->var_name_tok.pos, "'this' used outside class method", "QC-S150");
            return nullptr;
        }
    }
    if (currentNonTypeGenericValues.find(name) != currentNonTypeGenericValues.end()) {
        auto& entry = currentNonTypeGenericValues[name];
        llvm::Type* ty = llvmTypeFor(entry.nonTypeKind);
        if (ty->isIntegerTy()) {
            return llvm::ConstantInt::get(ty, std::stoull(entry.name), true);
        } else if (ty->isFloatingPointTy()) {
            return llvm::ConstantFP::get(ty, std::stod(entry.name));
        } else if (entry.nonTypeKind == "string") {
            return builder->CreateGlobalString(entry.name);
        }
    }
    llvm::Value* alloc = getVarAddress(name);
    if (alloc) {
        llvm::Type* ty = getPointeeType(name);
        if (ty == nullptr) {
            cg_error((*acc)->var_name_tok.pos, "could not resolve var type", "QC-T034");
            return nullptr;
        }
        return builder->CreateLoad(ty, alloc, resolveVolatileVar(name), name);
    }

    llvm::Function* fn = resolveFunction(name);
    if (fn) { return fn; }

    cg_error((*acc)->var_name_tok.pos, "use of undeclared variable '" + name + "'", "QC-S151");
    auto suggestions = getVisibleVariables();
    std::vector<std::pair<int, std::string>> matches;
    if (name.size() >= 3) {
        for (auto& vname : suggestions) {
            int distance = levenshteinDistance(name, vname);
            if (distance <= 2) { matches.push_back({distance, vname}); }
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
        cg_note((*acc)->var_name_tok.pos, note);
    }
    return nullptr;
}
llvm::Value* LLVMCompiler::emitAssignExpr(AssignExprNode* const* asn) {
    if (auto propAccess = std::get_if<PropertyAccessNode*>(&(*asn)->target)) {
        std::string fieldName = (*propAccess)->property_name.value;
        if (auto varAccess = std::get_if<VarAccessNode*>(&*(*propAccess)->base)) {
            std::string varName = (*varAccess)->var_name_tok.value;
            llvm::Value* locAlloc = getVarAddress(varName);
            if (!locAlloc) {
                cg_error(get_pos(*varAccess), "unknown variable: " + varName, "QC-S152");
                return nullptr;
            }
            llvm::Type* allocTy = getPointeeType(varName);
            auto structTy = llvm::dyn_cast<llvm::StructType>(allocTy);
            if (!structTy) {
                cg_error(get_pos(*varAccess), "not a struct", "QC-S153");
                return nullptr;
            }
            size_t fieldIdx;
            std::string resolvedFieldType;
            if (structTy->isLiteral()) {
                if (!std::all_of(fieldName.begin(), fieldName.end(), [](unsigned char c) -> bool { return std::isdigit(c); })) {
                    cg_error(get_pos(*propAccess), "tuple indicies must be a integer", "QC-TPL0");
                    return nullptr;
                }
                size_t fieldIdx = std::stoull(fieldName);
                if (structTy->getNumElements() <= fieldIdx) {
                    cg_error(get_pos(*propAccess), "tuple indice too big for tuple", "QC-TPL1");
                    cg_note(get_pos(*propAccess), "tuple has " + std::to_string(structTy->getNumElements()) + " fields, got " + fieldName);
                    return nullptr;
                }
                resolvedFieldType = getTupleFieldType(resolveVarType(varName), fieldIdx);
            } else {
                std::string structName = structTy->getName().str();
                fieldIdx = getFlattenedFieldIndex(structName, fieldName);
                std::function<bool(const std::string&)> findFieldType = [&](const std::string& cname) -> bool {
                    auto& ci = userTypes.at(baseTypeName(baseTypeName(cname)));
                    if (!ci.baseClassName.empty() && findFieldType(ci.baseClassName)) return true;
                    for (auto& field : ci.fields) {
                        if (field.name == fieldName) {
                            resolvedFieldType = field.type;
                            return true;
                        }
                    }
                    return false;
                };
                findFieldType(baseTypeName(structName));
            }
            llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, locAlloc, fieldIdx);
            llvm::Type* fieldTy = structTy->getElementType(fieldIdx);
            llvm::Value* rhsVal = emitExpr((*asn)->value);
            TokenType op = (*asn)->op_tok.type;
            if (op != TokenType::EQ) {
                llvm::Value* oldVal = builder->CreateLoad(fieldTy, fieldPtr);
                bool isFloat = fieldTy->isFloatingPointTy();
                if (op == TokenType::PLUS_EQ && (resolvedFieldType == "char*" || resolvedFieldType == "string") &&
                    std::unordered_set<std::string>({"string", "char*"}).contains(getExpressionType((*asn)->value))) {
                    llvm::Value* concatedString = callStringConcat(oldVal, rhsVal);
                    builder->CreateStore(concatedString, fieldPtr);
                    return concatedString;
                }
                if (oldVal->getType()->isPointerTy() && (op == TokenType::MINUS_EQ || op == TokenType::PLUS_EQ)) {
                    if (!rhsVal->getType()->isIntegerTy()) {
                        cg_error(get_pos((*asn)->value), "pointer offset must be an integer", "QC-S154");
                        return nullptr;
                    }
                    llvm::Value* offset = rhsVal;
                    if (op == TokenType::MINUS_EQ) { offset = builder->CreateNeg(offset, "neg_offset"); }
                    std::string baseType = resolvedFieldType;
                    baseType.pop_back();
                    llvm::Type* elementTy = resolvedFieldType == "string" ? builder->getInt8Ty() : llvmTypeFor(baseType);
                    llvm::Value* newPtr = builder->CreateGEP(elementTy, oldVal, offset, "ptr_add");
                    builder->CreateStore(newPtr, fieldPtr);
                    return newPtr;
                }
                switch (op) {
                case TokenType::PLUS_EQ: rhsVal = isFloat ? builder->CreateFAdd(oldVal, rhsVal) : builder->CreateAdd(oldVal, rhsVal); break;
                case TokenType::MINUS_EQ: rhsVal = isFloat ? builder->CreateFSub(oldVal, rhsVal) : builder->CreateSub(oldVal, rhsVal); break;
                case TokenType::MUL_EQ: rhsVal = isFloat ? builder->CreateFMul(oldVal, rhsVal) : builder->CreateMul(oldVal, rhsVal); break;
                case TokenType::DIV_EQ: rhsVal = isFloat ? builder->CreateFDiv(oldVal, rhsVal) : builder->CreateSDiv(oldVal, rhsVal); break;
                case TokenType::MOD_EQ: rhsVal = isFloat ? builder->CreateFRem(oldVal, rhsVal) : builder->CreateSRem(oldVal, rhsVal); break;
                case TokenType::RSH_EQ: rhsVal = builder->CreateAShr(oldVal, rhsVal); break;
                case TokenType::LSH_EQ: rhsVal = builder->CreateShl(oldVal, rhsVal); break;
                case TokenType::LRSH_EQ: rhsVal = builder->CreateLShr(oldVal, rhsVal); break;
                case TokenType::BIT_A_EQ: rhsVal = builder->CreateAnd(oldVal, rhsVal); break;
                case TokenType::BIT_O_EQ: rhsVal = builder->CreateOr(oldVal, rhsVal); break;
                case TokenType::BIT_X_EQ: rhsVal = builder->CreateXor(oldVal, rhsVal); break;
                case TokenType::LROT_EQ:
                    rhsVal = builder->CreateIntrinsic(llvm::Intrinsic::fshl, {oldVal->getType()}, {oldVal, oldVal, rhsVal});
                    break;
                case TokenType::RROT_EQ:
                    rhsVal = builder->CreateIntrinsic(llvm::Intrinsic::fshr, {oldVal->getType()}, {oldVal, oldVal, rhsVal});
                    break;

                default: break;
                }
            }
            builder->CreateStore(rhsVal, fieldPtr);
            return rhsVal;
        }
    }
    llvm::Value* alloc = emitLValue((*asn)->target);
    if (!alloc) {
        cg_error((*asn)->op_tok.pos,
                 "left side of assignment must be an L-value "
                 "(variable, property, or dereference)",
                 "QC-S155");
        return nullptr;
    }
    std::string name = "";
    if (auto acc = std::get_if<VarAccessNode*>(&((*asn)->target))) { name = (*acc)->var_name_tok.value; }
    std::string lhsTypeStr = getExpressionType((*asn)->target);
    llvm::Type* destTy = llvmTypeFor(lhsTypeStr);
    if (!destTy) {
        cg_error(get_pos((*asn)->target), "could not resolve type " + lhsTypeStr + " for assignment", "QC-T035");
        return nullptr;
    }
    for (auto& [unionName, unionTy] : unionTypes) {
        if (destTy == unionTy) {
            llvm::Value* rhs = emitExpr((*asn)->value);
            if (!rhs) return nullptr;
            if (rhs->getType() == unionTy) {
                builder->CreateStore(rhs, alloc, resolveVolatileVar(name));
                return rhs;
            }
            int tag = findUnionVariantTag(unionName, (*asn)->value, rhs);

            if (tag == -1) { continue; }
            auto member = genericiseOrFindUnion(unionName).members[tag];
            bool isLiteral = member.type.find(':') != std::string::npos;

            std::string baseType = isLiteral ? member.type.substr(0, member.type.find(':')) : member.type;

            llvm::Type* rhsTy = rhs->getType();
            llvm::Type* memberTy = llvmTypeFor(baseType);

            if (rhsTy->getTypeID() != memberTy->getTypeID()) {
                cg_error((*asn)->op_tok.pos, "union variant payload type mismatch", "QC-T036");
                return nullptr;
            }
            llvm::Value* unionVal = llvm::ConstantAggregateZero::get(unionTy);
            unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);
            llvm::Value* dataPtr = storeAndGetPointer(rhs);
            unionVal = builder->CreateInsertValue(unionVal, dataPtr, 1);

            builder->CreateStore(unionVal, alloc, resolveVolatileVar(name));
            return unionVal;
        }
    }
    llvm::Value* oldVal = nullptr;
    if ((*asn)->op_tok.type != TokenType::EQ) { oldVal = builder->CreateLoad(destTy, alloc, resolveVolatileVar(name), "assign_lhs_val"); }
    llvm::Value* rhsVal = nullptr;
    if (destTy->isPointerTy() && classTypes.count(getExpressionType((*asn)->value))) {
        rhsVal = emitLValue((*asn)->value);
    } else {
        rhsVal = emitExpr((*asn)->value);
    }
    std::string rhsType = getExpressionType((*asn)->value);
    if (!rhsVal) {
        cg_error(get_pos((*asn)->value), "failed to compile right-hand side of assignment", "QC-S156");
        return nullptr;
    }
    llvm::Type* srcTy = rhsVal->getType();
    for (auto& [unionName, unionTy] : unionTypes) {
        if (fixMangling(rhsType) == unionName) {
            llvm::Value* dataPtr = builder->CreateExtractValue(rhsVal, 1);

            if (destTy->isPointerTy()) {
                rhsVal = builder->CreateBitCast(dataPtr, destTy);
            } else {
                llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                rhsVal = builder->CreateLoad(destTy, typedPtr, resolveVolatileVar(name));
            }
            destTy = srcTy;
            break;
        }
    }
    if ((*asn)->op_tok.type != TokenType::EQ) {
        if (srcTy != destTy) {
            if (srcTy->isFloatTy() && destTy->isDoubleTy()) {
                warn("implicit-extend", get_pos(*asn), "implicit extension in assignment", "QC-W012");
                rhsVal = builder->CreateFPExt(rhsVal, destTy, "f2d");
                srcTy = destTy;
            } else if (auto structTy = llvm::dyn_cast<llvm::StructType>(destTy)) {
                if (structTy->hasName()) {
                    std::string destClassName = structTy->getName().str();
                    std::string srcClassName = getExpressionType((*asn)->value);
                    if (classTypes.count(destClassName) && classTypes.count(srcClassName)) {
                        auto& srcInfo = userTypes.at(baseTypeName(srcClassName));
                        if (srcInfo.baseClassName == destClassName) {
                        } else {
                            cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                            return nullptr;
                        }
                    } else {
                        cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                        return nullptr;
                    }
                } else {
                    cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                    return nullptr;
                }
            } else if (srcTy->isDoubleTy() && destTy->isFloatTy()) {
                warn("truncation", get_pos(*asn), "implicit truncation in assignment", "QC-W011");
                rhsVal = builder->CreateFPTrunc(rhsVal, destTy, "d2f");
                srcTy = destTy;
            } else if (srcTy->isIntegerTy() && destTy->isIntegerTy()) {
                unsigned srcBits = srcTy->getIntegerBitWidth();
                unsigned destBits = destTy->getIntegerBitWidth();
                if ((srcBits == 1 || srcBits == 2) && (destBits != srcBits)) {
                    cg_error((*asn)->op_tok.pos, "cannot convert bool/qbool to other integer types", "QC-T038");
                    return nullptr;
                }

                if (srcBits < destBits) {
                    warn("implicit-extend", get_pos(*asn), "implicit extension in assignment", "QC-W012");
                    rhsVal = builder->CreateSExt(rhsVal, destTy, "sext");
                    srcTy = destTy;
                } else if (srcBits > destBits) {
                    warn("truncation", get_pos(*asn), "implicit truncation in assignment", "QC-W011");
                    rhsVal = builder->CreateTrunc(rhsVal, destTy, "trunc");
                    srcTy = destTy;
                }
            } else if (srcTy->isIntegerTy() && destTy->isFloatingPointTy()) {
                warn("implicit-int-float", get_pos(*asn), "implicit cast between integer and decimal type in assignment", "QC-W013");
                rhsVal = builder->CreateSIToFP(rhsVal, destTy, "i2f");
                srcTy = destTy;
            } else if (srcTy->isFloatingPointTy() && destTy->isIntegerTy()) {
                warn("implicit-int-float", get_pos(*asn), "implicit cast between integer and decimal type in assignment", "QC-W013");
                rhsVal = builder->CreateFPToSI(rhsVal, destTy, "f2i");
                srcTy = destTy;
            } else if (srcTy->isPointerTy() && !destTy->isPointerTy()) {
                if (lhsTypeStr.ends_with("&")) {
                    rhsVal = builder->CreateLoad(destTy, rhsVal, "ref_peel");
                    srcTy = rhsVal->getType();
                } else {
                    cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                    return nullptr;
                }
            } else if (srcTy->isPointerTy() && destTy->isPointerTy()) {
                if (lhsTypeStr == "void*" || rhsType.ends_with("*") || lhsTypeStr == "@nullptr" || rhsType == "@nullptr") {
                } else if (lhsTypeStr == rhsType) {
                } else {
                    cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                    return nullptr;
                }
            } else if (srcTy->isIntegerTy() && destTy->isPointerTy()) {
                if ((*asn)->op_tok.type == TokenType::PLUS_EQ || (*asn)->op_tok.type == TokenType::MINUS_EQ) {
                    llvm::Value* offset = rhsVal;
                    if ((*asn)->op_tok.type == TokenType::MINUS_EQ) { offset = builder->CreateNeg(offset, "neg_offset"); }
                    std::string ptrType = getExpressionType((*asn)->target);
                    llvm::Type* elementTy;
                    if (ptrType == "string") {
                        elementTy = builder->getInt8Ty();
                    } else {
                        std::string baseType = ptrType;
                        baseType.pop_back();
                        elementTy = llvmTypeFor(baseType);
                    }
                    llvm::Value* newPtr = builder->CreateGEP(elementTy, oldVal, offset, "ptr_add");
                    builder->CreateStore(newPtr, alloc, resolveVolatileVar(name));
                    return newPtr;
                }
                cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                return nullptr;
            }
        }
    } else {
        if (srcTy != destTy) {
            if (srcTy->isFloatTy() && destTy->isDoubleTy()) {
                rhsVal = builder->CreateFPExt(rhsVal, destTy, "f2d");
            } else if (srcTy->isIntegerTy() && destTy->isIntegerTy()) {
                unsigned srcBits = srcTy->getIntegerBitWidth();
                unsigned destBits = destTy->getIntegerBitWidth();
                if ((srcBits == 1 || srcBits == 2) && (destBits != srcBits)) {
                    cg_error((*asn)->op_tok.pos, "cannot convert bool/qbool to other integer types", "QC-T038");
                    return nullptr;
                }

                if (srcBits < destBits) {
                    warn("implicit-extend", get_pos(*asn), "implicit extension in assignment", "QC-W012");
                    rhsVal = builder->CreateSExt(rhsVal, destTy, "sext");
                    srcTy = destTy;
                } else if (srcBits > destBits) {
                    warn("truncation", get_pos(*asn), "implicit truncation in assignment", "QC-W011");
                    rhsVal = builder->CreateTrunc(rhsVal, destTy, "trunc");
                    srcTy = destTy;
                }
            } else if (srcTy->isIntegerTy() && destTy->isFloatTy()) {
                warn("implicit-int-float", get_pos(*asn), "implicit cast between integer and decimal type in assignment", "QC-W013");
                rhsVal = builder->CreateSIToFP(rhsVal, destTy, "i2f");
            } else if (srcTy->isIntegerTy() && destTy->isDoubleTy()) {
                warn("implicit-int-float", get_pos(*asn), "implicit cast between integer and decimal type in assignment", "QC-W013");
                rhsVal = builder->CreateSIToFP(rhsVal, destTy, "i2d");
            } else if (srcTy->isDoubleTy() && destTy->isFloatTy()) {
                cg_error((*asn)->op_tok.pos, "cannot narrow double to float (loses precision)", "QC-S157");
                return nullptr;
            } else if (srcTy->isFloatingPointTy() && destTy->isIntegerTy()) {
                cg_error((*asn)->op_tok.pos,
                         "cannot convert floating point to integer (loses "
                         "precision)",
                         "QC-S158");
                return nullptr;
            } else if (srcTy->isPointerTy() && !destTy->isPointerTy()) {
                if (lhsTypeStr.ends_with("&")) {
                    rhsVal = builder->CreateLoad(destTy, rhsVal, "ref_peel");
                    srcTy = rhsVal->getType();
                }
            } else if (llvm::StructType* sTy = llvm::dyn_cast<llvm::StructType>(destTy);
                       sTy != nullptr && sTy->hasName() && classTypes.find(sTy->getName().str()) != classTypes.end()) {

            } else if (srcTy->isPointerTy() && destTy->isPointerTy()) {
                if (lhsTypeStr == "void*" || rhsType.ends_with("*") || lhsTypeStr == "@nullptr" || rhsType == "@nullptr") {
                } else if (lhsTypeStr == rhsType) {
                } else {
                    cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                    return nullptr;
                }
            } else {
                cg_error((*asn)->op_tok.pos, "type mismatch in assignment", "QC-T037");
                return nullptr;
            }
        }
    }
    llvm::Value* newVal = nullptr;
    bool isFloatTy = destTy->isFloatingPointTy();
    if ((*asn)->op_tok.type == TokenType::PLUS_EQ && (lhsTypeStr == "char*" || lhsTypeStr == "string") &&
        std::unordered_set<std::string>({"string", "char*"}).contains(rhsType)) {
        llvm::Value* concatedString = callStringConcat(oldVal, rhsVal);
        builder->CreateStore(concatedString, alloc, resolveVolatileVar(name));
        return concatedString;
    }
    if ((*asn)->op_tok.type == TokenType::EQ) {
        if (auto structTy = llvm::dyn_cast<llvm::StructType>(destTy)) {
            if (structTy->hasName()) {
                std::string className = structTy->getName().str();
                if (genericiseOrFindClass(className)) {
                    if (auto* arrLit = std::get_if<ArrayLiteralNode*>(&(*asn)->value)) {
                        std::string lhsType = getExpressionType((*asn)->target, false);
                        if (userTypes.count(baseTypeName(lhsType))) {
                            llvm::Value* len = builder->getInt32((*arrLit)->elements.size());
                            rhsVal = decayArrayToPointer(rhsVal);
                            if (rhsVal == nullptr) { return nullptr; }
                            llvm::Function* opMethod = findMethodOverload(className, "operator[]=", {rhsVal, len});
                            if (opMethod) {
                                llvm::Value* lhsAlloc = emitLValue((*asn)->target);
                                return emitMethodCall(opMethod, lhsAlloc, {rhsVal, len}, "operator[]=");
                            }
                            cg_error((*asn)->op_tok.pos, "class " + className + " has no valid matching operator[]=", "QC-S159");
                            addMethodNotes(className, "operator[]=", {len}, (*asn)->op_tok.pos);
                            return nullptr;
                        }
                    }
                    std::vector<llvm::Value*> args = {rhsVal};
                    llvm::Function* opMethod = findMethodOverload(className, "operator=", args);
                    if (opMethod) {
                        llvm::Type* expectedRhsTy = opMethod->getFunctionType()->getParamType(1);
                        if (expectedRhsTy->isStructTy() && rhsVal->getType()->isPointerTy()) {
                            rhsVal = builder->CreateLoad(expectedRhsTy, rhsVal, "op_rhs_load");
                        }
                        std::vector<llvm::Value*> allArgs = {alloc, rhsVal};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        llvm::Value* callResult = builder->CreateCall(opMethod, allArgs, "op_assign_tmp");
                        return callResult;
                    }
                }
                std::string destClassName = structTy->getName().str();
                std::string srcClassName = getExpressionType((*asn)->value);
                if (classTypes.count(destClassName) && classTypes.count(srcClassName)) {
                    auto& srcInfo = userTypes.at(baseTypeName(srcClassName));
                    if (srcInfo.baseClassName == destClassName) {
                        builder->CreateStore(newVal, alloc);
                        auto vtableIt = vtables.find(srcClassName);
                        if (vtableIt != vtables.end()) {
                            llvm::Value* vptrField = builder->CreateStructGEP(structTy, alloc, 0, "vptr_fix");
                            builder->CreateStore(vtableIt->second, vptrField);
                        }
                        return newVal;
                    }
                }
            }
        }
    } else {
        if (auto structTy = llvm::dyn_cast<llvm::StructType>(destTy)) {
            if (structTy->hasName()) {
                std::string className = structTy->getName().str();
                if (genericiseOrFindClass(className)) {
                    std::vector<llvm::Value*> args = {rhsVal};
                    llvm::Function* opMethod = findMethodOverload(className, getCombinationalOperatorMethodName((*asn)->op_tok.type), args);
                    if (opMethod) {
                        llvm::Type* expectedRhsTy = opMethod->getFunctionType()->getParamType(1);
                        if (expectedRhsTy->isStructTy() && rhsVal->getType()->isPointerTy()) {
                            rhsVal = builder->CreateLoad(expectedRhsTy, rhsVal, "op_rhs_load");
                        }
                        std::vector<llvm::Value*> allArgs = {alloc, rhsVal};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        llvm::Value* callResult = builder->CreateCall(opMethod, allArgs, "op_assign_tmp");
                        return callResult;
                    }
                } else if (auto it = userTypes.find(className);
                           it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                    std::string opMethodName = getOperatorMethodName((*asn)->op_tok.type);
                    if (!opMethodName.empty()) {
                        auto fit = functions.find(className + "_" + opMethodName);
                        if (fit != functions.end()) {
                            llvm::Function* opMethod = fit->second;
                            std::vector<llvm::Value*> allArgs = {alloc, rhsVal};
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                                return invk;
                            }
                            return builder->CreateCall(opMethod, allArgs, "op_assign_tmp");
                        }
                    }
                }
                cg_error(get_pos(*asn), "no valid overload to " + getCombinationalOperatorMethodName((*asn)->op_tok.type) + " found", "QC-O001");
                struct Candidate {
                    int score;
                    ClassMethodInfo* method;
                };
                std::vector<Candidate> candidates;
                for (auto& method : userTypes.at(baseTypeName(baseTypeName(className))).classMethods) {
                    if (method.is_constructor || (method.name_tok.value != getCombinationalOperatorMethodName((*asn)->op_tok.type))) continue;
                    int score = 0;
                    size_t argCount = 1;
                    size_t paramCount = 1;
                    if (srcTy == destTy) {
                        score += 3;
                    } else if ((srcTy->isIntegerTy() || srcTy->isFloatTy() || srcTy->isDoubleTy()) &&
                               (destTy->isIntegerTy() || destTy->isFloatTy() || destTy->isDoubleTy())) {
                        score += 1;
                    } else if (srcTy->isPointerTy() && destTy->isPointerTy()) {
                        score += 1;
                    } else {
                        score -= 3;
                    }
                    candidates.push_back({score, &method});
                }
                if (candidates.empty()) {
                    std::vector<std::pair<int, std::string>> suggestions;
                    for (auto& method : userTypes[baseTypeName(className)].classMethods) {
                        int distance = levenshteinDistance(getCombinationalOperatorMethodName((*asn)->op_tok.type), method.name_tok.value);
                        if (distance <= 2) { suggestions.push_back({distance, method.name_tok.value}); }
                    }
                    std::sort(suggestions.begin(), suggestions.end());
                    if (!suggestions.empty()) {
                        std::string note = "similar methods:";
                        for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                        cg_note(get_pos(*asn), note);
                    }
                    return nullptr;
                }
                std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                if (candidates[0].score > 0) { cg_note(get_pos(*asn), "closest matching overload: " + candidates[0].method->print()); }
                if (candidates.size() <= 5) {
                    std::string note = "available overloads:";
                    for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
                    cg_note(get_pos(*asn), note);
                } else {
                    std::string note = "other overloads:";
                    size_t shown = 0;
                    for (auto& candidate : candidates) {
                        if (shown >= 3) break;
                        note += "\n  - " + candidate.method->print();
                        shown++;
                    }
                    cg_note(get_pos(*asn), note);
                }
            }
        }
    }
    switch ((*asn)->op_tok.type) {
    case TokenType::EQ: newVal = rhsVal; break;
    case TokenType::PLUS_EQ: newVal = isFloatTy ? builder->CreateFAdd(oldVal, rhsVal, "fadd") : builder->CreateAdd(oldVal, rhsVal, "add"); break;
    case TokenType::MINUS_EQ: newVal = isFloatTy ? builder->CreateFSub(oldVal, rhsVal, "fsub") : builder->CreateSub(oldVal, rhsVal, "sub"); break;
    case TokenType::MUL_EQ: newVal = isFloatTy ? builder->CreateFMul(oldVal, rhsVal, "fmul") : builder->CreateMul(oldVal, rhsVal, "mul"); break;
    case TokenType::DIV_EQ: newVal = isFloatTy ? builder->CreateFDiv(oldVal, rhsVal, "fdiv") : builder->CreateSDiv(oldVal, rhsVal, "sdiv"); break;
    case TokenType::MOD_EQ: newVal = isFloatTy ? builder->CreateFRem(oldVal, rhsVal, "frem") : builder->CreateSRem(oldVal, rhsVal, "srem"); break;
    case TokenType::RSH_EQ: newVal = builder->CreateAShr(oldVal, rhsVal, "ashr"); break;
    case TokenType::LSH_EQ: newVal = builder->CreateShl(oldVal, rhsVal, "shl"); break;
    case TokenType::LRSH_EQ: newVal = builder->CreateLShr(oldVal, rhsVal, "lshr"); break;
    case TokenType::BIT_A_EQ: newVal = builder->CreateAnd(oldVal, rhsVal, "and"); break;
    case TokenType::BIT_O_EQ: newVal = builder->CreateOr(oldVal, rhsVal, "or"); break;
    case TokenType::BIT_X_EQ: newVal = builder->CreateXor(oldVal, rhsVal, "xor"); break;
    case TokenType::LROT_EQ: newVal = builder->CreateIntrinsic(llvm::Intrinsic::fshl, {oldVal->getType()}, {oldVal, oldVal, rhsVal}); break;
    case TokenType::RROT_EQ: newVal = builder->CreateIntrinsic(llvm::Intrinsic::fshr, {oldVal->getType()}, {oldVal, oldVal, rhsVal}); break;
    default: cg_error((*asn)->op_tok.pos, "unsupported assignment operator.", "QC-S160"); return nullptr;
    }
    builder->CreateStore(newVal, alloc, resolveVolatileVar(name));
    return newVal;
}
llvm::Value* LLVMCompiler::emitUnaryOp(UnaryOpNode* const* unary) {
    TokenType op = (*unary)->op_tok.type;
    llvm::Value* operand = emitExpr((*unary)->node);
    if (!operand) return nullptr;
    llvm::Type* operandTy = operand->getType();
    for (auto& [unionName, unionTy] : unionTypes) {
        if (fixMangling(getExpressionType((*unary)->node)) == unionName) {
            llvm::Type* targetTy = nullptr;

            if (op == TokenType::MINUS) {
                targetTy = builder->getInt32Ty();
            } else if (op == TokenType::NOT) {
                targetTy = builder->getInt1Ty();
            } else if (op == TokenType::QNOT) {
                targetTy = builder->getIntNTy(2);
            } else if (op == TokenType::MUL) {
                targetTy = builder->getPtrTy();
            }
            if (targetTy) {
                llvm::Value* dataPtr = builder->CreateExtractValue(operand, 1);
                llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                operand = builder->CreateLoad(targetTy, typedPtr);
                operandTy = targetTy;
            }
            break;
        }
    }
    if (auto structTy = llvm::dyn_cast<llvm::StructType>(operandTy)) {
        if (structTy->hasName()) {
            std::string className = structTy->getName().str();

            if (classTypes.find(className) != classTypes.end()) {
                std::string opMethodName = getUnaryOperatorMethodName((*unary)->op_tok.type);

                if (!opMethodName.empty()) {
                    std::vector<llvm::Value*> args = {};
                    llvm::Function* opMethod = findMethodOverload(className, opMethodName, args);

                    if (opMethod) {
                        llvm::AllocaInst* temp = createEntryAlloca("temp_unary_this", operandTy);
                        builder->CreateStore(operand, temp);

                        std::vector<llvm::Value*> allArgs = {temp};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "unary_op_result");
                    }
                }
            } else if (auto it = userTypes.find(className);
                       it != userTypes.end() && it->second.kind != UserTypeKind::Concept && it->second.kind != UserTypeKind::Modifier) {
                std::string opMethodName = getUnaryOperatorMethodName((*unary)->op_tok.type);
                if (!opMethodName.empty()) {
                    auto fit = functions.find(className + "_" + opMethodName);
                    if (fit != functions.end()) {
                        llvm::Function* opMethod = fit->second;
                        llvm::AllocaInst* temp = createEntryAlloca("temp_unary_this", operandTy);
                        builder->CreateStore(operand, temp);
                        std::vector<llvm::Value*> allArgs = {temp};
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            auto invk = builder->CreateInvoke(opMethod, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                            return invk;
                        }
                        return builder->CreateCall(opMethod, allArgs, "op_result");
                    }
                }
            }
        }
    }
    if ((*unary)->op_tok.type == TokenType::NOT) { return builder->CreateNot(toTruthiness(operand, (*unary)->op_tok.pos), "not"); }
    if ((*unary)->op_tok.type == TokenType::BITWISE_NOT) {
        llvm::Type* ty = operand->getType();
        if (ty->isFloatingPointTy() || ty->isPointerTy()) {
            cg_error((*unary)->op_tok.pos, "cannot perform bitwise NOT on non-integer type", "QC-T039");
            return nullptr;
        }
        llvm::Value* allOnes = llvm::ConstantInt::get(ty, -1, true);
        return builder->CreateXor(operand, allOnes, "nottmp");
    }
    if ((*unary)->op_tok.type == TokenType::QNOT) {
        if (operand->getType() == builder->getIntNTy(2)) {
            llvm::Function* fn = module->getFunction("qc_qnot");
            if (!fn) {
                llvm::FunctionType* fnTy = llvm::FunctionType::get(builder->getInt8Ty(), {builder->getInt8Ty()}, false);
                fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "qc_qnot", module);
            }
            llvm::Value* op8 = builder->CreateZExt(operand, builder->getInt8Ty());
            llvm::Value* result8 = builder->CreateCall(fn, {op8});
            return builder->CreateTrunc(result8, builder->getIntNTy(2));
        }
        cg_error((*unary)->op_tok.pos, "!! requires qbool operand", "QC-S161");
        return nullptr;
    }
    if ((*unary)->op_tok.type == TokenType::MINUS) {
        if (operandTy->isIntegerTy()) {
            return builder->CreateNeg(operand, "neg");
        } else if (operandTy->isFloatingPointTy()) {
            return builder->CreateFNeg(operand, "fneg");
        } else {
            cg_error((*unary)->op_tok.pos, "- requires numeric operand", "QC-S162");
            return nullptr;
        }
    }
    if ((*unary)->op_tok.type == TokenType::INCREMENT || (*unary)->op_tok.type == TokenType::DECREMENT) {
        bool isPostfix = (*unary)->is_postfix;
        llvm::Value* lhsVal = operand;
        llvm::Value* lhs = emitLValue((*unary)->node);
        llvm::Type* type = lhsVal->getType();
        std::string ptrTy = getExpressionType((*unary)->node);
        std::string name = std::get_if<VarAccessNode*>(&(*unary)->node) ? (*(std::get_if<VarAccessNode*>(&(*unary)->node)))->var_name_tok.value : "";
        llvm::Value* oldVal = builder->CreateLoad(lhsVal->getType(), lhs, resolveVolatileVar(name), "inc_deref");
        if (lhsVal->getType()->isPointerTy()) {
            if (ptrTy == "string") {
                ptrTy = "char";
            } else {
                ptrTy.pop_back();
            }
            llvm::Value* newVal;
            llvm::Value* one = llvm::ConstantInt::get(builder->getIntNTy(getPtrSize()), 1);
            if ((*unary)->op_tok.type == TokenType::INCREMENT) {
                newVal = builder->CreateGEP(llvmTypeFor(ptrTy), oldVal, one, "ptr_inc");
            } else {
                llvm::Value* negOne = llvm::ConstantInt::get(builder->getIntNTy(getPtrSize()), -1, true);
                newVal = builder->CreateGEP(llvmTypeFor(ptrTy), oldVal, negOne, "ptr_dec");
            }
            builder->CreateStore(newVal, lhs, resolveVolatileVar(name));
            return isPostfix ? oldVal : newVal;
        }
        if (!lhsVal->getType()->isIntegerTy()) {
            cg_error((*unary)->op_tok.pos, "++/-- only valid on int-like", "QC-S163");
            return nullptr;
        }
        llvm::Value* one = llvm::ConstantInt::get(lhsVal->getType(), 1);
        llvm::Value* newVal;
        if ((*unary)->op_tok.type == TokenType::INCREMENT) {
            newVal = builder->CreateAdd(oldVal, one, "inc");
        } else {
            newVal = builder->CreateSub(oldVal, one, "dec");
        }

        builder->CreateStore(newVal, lhs, resolveVolatileVar(name));
        return isPostfix ? oldVal : newVal;
    }
    if ((*unary)->op_tok.type == TokenType::AMPERSAND) { return emitLValue((*unary)->node); }
    if ((*unary)->op_tok.type == TokenType::MUL) {
        std::string name = std::get_if<VarAccessNode*>(&(*unary)->node) ? (*(std::get_if<VarAccessNode*>(&(*unary)->node)))->var_name_tok.value : "";
        llvm::Value* val = operand;
        std::string type = getExpressionType((*unary)->node);
        if (!type.ends_with("*") && !type.ends_with("[]") && type != "string") {
            cg_error((*unary)->op_tok.pos, "you can only dereference pointer types, found: " + type, "QC-T040");
            return nullptr;
        }
        if (type == "void*") {
            cg_error((*unary)->op_tok.pos, "you canot dereference void*", "QC-S164");
            return nullptr;
        }
        if (type.ends_with("]")) type.pop_back();
        std::string baseType = type == "string" ? "char" : type.substr(0, type.size() - 1);
        return builder->CreateLoad(llvmTypeFor(baseType), val, resolveVolatileVar(name), "deref");
    }
    if ((*unary)->op_tok.type == TokenType::SIZEOF) {
        const llvm::DataLayout& dl = module->getDataLayout();
        uint64_t size;
        if (StringNode* val = std::get_if<StringNode>(&(*unary)->node)) {
            if (!resolveTypeName(val->tok.value, false).ends_with("*")) {
                std::string resolved = resolveTypeName(val->tok.value, false);
                if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                    generateStruct(resolved, userTypes.at(resolved));
                } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                    generateClass(resolved, userTypes.at(resolved));
                } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                    generateEnum(resolved, userTypes.at(resolved));
                }
            }
            llvm::Type* ty = llvmTypeFor(val->tok.value);
            if (ty) {
                size = dl.getTypeAllocSize(ty);
            } else {
                size = dl.getTypeAllocSize(operand->getType());
            }
        } else if (TypeValueNode* t = std::get_if<TypeValueNode>(&(*unary)->node)) {
            if (!resolveTypeName(t->tok.value, false).ends_with("*")) {
                std::string resolved = resolveTypeName(t->tok.value, false);
                if (structTypes.count(resolved) && structTypes.at(resolved)->isOpaque()) {
                    generateStruct(resolved, userTypes.at(resolved));
                } else if (classTypes.count(resolved) && classTypes.at(resolved)->isOpaque()) {
                    generateClass(resolved, userTypes.at(resolved));
                } else if (enumTypes.count(resolved) && enumTypes.at(resolved)->isOpaque()) {
                    generateEnum(resolved, userTypes.at(resolved));
                }
            }
            llvm::Type* ty = llvmTypeFor(t->tok.value);
            if (ty) {
                size = dl.getTypeAllocSize(ty);
            } else {
                cg_error(t->getPos(), "unknown type `" + t->tok.value + "`", "QC-T041");
                addTypeNotes(t->tok.value, t->getPos());
                return nullptr;
            }
        } else {
            size = dl.getTypeAllocSize(operand->getType());
        }
        unsigned ptrBitWidth = dl.getPointerSizeInBits();
        llvm::IntegerType* addrType = llvm::IntegerType::get(context, ptrBitWidth);
        return llvm::ConstantInt::get(addrType, size);
    }
    if ((*unary)->op_tok.type == TokenType::THROW) {
        llvm::Value* type = getStringConstant(getExpressionType((*unary)->node));
        llvm::Value* value = emitExpr((*unary)->node);
        auto* valTy = value->getType();
        if (valTy->isFloatingPointTy()) {
            if (valTy->isFloatTy()) {
                value = builder->CreateBitCast(value, builder->getInt32Ty());
                value = builder->CreateZExt(value, builder->getInt64Ty());
            } else if (valTy->isDoubleTy()) {
                value = builder->CreateBitCast(value, builder->getInt64Ty());
            }
            value = builder->CreateIntToPtr(value, builder->getPtrTy());
        } else if (valTy->isIntegerTy()) {
            llvm::Value* int64Val = builder->CreateZExtOrTrunc(value, builder->getInt64Ty());
            value = builder->CreateIntToPtr(int64Val, builder->getPtrTy());
        }
        llvm::Function* createFn = module->getFunction("__qc_create_exception");
        llvm::Value* exception = builder->CreateCall(createFn, {type, value}, "exception");
        llvm::Function* throwFn = module->getFunction("__qc_throw");
        if (insideTry()) {
            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
            builder->CreateInvoke(throwFn, contBB, currentLandingPad(), {exception});
            builder->SetInsertPoint(contBB);
        } else {
            builder->CreateCall(throwFn, {exception});
        }
        builder->CreateUnreachable();
        return nullptr;
    }
    if ((*unary)->op_tok.type == TokenType::TYPEOF) {
        AnyNode& argNode = (*unary)->node;
        llvm::Value* arg = emitExpr(argNode);
        if (!arg) return nullptr;
        llvm::Type* argTy = arg->getType();
        for (auto& [unionName, unionTy] : unionTypes) {
            if (argTy == unionTy) {
                llvm::Value* tag = builder->CreateExtractValue(arg, 0, "typeof_tag");
                auto type = genericiseOrFindUnion(unionName);
                auto& members = type.members;
                llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "typeof_end", currentFunction);
                llvm::AllocaInst* resultAlloc = createEntryAlloca("typeof_result", llvm::PointerType::get(context, 0));
                llvm::SwitchInst* switchInst = builder->CreateSwitch(tag, endBB, members.size());
                for (size_t i = 0; i < members.size(); i++) {
                    llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "typeof_case_" + std::to_string(i), currentFunction);
                    builder->SetInsertPoint(caseBB);
                    std::string baseType = members[i].type;
                    size_t colonPos = baseType.find(':');
                    if (colonPos != std::string::npos) baseType = baseType.substr(0, colonPos);
                    llvm::Value* variantName = builder->CreateGlobalString(baseType);
                    builder->CreateStore(variantName, resultAlloc);
                    builder->CreateBr(endBB);
                    switchInst->addCase(builder->getInt32(i), caseBB);
                }
                builder->SetInsertPoint(endBB);
                return builder->CreateLoad(llvm::PointerType::get(context, 0), resultAlloc, "typeof_result");
            }
        }
        std::string typeName = getExpressionType(argNode);
        if (typeName != "unknown") return builder->CreateGlobalString(typeName);
        if (argTy->isIntegerTy(32))
            typeName = "int";
        else if (argTy->isIntegerTy(4))
            typeName = "nibble";
        else if (argTy->isIntegerTy(64))
            typeName = "addr_t";
        if (argTy->isIntegerTy(16))
            typeName = "short int";
        else if (argTy->isFloatTy())
            typeName = "float";
        else if (argTy->isDoubleTy())
            typeName = "double";
        else if (argTy->isIntegerTy(8))
            typeName = "char";
        else if (argTy->isIntegerTy(1))
            typeName = "bool";
        else if (argTy->isIntegerTy(2))
            typeName = "qbool";
        else if (argTy->isPointerTy())
            typeName = "pointer";
        if (auto structTy = llvm::dyn_cast<llvm::StructType>(argTy)) typeName = structTy->getName().str();
        return builder->CreateGlobalString(typeName);
    }
    return nullptr;
}
llvm::Value* LLVMCompiler::emitMapLit(MapLiteralNode* const* mapLit) {
    if ((*mapLit)->struct_type.empty()) {
        cg_error(get_pos(*mapLit), "struct literals must have a struct type", "QC-T042");
        return nullptr;
    }
    llvm::StructType* structTy = genericiseOrFindStruct((*mapLit)->struct_type);
    if (!structTy) {
        cg_error(get_pos(*mapLit), "unknown struct type '" + (*mapLit)->struct_type + "'", "QC-T043");
        std::vector<std::pair<int, std::string>> matches;
        if ((*mapLit)->struct_type.size() >= 3) {
            for (auto& [vname, strct] : userTypes) {
                if (strct.kind != UserTypeKind::Struct) continue;
                int distance = levenshteinDistance((*mapLit)->struct_type, vname);
                if (distance <= 2) { matches.push_back({distance, vname}); }
            }
        }
        std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (!matches.empty()) {
            std::string note = "did you mean ";
            size_t count = std::min<size_t>(3, matches.size());
            for (size_t i = 0; i < count; i++) {
                if (i != 0) note += ", ";
                note += "`" + buildMangledName(baseTypeName(matches[i].second), genericParamsFromName((*mapLit)->struct_type), true) + "`";
            }
            note += "?";
            cg_note(get_pos(*mapLit), note);
        }
        return nullptr;
    }
    llvm::Value* structVal = llvm::ConstantAggregateZero::get(structTy);
    auto structInfo = userTypes.find(baseTypeName((*mapLit)->struct_type))->second;
    for (auto& [keyNode, valueNode] : (*mapLit)->pairs) {
        std::string fieldName;
        if (auto key = std::get_if<VarAccessNode*>(&keyNode)) {
            fieldName = (*key)->var_name_tok.value;
        } else if (auto key = std::get_if<StringNode>(&keyNode)) {
            fieldName = key->tok.value;
        } else {
            cg_error((*mapLit)->pos, "struct field name must be an identifier", "QC-S141");
            return nullptr;
        }
        int fieldIndex = -1;
        for (size_t i = 0; i < structInfo.fields.size(); i++) {
            if (structInfo.fields[i].name == fieldName) {
                fieldIndex = i;
                break;
            }
        }
        if (fieldIndex == -1) {
            cg_error((*mapLit)->pos, "unknown field '" + fieldName + "' in struct " + (*mapLit)->struct_type, "QC-S142");
            return nullptr;
        }
        llvm::Value* fieldValue = emitExpr(valueNode);
        if (!fieldValue) return nullptr;
        structVal = builder->CreateInsertValue(structVal, fieldValue, fieldIndex);
    }
    return structVal;
}
llvm::Value* LLVMCompiler::emitArrLit(ArrayLiteralNode* const* arrLit) {
    if (!(*arrLit)->type.empty() && std::holds_alternative<std::monostate>((*arrLit)->length)) {
        llvm::StructType* structTy = genericiseOrFindStruct((*arrLit)->type);
        if (!structTy) {
            cg_error(get_pos(*arrLit), "unknown struct type '" + (*arrLit)->type + "'", "QC-T043");
            std::vector<std::pair<int, std::string>> matches;
            if ((*arrLit)->type.size() >= 3) {
                for (auto& [vname, strct] : userTypes) {
                    if (strct.kind != UserTypeKind::Struct) continue;
                    int distance = levenshteinDistance((*arrLit)->type, vname);
                    if (distance <= 2) { matches.push_back({distance, vname}); }
                }
            }
            std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            if (!matches.empty()) {
                std::string note = "did you mean ";
                size_t count = std::min<size_t>(3, matches.size());
                for (size_t i = 0; i < count; i++) {
                    if (i != 0) note += ", ";
                    note += "`" + buildMangledName(baseTypeName(matches[i].second), genericParamsFromName((*arrLit)->type), true) + "`";
                }
                note += "?";
                cg_note(get_pos(*arrLit), note);
            }
            return nullptr;
        }
        llvm::Value* structVal = llvm::ConstantAggregateZero::get(structTy);
        auto structInfo = userTypes.find(baseTypeName((*arrLit)->type))->second;
        for (size_t i = 0; i < (*arrLit)->elements.size(); i++) {
            std::string fieldType = structInfo.fields[i].type;
            auto fieldTypeIt = userTypes.find(fieldType);
            llvm::Value* val;
            if (fieldTypeIt != userTypes.end() && fieldTypeIt->second.kind == UserTypeKind::Struct) {
                if (auto nestedArrLit = std::get_if<ArrayLiteralNode*>(&(*arrLit)->elements[i])) {
                    llvm::StructType* nestedStructTy = genericiseOrFindStruct(fieldType);
                    llvm::Value* nestedStruct = llvm::ConstantAggregateZero::get(nestedStructTy);
                    for (size_t j = 0; j < (*nestedArrLit)->elements.size(); j++) {
                        llvm::Value* fieldVal = emitExpr((*nestedArrLit)->elements[j]);
                        if (!fieldVal) return nullptr;
                        nestedStruct = builder->CreateInsertValue(nestedStruct, fieldVal, j);
                    }
                    val = nestedStruct;
                } else {
                    val = emitExpr((*arrLit)->elements[i]);
                    if (!val) return nullptr;
                }
            } else {
                val = emitExpr((*arrLit)->elements[i]);
                if (!val) return nullptr;
            }
            structVal = builder->CreateInsertValue(structVal, val, i);
        }
        return structVal;
    }
    if ((*arrLit)->elements.empty()) {
        llvm::Type* elemType = llvmTypeFor((*arrLit)->type);
        if (elemType == nullptr) {
            cg_error(get_pos(*arrLit), "empty array literals without an element type are not allowed", "QC-T044");
            cg_note(get_pos(*arrLit), "for a empty literal of integers, you can do `[int, 0]`, or for a array of 10 ints, you can do `[int, 10]`");
            return nullptr;
        }
        llvm::Value* length = emitExpr((*arrLit)->length);
        llvm::ConstantInt* ci = llvm::dyn_cast<llvm::ConstantInt>(length);
        if (ci == nullptr) {
            cg_error(get_pos(*arrLit), "empty array literal length must be a constant compile time int", "QC-S165");
            return nullptr;
        }
        llvm::ArrayType* arrTy = llvm::ArrayType::get(elemType, ci->getZExtValue());
        return llvm::ConstantAggregateZero::get(arrTy);
    }

    bool hasRuntimeSpread = false;
    llvm::Value* totalSize = builder->getInt32(0);

    for (auto& elem : (*arrLit)->elements) {
        if (auto spread = std::get_if<SpreadNode*>(&elem)) {
            llvm::Value* collVal = emitExpr((*spread)->expr);
            llvm::Value* spreadLen = getCollectionLength(collVal, (*spread)->expr);

            if (!llvm::isa<llvm::ConstantInt>(spreadLen)) { hasRuntimeSpread = true; }
            totalSize = builder->CreateAdd(totalSize, spreadLen);
        } else {
            totalSize = builder->CreateAdd(totalSize, builder->getInt32(1));
        }
    }

    if (hasRuntimeSpread) { return createRuntimeSizedArray((*arrLit)->elements, totalSize); }
    std::vector<llvm::Value*> allElements;
    for (auto& elem : (*arrLit)->elements) {
        if (auto spread = std::get_if<SpreadNode*>(&elem)) {
            llvm::Value* collVal = emitExpr((*spread)->expr);
            expandSpreadIntoVector(collVal, (*spread)->expr, allElements);
        } else {
            llvm::Value* v = emitExpr(elem);
            if (v) allElements.push_back(v);
        }
    }

    if (allElements.empty()) return nullptr;

    llvm::Value* firstElem = allElements[0];
    llvm::Type* elemTy = firstElem->getType();
    size_t arraySize = allElements.size();

    std::vector<llvm::Constant*> constElems;
    bool allConst = true;
    for (auto* v : allElements) {
        if (auto* constVal = llvm::dyn_cast<llvm::Constant>(v)) {
            constElems.push_back(constVal);
        } else {
            allConst = false;
            break;
        }
    }

    if (allConst) {
        llvm::ArrayType* arrTy = llvm::ArrayType::get(elemTy, arraySize);
        return llvm::ConstantArray::get(arrTy, constElems);
    }

    llvm::ArrayType* arrTy = llvm::ArrayType::get(elemTy, arraySize);
    llvm::AllocaInst* alloc = createEntryAlloca("arr_lit", arrTy);

    for (size_t i = 0; i < allElements.size(); i++) {
        std::vector<llvm::Value*> indices = {builder->getInt32(0), builder->getInt32(i)};
        llvm::Value* elemPtr = builder->CreateInBoundsGEP(arrTy, alloc, indices, "arr_elem_ptr");
        builder->CreateStore(allElements[i], elemPtr);
    }

    std::vector<llvm::Value*> indices = {builder->getInt32(0), builder->getInt32(0)};
    return builder->CreateInBoundsGEP(arrTy, alloc, indices, "arr_ptr");
}
llvm::Value* LLVMCompiler::emitCall(CallNode* const* callPtr) {
    CallNode& call = *(*callPtr);
    if (auto* varAccess = std::get_if<VarAccessNode*>(&call.node_to_call)) {
        std::string funcName = (*varAccess)->var_name_tok.value;
        if (funcName == "proceed") {
            auto it = functions.find("proceed");
            if (it != functions.end()) {
                llvm::Function* targetProceed = it->second;
                llvm::FunctionType* procTy = targetProceed->getFunctionType();
                std::vector<llvm::Value*> callArgs;
                if (call.arg_nodes.empty()) {
                    for (auto& arg : currentFunction->args()) { callArgs.push_back(&arg); }
                } else {
                    for (auto& argNode : call.arg_nodes) {
                        llvm::Value* argVal = emitExpr(argNode);
                        if (!argVal) return nullptr;
                        callArgs.push_back(argVal);
                    }
                }
                if (callArgs.size() != procTy->getNumParams()) {
                    cg_error((*varAccess)->var_name_tok.pos,
                             "proceed() argument count mismatch: expected " + std::to_string(procTy->getNumParams()) + ", got " +
                                 std::to_string(callArgs.size()),
                             "QC-S166");
                    return nullptr;
                }
                llvm::Type* retTy = procTy->getReturnType();
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    auto* invokeInst = builder->CreateInvoke(procTy, targetProceed, contBB, currentLandingPad(), callArgs,
                                                             retTy->isVoidTy() ? "" : "calltmp");
                    builder->SetInsertPoint(contBB);
                    return retTy->isVoidTy() ? nullptr : invokeInst;
                }
                auto* callInst = builder->CreateCall(procTy, targetProceed, callArgs, retTy->isVoidTy() ? "" : "calltmp");
                return retTy->isVoidTy() ? nullptr : callInst;
            }
        }
        std::string resolvedName = funcName;
        if (llvm::Value* v = resolveVariable(baseTypeName(funcName))) {
            if (std::string className = resolveVarType(baseTypeName(funcName)); !className.empty()) {
                if (classTypes.find(className) != classTypes.end()) {
                    if (funcName.find("<") != std::string::npos)
                        if (llvm::Value* val = tryHandleSpecialized(
                                className, buildMangledName("operator()", genericParamsFromName(funcName)),
                                methodCallFromCall(*callPtr, buildMangledName("operator()", genericParamsFromName(funcName))), v))
                            return val;
                    std::vector<std::string> argTypes;
                    for (auto& arg : call.arg_nodes) { argTypes.push_back(getExpressionType(arg)); }
                    ClassMethodInfo* info = findMethodInfo(className, "operator()", argTypes);
                    if (!info) {
                        cg_error(get_pos(*callPtr), "no matching operator() overload for class " + className, "QC-O002");
                        addMethodNotes(className, "operator()", argTypes, get_pos(*callPtr));
                        return nullptr;
                    }
                    MethodCallNode* n = methodCallFromCall(*callPtr, "operator()");
                    auto args = prepareArgs(info, n->args);
                    delete n;
                    bool isVariadic = !info->params.empty() && info->params.back().type.value == "...";
                    if (isVariadic) {
                        size_t numFixedParams = info->params.size() - 1;
                        std::vector<llvm::Value*> varVals;
                        if (args.size() > numFixedParams) {
                            varVals.assign(args.begin() + numFixedParams, args.end());
                            args.resize(numFixedParams);
                        }
                        args.push_back(packVariadicArgs(varVals));
                    }
                    llvm::Function* opMethod = findMethodOverload(className, "operator()", args);
                    if (!opMethod) {
                        cg_error(get_pos(*callPtr), "no matching operator() overload for class " + className, "QC-O002");
                        addMethodNotes(className, "operator()", args, get_pos(*callPtr));
                        return nullptr;
                    }
                    return emitMethodCall(opMethod, v, args, "operator()");
                }
            }
        }
        llvm::Function* resolved = resolveFunction(funcName);
        if (resolved) {
            resolvedName = resolved->getName().str();
        } else {
            std::string ns = getCurrentNamespace();
            while (!ns.empty()) {
                std::string candidate = ns + "::" + funcName;
                if (functionDefs.count(candidate)) {
                    resolvedName = candidate;
                    break;
                }
                size_t pos = ns.rfind("::");
                ns = (pos == std::string::npos) ? "" : ns.substr(0, pos);
            }
        }
        funcName = resolvedName;
        auto funcDefIt = functionDefs.find(baseTypeName(funcName));
        if (funcDefIt != functionDefs.end()) {
            FuncDefNode* funcDef = funcDefIt->second;
            if (!funcDef->generics.empty()) {
                std::vector<llvm::Value*> argValues;
                auto paramIt = funcDef->params.begin();
                bool hasSpread = false;
                int paramIdx = 0;
                auto argIt = call.arg_nodes.begin();
                while (paramIt != funcDef->params.end()) {
                    llvm::Value* argVal;
                    auto param = *paramIt++;
                    if (paramIdx >= call.arg_nodes.size()) {
                        if (param.default_value.has_value()) {
                            AnyNode& defaultRef = const_cast<AnyNode&>(param.default_value.value());
                            argVal = emitExpr(defaultRef);
                            if (!argVal) {
                                cg_error(get_pos(&call), "failed to evaluate default parameter", "QC-S168");
                                return nullptr;
                            }
                        } else {
                            cg_error(get_pos(&call), "missing required argument at position " + std::to_string(paramIdx), "QC-S169");
                            return nullptr;
                        }
                    } else {
                        auto argNode = *argIt;
                        argIt++;
                        if (std::holds_alternative<SpreadNode*>(argNode)) { hasSpread = true; }
                        std::string ptype = (paramIt != funcDef->params.end()) ? paramIt->type.value : "...";
                        if (ptype.ends_with("&")) {
                            argVal = emitLValue(argNode);
                        } else {
                            argVal = emitExpr(argNode);
                        }
                    }
                    paramIdx++;
                    argValues.push_back(argVal);
                }
                if (hasSpread) {
                    cg_error(get_pos(&call), "spread is no longer allowed in function calls.", "QC-S170");
                    return nullptr;
                }
                funcName = fixMangling(funcName);
                if (specializedFunctions.find(funcName) == specializedFunctions.end()) {
                    llvm::Function* specializedFn = generateSpecializedFunction(funcDef, funcName);
                    if (!specializedFn) return nullptr;
                    specializedFunctions[funcName] = specializedFn;
                }
                llvm::Function* fn = specializedFunctions[funcName];
                if (funcDef->params.size() > 0 && funcDef->params.back().type.value == "...") {
                    size_t fixedCount = funcDef->params.size() - 1;
                    std::vector<llvm::Value*> varVals(argValues.begin() + fixedCount, argValues.end());
                    argValues.resize(fixedCount);
                    argValues.push_back(packVariadicArgs(varVals));
                }
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    llvm::InvokeInst* invoke = builder->CreateInvoke(fn, contBB, currentLandingPad(), argValues);
                    builder->SetInsertPoint(contBB);
                    return invoke;
                } else {
                    return builder->CreateCall(fn, argValues);
                }
            }
        }
        std::string saved_name = funcName;
        resolvedName = resolveTypeName(funcName, false);
        if (!currentGenericTypeStrings.empty()) {
            std::string substituted = substituteGenerics(resolvedName);
            if (substituted != resolvedName) resolvedName = substituted;
        }
        saved_name = resolvedName;
        auto classIt = userTypes.find(baseTypeName(resolvedName));

        if (classIt != userTypes.end() && classIt->second.kind == UserTypeKind::Class) {
            llvm::StructType* classTy = genericiseOrFindClass(resolvedName);
            if (classTy == nullptr) {
                if (saved_name.find('<') != std::string::npos) {
                    size_t lt = saved_name.find('<');
                    std::string inner = saved_name.substr(lt + 1, saved_name.size() - lt - 2);
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
                    std::string fullName = buildMangledName(resolvedName, genericParams);
                    classTy = generateGenericClass(resolvedName, classIt->second, genericParams);
                    resolvedName = fullName;
                    if (classTy == nullptr) {
                        cg_error(get_pos(*varAccess), "failed to generate generic subset for class " + resolvedName, "QC-G013");
                        return nullptr;
                    }
                } else {
                    cg_error(get_pos(*callPtr), "class '" + resolvedName + "' has no generated type", "QC-T045");
                    return nullptr;
                }
            }
            llvm::AllocaInst* temp = createEntryAlloca("temp_" + resolvedName, classTy);
            std::string ctorName = "";
            ClassMethodInfo* ctorInfo = nullptr;
            for (auto& method : classIt->second.classMethods) {
                if (method.is_constructor) {
                    ctorName = method.name_tok.value;
                    ctorInfo = &method;
                    break;
                }
            }
            if (!ctorName.empty()) {
                std::vector<llvm::Value*> ctorArgs;
                for (auto& argNode : call.arg_nodes) {
                    llvm::Value* arg = emitExpr(argNode);
                    if (!arg) return nullptr;
                    ctorArgs.push_back(arg);
                }
                llvm::Function* ctor = findMethodOverload(resolvedName, ctorName, ctorArgs);
                if (!ctor) {
                    cg_error((*varAccess)->var_name_tok.pos, "no matching constructor for " + resolvedName, "QC-S171");
                    addConstructorNotes(resolvedName, ctorArgs, get_pos(*varAccess));
                    return nullptr;
                }
                bool isCtorVariadic = (ctorInfo->params.size() > 0 && ctorInfo->params.back().type.value == "...");
                if (isCtorVariadic) {
                    size_t fixedCount = ctorInfo->params.size();
                    std::vector<llvm::Value*> varVals(ctorArgs.begin() + fixedCount, ctorArgs.end());
                    ctorArgs.resize(fixedCount);
                    ctorArgs.push_back(packVariadicArgs(varVals));
                }

                std::vector<llvm::Value*> allArgs = {temp};
                allArgs.insert(allArgs.end(), ctorArgs.begin(), ctorArgs.end());
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    builder->CreateInvoke(ctor, contBB, currentLandingPad(), allArgs);
                    builder->SetInsertPoint(contBB);
                } else {
                    builder->CreateCall(ctor, allArgs);
                }
                auto vtableIt = vtables.find(resolvedName);
                if (vtableIt != vtables.end()) {
                    llvm::Value* vptrField = builder->CreateStructGEP(classTy, temp, 0, "vptr_field");
                    builder->CreateStore(vtableIt->second, vptrField);
                }
            } else {
                builder->CreateStore(llvm::Constant::getNullValue(classTy), temp);
                auto vtableIt = vtables.find(resolvedName);
                if (vtableIt != vtables.end()) {
                    llvm::Value* vptrField = builder->CreateStructGEP(classTy, temp, 0, "vptr_field");
                    builder->CreateStore(vtableIt->second, vptrField);
                }
            }
            return builder->CreateLoad(classTy, temp, resolvedName + "_inst");
        }
        static const std::unordered_map<std::string, std::string> builtins = {{"`time", "qc_time"},
                                                                              {"`seed", "qc_seed"},
                                                                              {"`random", "qc_random_int"},
                                                                              {"`len", "qc_len"},
                                                                              {"`to_lower", "qc_to_lower"},
                                                                              {"`to_upper", "qc_to_upper"},
                                                                              {"`substring", "qc_substring"},
                                                                              {"`contains", "qc_contains"},
                                                                              {"`startswith", "qc_startswith"},
                                                                              {"`endswith", "qc_endswith"},
                                                                              {"`trim", "qc_trim"},
                                                                              {"`replace", "qc_replace"},
                                                                              {"`to_int", "qc_to_int_from_string"},
                                                                              {"`to_float", "qc_to_float_from_string"},
                                                                              {"`to_double", "qc_to_double_from_string"},
                                                                              {"`to_char", "qc_to_char_from_string"},
                                                                              {"`to_bool", "qc_to_bool_from_string"},
                                                                              {"`to_string", "qc_to_string_int"},
                                                                              {"`to_byte", "qc_to_byte_from_string"},
                                                                              {"`to_nibble", "qc_to_nibble_from_string"},
                                                                              {"`to_addr_t", "qc_to_addr_t_from_string"},
                                                                              {"`to_qbool", "qc_to_qbool_from_string"},
                                                                              {"`to_long_int", "qc_to_long_int_from_string"},
                                                                              {"`to_short_int", "qc_to_short_int_from_string"},
                                                                              {"`qout", ""},
                                                                              {"`open", "qc_open"},
                                                                              {"`close", "qc_close"},
                                                                              {"`read", "qc_read"},
                                                                              {"`typeof", ""},
                                                                              {"`cast", ""},
                                                                              {"`write", ""},
                                                                              {"`malloc", "qc_malloc"},
                                                                              {"`calloc", "qc_calloc"},
                                                                              {"`free", "qc_free"},
                                                                              {"`realloc", "qc_realloc"},
                                                                              {"`mapped_ptr", ""},
                                                                              {"`ternary", ""},
                                                                              {"`to_address", ""},
                                                                              {"`inline", ""},
                                                                              {"`flush", "qc_flush"},
                                                                              {"`next", ""},
                                                                              {"`is_empty", ""},
                                                                              {"`float_bits", ""},
                                                                              {"`double_bits", ""},
                                                                              {"`compile_error", ""},
                                                                              {"`compile_warn", ""},
                                                                              {"`compile_note", ""},
                                                                              {"`atomic_load", ""},
                                                                              {"`atomic_store", ""},
                                                                              {"`atomic_exchange", ""},
                                                                              {"`atomic_add", ""},
                                                                              {"`atomic_sub", ""},
                                                                              {"`atomic_and", ""},
                                                                              {"`atomic_or", ""},
                                                                              {"`atomic_xor", ""},
                                                                              {"`atomic_nand", ""},
                                                                              {"`atomic_min", ""},
                                                                              {"`atomic_max", ""},
                                                                              {"`atomic_umin", ""},
                                                                              {"`atomic_umax", ""},
                                                                              {"`atomic_cmpxchg", ""},
                                                                              {"`atomic_fence", ""},
                                                                              {"`lseek", "qc_lseek"},
                                                                              {"`opendir", "qc_opendir"},
                                                                              {"`readdir", "qc_readdir"},
                                                                              {"`closedir", "qc_closedir"},
                                                                              {"`memset", ""},
                                                                              {"`memmove", ""},
                                                                              {"`memcpy", ""}};
        auto it = builtins.find(funcName);

        if (it != builtins.end()) {
            std::string runtimeName = it->second;
            if (funcName == "`memset") {
                if (call.arg_nodes.size() != 3) {
                    cg_error(get_pos(&call), "`memset expectes exactly 3 arguments", "QC-S172");
                    return nullptr;
                }
                llvm::Value* dest_ptr = emitExpr(call.arg_nodes.front());
                llvm::Value* size = emitExpr(call.arg_nodes.back());
                llvm::Value* value = emitExpr(*std::next(call.arg_nodes.begin(), 1));
                builder->CreateMemSet(dest_ptr, value, size, llvm::MaybeAlign(), false);
                return nullptr;
            }
            if (funcName == "`memmove") {
                if (call.arg_nodes.size() != 3) {
                    cg_error(get_pos(&call), "`memmove expectes exactly 3 arguments", "QC-S172");
                    return nullptr;
                }
                llvm::Value* dest_ptr = emitExpr(call.arg_nodes.front());
                llvm::Value* size = emitExpr(call.arg_nodes.back());
                llvm::Value* src_ptr = emitExpr(*std::next(call.arg_nodes.begin(), 1));
                builder->CreateMemMove(dest_ptr, llvm::MaybeAlign(), src_ptr, llvm::MaybeAlign(), size, false);
                return nullptr;
            }
            if (funcName == "`memcpy") {
                if (call.arg_nodes.size() != 3) {
                    cg_error(get_pos(&call), "`memcpy expectes exactly 3 arguments", "QC-S172");
                    return nullptr;
                }
                llvm::Value* dest_ptr = emitExpr(call.arg_nodes.front());
                llvm::Value* size = emitExpr(call.arg_nodes.back());
                llvm::Value* src_ptr = emitExpr(*std::next(call.arg_nodes.begin(), 1));
                builder->CreateMemCpy(dest_ptr, llvm::MaybeAlign(), src_ptr, llvm::MaybeAlign(), size, false);
                return nullptr;
            }
            if (funcName == "`atomic_load") {
                if (call.arg_nodes.size() != 1) {
                    cg_error(get_pos(*callPtr), "`atomic_load expects exactly one argument", "QC-S172");
                    return nullptr;
                }
                AnyNode& atomicNode = call.arg_nodes.front();
                llvm::Value* addr = emitLValue(atomicNode);
                if (!addr) return nullptr;
                llvm::Type* valueType = llvmTypeFor(getExpressionType(atomicNode, false));
                auto* load = builder->CreateLoad(valueType, addr);
                load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                return load;
            } else if (funcName == "`atomic_store") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_store expects (atomic_variable, value)", "QC-S173");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                AnyNode& atomicNode = *it++;
                AnyNode& valueNode = *it;
                llvm::Value* addr = emitLValue(atomicNode);
                llvm::Value* value = emitExpr(valueNode);
                if (!addr || !value) return nullptr;
                auto* store = builder->CreateStore(value, addr);
                store->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                return nullptr;
            } else if (funcName == "`atomic_exchange") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_exchange expects (atomic_variable, value)", "QC-S174");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                AnyNode& atomicNode = *it++;
                const AnyNode& valueNode = *it;
                llvm::Value* addr = emitLValue(atomicNode);
                llvm::Value* value = emitExpr(valueNode);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_add") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_add expects (atomic_variable, value)", "QC-S175");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Add, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_sub") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_sub expects (atomic_variable, value)", "QC-S176");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Sub, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_and") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_and expects (atomic_variable, value)", "QC-S177");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::And, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_or") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_or expects (atomic_variable, value)", "QC-S178");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Or, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_xor") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_xor expects (atomic_variable, value)", "QC-S179");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xor, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_nand") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_nand expects (atomic_variable, value)", "QC-S180");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Nand, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_min") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_min expects (atomic_variable, value)", "QC-S181");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Min, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_max") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_max expects (atomic_variable, value)", "QC-S182");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::Max, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_umin") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_umin expects (atomic_variable, value)", "QC-S183");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::UMin, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_umax") {
                if (call.arg_nodes.size() != 2) {
                    cg_error(get_pos(*callPtr), "`atomic_umax expects (atomic_variable, value)", "QC-S184");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* value = emitExpr(*it);
                if (!addr || !value) return nullptr;
                return builder->CreateAtomicRMW(llvm::AtomicRMWInst::UMax, addr, value, llvm::MaybeAlign(),
                                                llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_cmpxchg") {
                if (call.arg_nodes.size() != 3) {
                    cg_error(get_pos(*callPtr),
                             "`atomic_cmpxchg expects "
                             "(atomic_variable, expected, desired)",
                             "QC-S185");
                    return nullptr;
                }
                auto it = call.arg_nodes.begin();
                llvm::Value* addr = emitLValue(*it++);
                llvm::Value* expected = emitExpr(*it++);
                llvm::Value* desired = emitExpr(*it);
                if (!addr || !expected || !desired) return nullptr;
                return builder->CreateAtomicCmpXchg(addr, expected, desired, llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent,
                                                    llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`atomic_fence") {
                if (!call.arg_nodes.empty()) {
                    cg_error(get_pos(*callPtr), "`atomic_fence expects no arguments", "QC-S186");
                    return nullptr;
                }
                return builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
            } else if (funcName == "`compile_error" && !call.arg_nodes.empty()) {
                AnyNode node = call.arg_nodes.back();
                StringNode* n = std::get_if<StringNode>(&node);
                if (!n) {
                    cg_error(get_pos(node), "`compile_error takes a comptime string", "QC-S187");
                    return nullptr;
                }
                cg_error(get_pos(node), n->tok.value, "QC-S188");
                return nullptr;
            } else if (funcName == "`compile_warn" && !call.arg_nodes.empty()) {
                AnyNode node = call.arg_nodes.back();
                StringNode* n = std::get_if<StringNode>(&node);
                if (!n) {
                    cg_error(get_pos(node), "`compile_warn takes a comptime string", "QC-S189");
                    return nullptr;
                }
                cg_warn(get_pos(node), n->tok.value, "QC-S188");
                return nullptr;
            } else if (funcName == "`compile_note" && !call.arg_nodes.empty()) {
                if (errors.empty()) cg_warn(get_pos(&call), "");
                AnyNode node = call.arg_nodes.back();
                StringNode* n = std::get_if<StringNode>(&node);
                if (!n) {
                    cg_error(get_pos(node), "`compile_note takes a comptime string", "QC-S190");
                    return nullptr;
                }
                cg_note(get_pos(node), n->tok.value);
                return nullptr;
            } else if (funcName == "`float_bits" && !call.arg_nodes.empty()) {
                llvm::Value* value = emitExpr(call.arg_nodes.front());
                if (!value) return nullptr;
                value = normalizeValue(value, call.arg_nodes.front());
                if (!value->getType()->isIntegerTy()) return nullptr;
                return builder->CreateBitCast(value, llvm::Type::getFloatTy(context));
            } else if (funcName == "`double_bits" && !call.arg_nodes.empty()) {
                llvm::Value* value = emitExpr(call.arg_nodes.front());
                if (!value) return nullptr;
                value = normalizeValue(value, call.arg_nodes.front());
                if (!value->getType()->isIntegerTy()) return nullptr;
                return builder->CreateBitCast(value, llvm::Type::getDoubleTy(context));
            } else if (funcName == "`typeof" && !call.arg_nodes.empty()) {
                AnyNode& argNode = call.arg_nodes.back();
                llvm::Value* arg = emitExpr(argNode);
                if (!arg) return nullptr;
                llvm::Type* argTy = arg->getType();
                for (auto& [unionName, unionTy] : unionTypes) {
                    if (argTy == unionTy) {
                        llvm::Value* tag = builder->CreateExtractValue(arg, 0, "typeof_tag");
                        auto type = genericiseOrFindUnion(unionName);
                        auto& members = type.members;
                        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "typeof_end", currentFunction);
                        llvm::AllocaInst* resultAlloc = createEntryAlloca("typeof_result", llvm::PointerType::get(context, 0));
                        llvm::SwitchInst* switchInst = builder->CreateSwitch(tag, endBB, members.size());
                        for (size_t i = 0; i < members.size(); i++) {
                            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "typeof_case_" + std::to_string(i), currentFunction);
                            builder->SetInsertPoint(caseBB);
                            std::string baseType = members[i].type;
                            size_t colonPos = baseType.find(':');
                            if (colonPos != std::string::npos) baseType = baseType.substr(0, colonPos);
                            llvm::Value* variantName = builder->CreateGlobalString(baseType);
                            builder->CreateStore(variantName, resultAlloc);
                            builder->CreateBr(endBB);
                            switchInst->addCase(builder->getInt32(i), caseBB);
                        }
                        builder->SetInsertPoint(endBB);
                        return builder->CreateLoad(llvm::PointerType::get(context, 0), resultAlloc, "typeof_result");
                    }
                }
                std::string typeName = getExpressionType(argNode);
                if (typeName != "unknown") return builder->CreateGlobalString(typeName);
                if (argTy->isIntegerTy(32))
                    typeName = "int";
                else if (argTy->isIntegerTy(4))
                    typeName = "nibble";
                else if (argTy->isIntegerTy(64))
                    typeName = "addr_t";
                if (argTy->isIntegerTy(16))
                    typeName = "short int";
                else if (argTy->isFloatTy())
                    typeName = "float";
                else if (argTy->isDoubleTy())
                    typeName = "double";
                else if (argTy->isIntegerTy(8))
                    typeName = "char";
                else if (argTy->isIntegerTy(1))
                    typeName = "bool";
                else if (argTy->isIntegerTy(2))
                    typeName = "qbool";
                else if (argTy->isPointerTy())
                    typeName = "pointer";
                if (auto structTy = llvm::dyn_cast<llvm::StructType>(argTy)) typeName = structTy->getName().str();
                return builder->CreateGlobalString(typeName);
            } else if (funcName == "`cast" && call.arg_nodes.size() >= 2) {
                llvm::Value* value = emitExpr(call.arg_nodes.front());
                if (!value) return nullptr;
                value = normalizeValue(value, call.arg_nodes.front());
                auto* typeNode = std::get_if<TypeValueNode>(&call.arg_nodes.back());
                if (!typeNode) return nullptr;
                llvm::Type* dstTy = llvmTypeFor(typeNode->tok.value);
                if (!dstTy) return nullptr;
                llvm::Type* srcTy = value->getType();
                if (srcTy == dstTy) return value;
                bool srcSigned = std::unordered_set<std::string>({"addr_t", "byte", "nibble"}).contains(getExpressionType(call.arg_nodes.front()));
                bool dstSigned = std::unordered_set<std::string>({"addr_t", "byte", "nibble"}).contains(typeNode->tok.value);
                if (srcTy->isIntegerTy() && dstTy->isIntegerTy()) {
                    unsigned srcBits = srcTy->getIntegerBitWidth();
                    unsigned dstBits = dstTy->getIntegerBitWidth();
                    if (dstBits > srcBits) return srcSigned ? builder->CreateSExt(value, dstTy) : builder->CreateZExt(value, dstTy);
                    if (dstBits < srcBits) return builder->CreateTrunc(value, dstTy);
                    return value;
                }
                if (srcTy->isIntegerTy() && dstTy->isFloatingPointTy())
                    return srcSigned ? builder->CreateSIToFP(value, dstTy) : builder->CreateUIToFP(value, dstTy);
                if (srcTy->isFloatingPointTy() && dstTy->isIntegerTy())
                    return dstSigned ? builder->CreateFPToSI(value, dstTy) : builder->CreateFPToUI(value, dstTy);
                if (srcTy->isFloatingPointTy() && dstTy->isFloatingPointTy())
                    return dstTy->getPrimitiveSizeInBits() > srcTy->getPrimitiveSizeInBits() ? builder->CreateFPExt(value, dstTy)
                                                                                             : builder->CreateFPTrunc(value, dstTy);
                if (srcTy->isPointerTy() && dstTy->isPointerTy()) return builder->CreateBitCast(value, dstTy);
                if (srcTy->isPointerTy() && dstTy->isIntegerTy()) return builder->CreatePtrToInt(value, dstTy);
                if (srcTy->isIntegerTy() && dstTy->isPointerTy()) return builder->CreateIntToPtr(value, dstTy);
                return nullptr;
            } else if (funcName == "`write" && !call.arg_nodes.empty()) {
                if (call.arg_nodes.size() == 3)
                    runtimeName = "qc_write_sized";
                else
                    runtimeName = "qc_write";
            } else if (funcName == "`random" && !call.arg_nodes.empty()) {
                if (call.arg_nodes.size() == 1)
                    runtimeName = "qc_random_int";
                else if (call.arg_nodes.size() == 2)
                    runtimeName = "qc_random_range";
            } else if (funcName == "`qout") { // Ṱ̵̺̙̙͔̯̣͓̼̈́͜h̶̳͖̝̰͍̮͆̅̊e̶̡̧̮͍̘̘͍̮͎͎̺̗̦͕̾͗͐̽͑̔̅́̑̌̕ ̶̥̮̪͙̎͛̐͑̔̉́̂̂̐́̽̔̔͂̃d̴̛̪̦̞́́̎͊̌̈̍̓̓̔̑͑̒͘͝e̶͎̤̠̞̞͖̊ṽ̴̡͖̫̩̣̳̖̞̯̪͇̰̆͑͐͐̀̿͐̍̑̕͘̕͝͝ͅͅͅí̵̜̬͍̖̒͑̎͗l̸̛͍̰̜̞̩̜̘͈̯̬̇̀̋̈͐̔̿̓̅͌̉̅͂̌͘͜͝ ̷̡̣̰͙̰̪͈̪̣̺̺̤̦̰͌̊̀̀̑͑̅̈́ş̶̛̳̟̫͇̠͉͍̺̣̲̬̻̰͍̙̋̂͗̕͠ͅę̸̹̹̈́͒̐̃̋̓͐̓͆̉̀̊̀̏̿͘é̷͖͎̹̉́̈́͠͠͝s̸̡̢̢̩͍̹̼͈͕̘̖͋̋̃̓͗͆͌̕͠ͅͅͅ ̴̛̮͉̣̈́̒͋͐̿̾̐̽̚ḩ̶̨̧̺͉̹̩̙̫͇̰̫̯̬͐́̑͜i̶̠͖̠̟̻̭̫̙̳̪͆̄̿̈́̾̊̈́̒͑͊̆̋̃̎̿̂͗ş̴̥̤̜̦̗͍̟̈́̽̑̏ ̶̡̛̫̥̝̰̣̟͇͔̤̱̯͉̱̩̋̈̈́͐̓̑̋̎͝͝ͅö̷̡̝̣́̎̎͝ẘ̶̢̡̨̡̭̞̯̘̦̟̳̮̫͎̑͂̇̀͆̋̐̃̒́̏̓͒̅͜͝͝n̵̳͎̣̬̪̝̩͒͊̓̾̓̄̃̂͗̉͆̒̋̚͜͜͝ ̴͔̫̂̏ͅͅį̷̡̤̼͈̗̦̣̘̮̠̣͎̬̰̍͗ṉ̸̨̯̱̦͕͐̉̀͌͑̀͐̽̕͜
                                              // ̷̛̜̈́̐̇̑͛̕ṯ̸̟̰̩̩̼̀͆̏̀̔̈́͛̍͑͑͠͝h̶̺̺͙͙̤̘̦̬̝̱̟͕̟̟͕̯͛̌͋̓́̔̊͘͘ͅè̷̢̡̝̗͙̘͍̠̝͑̃̋͜͝͝ ̶̬̐̂̏̆̀͝͠s̴̨̮̺͙͙̪̹͖͓̆̌̔͆̿̌̏̇̎͜͝h̴̛̝̜̥̺͇̗̪̄̀͆̆̅͋͂̅͘ͅḁ̸̖͐̅̑͗̃̂͌̃͝d̶̢͇͉͈̹̯͌̓͂̈̒́͐̈́͑̏̀͊͋͐͠o̴̧̧̥͎͓̒̀̍̀͒͠w̵̢̰̰̭̟̼̋̓͋̈́̅ ̸̢̖̘͓̯̦͎̼̗̠̤̙̿̄̍̎̎͑͐ȏ̷̹̫̲͎͖͉̩̺̫̖͊̐̄̀͌̃̀́̌͑͒̈́̐̀͘f̴̧̣͔͇̹͙͙̦͎̿̋͊͊̀̽͗͒ ̷͕̥͕̣͎̫̿͊͊̅͆͂͘͜ǫ̴̢̱͍͍͍̰͓͚̟͚̹͗̔̎͜͠͠ţ̷̨̺̯̥͕̳̮̳̜̙̫̫̺͐̀͊̽̀̇̽̋̚̚͠ͅh̷̼̦̦̝̺̒͌͐͐̀̈́̕̕͠ͅḙ̷̢̨̜͕͖͈̜͖̥̈́̐́̀̓́̽̀̈͂̅́̍̚͜͝r̷͙̎͐̅̍̐̈́͌͊͌̇́ŝ̵̥̱̞͔̩̉͋̌͂̉͑̇̆̓͆̃̚͝.̸̡̣̘̗̖̦͙͕̯̗̩́̔͜͠
                if (call.arg_nodes.empty()) {
                    cg_error((*varAccess)->var_name_tok.pos, "qout requires arguments: " + funcName, "QC-S191");
                    return nullptr;
                }
                std::vector<AnyNode> goodArgs((call.arg_nodes.begin()), (call.arg_nodes.end()));
                int current_arg = 0;
                std::string fmtString = "";
                llvm::Value* argVal = emitExpr(goodArgs[0]);
                llvm::ConstantDataSequential* constArray = nullptr;
                if (auto* CE = llvm::dyn_cast<llvm::ConstantExpr>(argVal)) {
                    if (CE->getOpcode() == llvm::Instruction::GetElementPtr) { argVal = CE->getOperand(0); }
                }
                if (auto* GV = llvm::dyn_cast<llvm::GlobalVariable>(argVal)) {
                    if (GV->hasInitializer()) { constArray = llvm::dyn_cast<llvm::ConstantDataSequential>(GV->getInitializer()); }
                } else {
                    constArray = llvm::dyn_cast<llvm::ConstantDataSequential>(argVal);
                }
                if (constArray && constArray->isString()) {
                    fmtString = constArray->getAsString().str();
                } else {
                    cg_error((*varAccess)->var_name_tok.pos,
                             "qout requires the first argument to be a "
                             "string: " +
                                 funcName,
                             "QC-S192");
                    return nullptr;
                }
                std::string to_print = "";
                char c;
                llvm::Function* printString = module->getFunction("qc_print_string");
                if (!printString) {
                    llvm::FunctionType* prStrFnTy = llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::get(context, 0)}, false);
                    printString = llvm::Function::Create(prStrFnTy, llvm::Function::ExternalLinkage, "qc_print_string", module);
                }
                llvm::Function* fmtStr = module->getFunction("qc_fmt_string");
                if (!fmtStr) {
                    llvm::FunctionType* prStrFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {llvm::PointerType::get(context, 0), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtStr = llvm::Function::Create(prStrFnTy, llvm::Function::ExternalLinkage, "qc_fmt_string", module);
                }
                llvm::Function* fmtInt = module->getFunction("qc_fmt_int");
                if (!fmtInt) {
                    llvm::FunctionType* fmtIntFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {builder->getIntNTy(getPtrSize()), builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtInt = llvm::Function::Create(fmtIntFnTy, llvm::Function::ExternalLinkage, "qc_fmt_int", module);
                }
                llvm::Function* fmtUInt = module->getFunction("qc_fmt_unsigned_int");
                if (!fmtUInt) {
                    llvm::FunctionType* fmtUIntFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {builder->getIntNTy(getPtrSize()), builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtUInt = llvm::Function::Create(fmtUIntFnTy, llvm::Function::ExternalLinkage, "qc_fmt_unsigned_int", module);
                }
                llvm::Function* fmtFloat = module->getFunction("qc_fmt_float");
                if (!fmtFloat) {
                    llvm::FunctionType* fmtFloatFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {builder->getDoubleTy(), builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtFloat = llvm::Function::Create(fmtFloatFnTy, llvm::Function::ExternalLinkage, "qc_fmt_float", module);
                }
                llvm::Function* fmtDouble = module->getFunction("qc_fmt_double");
                if (!fmtDouble) {
                    llvm::FunctionType* fmtDoubleFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {builder->getDoubleTy(), builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtDouble = llvm::Function::Create(fmtDoubleFnTy, llvm::Function::ExternalLinkage, "qc_fmt_double", module);
                }
                llvm::Function* fmtChar = module->getFunction("qc_fmt_char");
                if (!fmtChar) {
                    llvm::FunctionType* fmtCharFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {builder->getInt8Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtChar = llvm::Function::Create(fmtCharFnTy, llvm::Function::ExternalLinkage, "qc_fmt_char", module);
                }
                llvm::Function* fmtQBool = module->getFunction("qc_fmt_qbool");
                if (!fmtQBool) {
                    llvm::FunctionType* fmtQBoolFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {builder->getInt1Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtQBool = llvm::Function::Create(fmtQBoolFnTy, llvm::Function::ExternalLinkage, "qc_fmt_qbool", module);
                }
                llvm::Function* fmtBool = module->getFunction("qc_fmt_bool");
                if (!fmtBool) {
                    llvm::FunctionType* fmtBoolFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {builder->getInt8Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtBool = llvm::Function::Create(fmtBoolFnTy, llvm::Function::ExternalLinkage, "qc_fmt_bool", module);
                }
                llvm::Function* fmtPtr = module->getFunction("qc_fmt_ptr");
                if (!fmtPtr) {
                    llvm::FunctionType* fmtPtrFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {llvm::PointerType::get(context, 0), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtPtr = llvm::Function::Create(fmtPtrFnTy, llvm::Function::ExternalLinkage, "qc_fmt_ptr", module);
                }
                llvm::Function* fmtOctal = module->getFunction("qc_fmt_octal");
                if (!fmtOctal) {
                    llvm::FunctionType* fmtOctalFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {builder->getIntNTy(getPtrSize()), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtOctal = llvm::Function::Create(fmtOctalFnTy, llvm::Function::ExternalLinkage, "qc_fmt_octal", module);
                }
                llvm::Function* fmtHex = module->getFunction("qc_fmt_hex");
                if (!fmtHex) {
                    llvm::FunctionType* fmtHexFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0), {builder->getIntNTy(getPtrSize()), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtHex = llvm::Function::Create(fmtHexFnTy, llvm::Function::ExternalLinkage, "qc_fmt_hex", module);
                }
                llvm::Function* fmtScientific = module->getFunction("qc_fmt_scientific");
                if (!fmtScientific) {
                    llvm::FunctionType* fmtScientificFnTy = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {builder->getDoubleTy(), builder->getInt32Ty(), builder->getInt32Ty(), builder->getInt1Ty()}, false);
                    fmtScientific = llvm::Function::Create(fmtScientificFnTy, llvm::Function::ExternalLinkage, "qc_fmt_scientific", module);
                }
                for (size_t i = 0; i < fmtString.length(); i++) {
                    c = fmtString[i];
                    if (c != '%') {
                        to_print += c;
                        continue;
                    }
                    i++;
                    if (i > fmtString.length() - 1) {
                        cg_error((*varAccess)->var_name_tok.pos, "unexpected end of fmt string: " + funcName, "QC-S193");
                        return nullptr;
                    }
                    c = fmtString[i];
                    bool zero_pad = false;
                    int width = -1;
                    int precision = -1;
                    if (c == '%') {
                        to_print += '%';
                        continue;
                    }
                    if (c == '0') {
                        zero_pad = true;
                        i++;
                        if (i >= fmtString.size()) {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid formater: " + funcName, "QC-S194");
                            break;
                        }
                        c = fmtString[i];
                    }
                    if (std::isdigit(static_cast<unsigned char>(c))) {
                        std::string num;
                        while (i < fmtString.size() && std::isdigit(static_cast<unsigned char>(fmtString[i]))) {
                            num += fmtString[i];
                            i++;
                        }
                        c = fmtString[i];
                        width = std::stoi(num);
                    }
                    if (c == '.') {
                        i++;
                        if (i >= fmtString.size()) {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid formater: " + funcName, "QC-S194");
                            break;
                        }
                        c = fmtString[i];
                        if (std::isdigit(static_cast<unsigned char>(c))) {
                            std::string num;
                            while (i < fmtString.size() && std::isdigit(static_cast<unsigned char>(fmtString[i]))) {
                                num += fmtString[i];
                                i++;
                            }
                            c = fmtString[i];
                            precision = std::stoi(num);
                        } else {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid formater: " + funcName, "QC-S194");
                            break;
                        }
                    }
                    switch (c) {
                    case 'i': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* itgVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        llvm::Value* bigIntSigned = nullptr;
                        if (!itgVal || !itgVal->getType()->isIntegerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "%i formater takes an integer", "QC-S196");
                            return nullptr;
                        }
                        llvm::Type* i64Ty = builder->getIntNTy(getPtrSize());
                        unsigned bitWidth = itgVal->getType()->getIntegerBitWidth();

                        if (bitWidth < 64) {
                            bigIntSigned = builder->CreateSExt(itgVal, i64Ty);
                        } else if (bitWidth > 64) {
                            bigIntSigned = builder->CreateTrunc(itgVal, i64Ty);
                        } else {
                            bigIntSigned = itgVal;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtInt, {bigIntSigned, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                          llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                          llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'u': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* itgVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        llvm::Value* bigIntSigned = nullptr;
                        if (!itgVal || !itgVal->getType()->isIntegerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos,
                                     "%u formater takes an int-like (int, "
                                     "long int, short "
                                     "int, addr_t, nibble, byte)",
                                     "QC-S197");
                            return nullptr;
                        }
                        llvm::Type* i64Ty = builder->getIntNTy(getPtrSize());
                        unsigned bitWidth = itgVal->getType()->getIntegerBitWidth();

                        if (bitWidth < 64) {
                            bigIntSigned = builder->CreateZExt(itgVal, i64Ty);
                        } else if (bitWidth > 64) {
                            bigIntSigned = builder->CreateTrunc(itgVal, i64Ty);
                        } else {
                            bigIntSigned = itgVal;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtUInt, {bigIntSigned, llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 's': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            return nullptr;
                        }
                        llvm::Value* stVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!stVal) {
                            cg_error((*varAccess)->var_name_tok.pos,
                                     "failed to resolve argument for "
                                     "formatter in " +
                                         funcName,
                                     "QC-S198");
                            return nullptr;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        if (i >= fmtString.size()) {
                        } else if (fmtString[i + 1] == 't') {
                            i++;
                            llvm::Type* ty = stVal->getType();
                            if (auto structTy = llvm::dyn_cast<llvm::StructType>(stVal->getType())) {
                                if (structTy->hasName()) {
                                    std::string className = structTy->getName().str();

                                    if (classTypes.find(className) != classTypes.end()) {
                                        cg_error((*varAccess)->var_name_tok.pos,
                                                 "st formater takes a struct "
                                                 "instance: " +
                                                     funcName,
                                                 "QC-S199");
                                        break;
                                    } else if (structTypes.find(className) != structTypes.end()) {
                                        llvm::Function* nestedReprFn = module->getFunction(className + "_repr");
                                        if (nestedReprFn) {
                                            builder->CreateCall(printString, {builder->CreateCall(nestedReprFn, {stVal})});
                                        } else {
                                            builder->CreateCall(printString, {builder->CreateGlobalString("(unknown "
                                                                                                          "struct)")});
                                        }
                                    } else {
                                        cg_error((*varAccess)->var_name_tok.pos,
                                                 "st formater takes a struct "
                                                 "instance: " +
                                                     funcName,
                                                 "QC-S199");
                                        break;
                                    }
                                }
                            } else {
                                cg_error((*varAccess)->var_name_tok.pos,
                                         "st formater takes a struct "
                                         "instance: " +
                                             funcName,
                                         "QC-S199");
                                return nullptr;
                            }
                        } else {
                            if (!stVal->getType()->isPointerTy()) {
                                cg_error((*varAccess)->var_name_tok.pos, "s formater takes a string: " + funcName, "QC-S200");
                                return nullptr;
                            }
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtStr, {stVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                              llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        }
                        break;
                    }
                    case 'f': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* floatVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!floatVal || !floatVal->getType()->isFloatTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "f formater takes a float: " + funcName, "QC-S201");
                            return nullptr;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString, {builder->CreateCall(fmtFloat, {builder->CreateFPExt(floatVal, builder->getDoubleTy()),
                                                                                         llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                         llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                                         llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'd': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* doubVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!doubVal || !doubVal->getType()->isDoubleTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "d formater takes a double: " + funcName, "QC-S202");
                            return nullptr;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtDouble, {doubVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                             llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                             llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'c': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        llvm::Value* cVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (i >= fmtString.size()) {
                        } else if (fmtString[i + 1] == 's') {
                            i++;
                            llvm::Type* ty = cVal->getType();
                            if (auto structTy = llvm::dyn_cast<llvm::StructType>(cVal->getType())) {
                                if (structTy->hasName()) {
                                    std::string className = structTy->getName().str();

                                    if (classTypes.find(className) != classTypes.end()) {
                                        auto [reprMethod, ownerClass] = findMethodInHierarchy(className, "_repr");

                                        if (reprMethod) {
                                            std::vector<llvm::Value*> args;
                                            llvm::AllocaInst* temp = createEntryAlloca("temp_repr", ty);
                                            builder->CreateStore(cVal, temp);
                                            args.push_back(temp);

                                            builder->CreateCall(printString, {builder->CreateCall(reprMethod, args)});
                                        } else {
                                            to_print += "(reprless class)";
                                        }
                                    }
                                }
                            } else {
                                cg_error((*varAccess)->var_name_tok.pos, "cs formater takes a class instance: " + funcName, "QC-S203");
                                return nullptr;
                            }
                        } else {
                            if (!cVal || !cVal->getType()->isIntegerTy(8)) {
                                cg_error((*varAccess)->var_name_tok.pos, "c formater takes a char: " + funcName, "QC-S204");
                                return nullptr;
                            }
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtChar, {cVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                               llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        break;
                    }
                    case 'b': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* boolVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!boolVal || !boolVal->getType()->isIntegerTy(1)) {
                            cg_error((*varAccess)->var_name_tok.pos, "b formater takes a bool: " + funcName, "QC-S205");
                            return nullptr;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString, {builder->CreateCall(fmtBool, {boolVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                        llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'q': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* qboolVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!qboolVal || !qboolVal->getType()->isIntegerTy(2)) {
                            cg_error((*varAccess)->var_name_tok.pos, "q formater takes a qbool: " + funcName, "QC-S206");
                            return nullptr;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtQBool, {qboolVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                            llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'x': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* itgVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        llvm::Value* bigIntUnsigned;
                        if (!itgVal || !itgVal->getType()->isIntegerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "x formater takes a int: " + funcName, "QC-S207");
                            return nullptr;
                        }
                        llvm::Type* i64Ty = builder->getIntNTy(getPtrSize());
                        unsigned bitWidth = itgVal->getType()->getIntegerBitWidth();

                        if (bitWidth < 64) {
                            bigIntUnsigned = builder->CreateZExt(itgVal, i64Ty);
                        } else if (bitWidth > 64) {
                            bigIntUnsigned = builder->CreateTrunc(itgVal, i64Ty);
                        } else {
                            bigIntUnsigned = itgVal;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtHex, {bigIntUnsigned, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                          llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'o': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            break;
                        }
                        llvm::Value* itgVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!itgVal || !itgVal->getType()->isIntegerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "o formater takes a int: " + funcName, "QC-S208");
                            return nullptr;
                        }
                        llvm::Type* i64Ty = builder->getIntNTy(getPtrSize());
                        llvm::Value* bigIntUnsigned;
                        unsigned bitWidth = itgVal->getType()->getIntegerBitWidth();

                        if (bitWidth < 64) {
                            bigIntUnsigned = builder->CreateZExt(itgVal, i64Ty);
                        } else if (bitWidth > 64) {
                            bigIntUnsigned = builder->CreateTrunc(itgVal, i64Ty);
                        } else {
                            bigIntUnsigned = itgVal;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtOctal, {bigIntUnsigned, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                            llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'p': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            return nullptr;
                        }
                        llvm::Value* ptVal = emitExpr(goodArgs[current_arg]);
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        if (!ptVal || !ptVal->getType()->isPointerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "p formater takes a pointer: " + funcName, "QC-S209");
                            break;
                        }
                        builder->CreateCall(printString, {builder->CreateCall(fmtPtr, {ptVal, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                       llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'e': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            return nullptr;
                        }
                        llvm::Value* decimalVal = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);
                        if (!decimalVal ||
                            !decimalVal->getType()->isFloatTy() && !decimalVal->getType()->isDoubleTy() && !decimalVal->getType()->isIntegerTy()) {
                            cg_error((*varAccess)->var_name_tok.pos, "e formater takes a number: " + funcName, "QC-S210");
                        }
                        if (decimalVal->getType()->isIntegerTy()) {
                            decimalVal = builder->CreateSIToFP(decimalVal, builder->getDoubleTy());
                        } else if (decimalVal->getType()->isFloatTy()) {
                            decimalVal = builder->CreateFPExt(decimalVal, builder->getDoubleTy());
                        } else if (decimalVal->getType()->isDoubleTy()) {
                            decimalVal = decimalVal;
                        }
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        builder->CreateCall(printString,
                                            {builder->CreateCall(fmtScientific, {builder->CreateFPExt(decimalVal, builder->getDoubleTy()),
                                                                                 llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                 llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                                 llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                        break;
                    }
                    case 'a': {
                        current_arg++;
                        if (goodArgs.size() - 1 < current_arg) {
                            cg_error((*varAccess)->var_name_tok.pos, "too few args: " + funcName, "QC-S195");
                            return nullptr;
                        }

                        llvm::Value* val = derefIfReference(emitExpr(goodArgs[current_arg]), goodArgs[current_arg]);

                        if (!val) {
                            cg_error((*varAccess)->var_name_tok.pos, "failed to evaluate argument " + std::to_string(current_arg) + ": " + funcName,
                                     "QC-S211");
                            return nullptr;
                        }
                        llvm::Type* aTy = val->getType();
                        llvm::Value* strVal = builder->CreateGlobalString(to_print);
                        builder->CreateCall(printString, {strVal});
                        to_print = "";
                        if (aTy->isIntegerTy(32) || aTy->isIntegerTy(64) || aTy->isIntegerTy(16) || aTy->isIntegerTy(4)) {
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtInt, {builder->CreateZExt(val, builder->getIntNTy(getPtrSize())),
                                                                              llvm::ConstantInt::get(builder->getIntNTy(getPtrSize()), width),
                                                                              llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                              llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (auto structTy = llvm::dyn_cast<llvm::StructType>(aTy)) {
                            if (structTy->hasName()) {
                                std::string className = structTy->getName().str();
                                if (structTypes.find(className) != structTypes.end()) {
                                    llvm::Function* nestedReprFn = module->getFunction(className + "_repr");
                                    if (nestedReprFn) {
                                        builder->CreateCall(printString, {builder->CreateCall(nestedReprFn, {val})});
                                    } else {
                                        builder->CreateCall(printString, {builder->CreateGlobalString("(unknown struct)")});
                                    }
                                }
                            }
                            break;
                        }
                        std::string sourceType = getExpressionType(goodArgs[current_arg]);
                        if (sourceType == "string" || sourceType == "char*") {
                            builder->CreateCall(printString, {builder->CreateCall(fmtStr, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                           llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (aTy->isFloatTy()) {
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtFloat, {builder->CreateFPExt(val, builder->getDoubleTy()),
                                                                                llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                                llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (aTy->isDoubleTy()) {
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtDouble, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                 llvm::ConstantInt::get(builder->getInt32Ty(), precision),
                                                                                 llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (auto structTy = llvm::dyn_cast<llvm::StructType>(aTy)) {
                            if (structTy->hasName()) {
                                std::string className = structTy->getName().str();
                                if (classTypes.find(className) != classTypes.end()) {
                                    auto [reprMethod, ownerClass] = findMethodInHierarchy(className, "_repr");
                                    if (reprMethod) {
                                        std::vector<llvm::Value*> args;
                                        llvm::AllocaInst* temp = createEntryAlloca("temp_repr", aTy);
                                        builder->CreateStore(val, temp);
                                        args.push_back(temp);
                                        builder->CreateCall(printString, {builder->CreateCall(reprMethod, args)});
                                    } else {
                                        to_print += "(reprless class)";
                                    }
                                }
                            }
                            if (aTy->isIntegerTy(8)) {
                                builder->CreateCall(printString,
                                                    {builder->CreateCall(fmtChar, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                   llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                                break;
                            }
                            break;
                        }
                        if (aTy->isIntegerTy(1)) {
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtBool, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                               llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (aTy->isIntegerTy(2)) {
                            builder->CreateCall(printString,
                                                {builder->CreateCall(fmtQBool, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        if (aTy->isPointerTy()) {
                            builder->CreateCall(printString, {builder->CreateCall(fmtPtr, {val, llvm::ConstantInt::get(builder->getInt32Ty(), width),
                                                                                           llvm::ConstantInt::get(builder->getInt1Ty(), zero_pad)})});
                            break;
                        }
                        break;
                    }
                    default: cg_error((*varAccess)->var_name_tok.pos, "invalid formater: " + funcName, "QC-S194"); break;
                    }
                }
                llvm::Value* strVal = builder->CreateGlobalString(to_print);
                builder->CreateCall(printString, {strVal});
                to_print = "";
                return nullptr;
            }
            if (funcName == "`to_string" && !call.arg_nodes.empty()) {
                AnyNode& argNode = call.arg_nodes.front();
                llvm::Value* arg = emitExpr(argNode);
                if (!arg) return nullptr;
                return convertToString(arg, argNode, get_pos(*callPtr));
            }
            if (funcName == "`to_long_int" && !call.arg_nodes.empty()) {
                AnyNode& argNode = call.arg_nodes.front();
                llvm::Value* arg = emitExpr(argNode);
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "long int", get_pos(*callPtr));
            }
            if (funcName == "`to_short_int" && !call.arg_nodes.empty()) {
                AnyNode& argNode = call.arg_nodes.front();
                llvm::Value* arg = emitExpr(argNode);
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "short int", get_pos(*callPtr));
            }
            if (funcName == "`to_qbool" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "qbool", get_pos(*callPtr));
            }
            if (funcName == "`to_int" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "int", get_pos(*callPtr));
            }

            if (funcName == "`to_float" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "float", get_pos(*callPtr));
            }

            if (funcName == "`to_double" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "double", get_pos(*callPtr));
            }

            if (funcName == "`to_bool" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "bool", get_pos(*callPtr));
            }

            if (funcName == "`to_char" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "char", get_pos(*callPtr));
            }
            if (funcName == "`to_addr_t" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "addr_t", get_pos(*callPtr));
            }
            if (funcName == "`to_byte" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "byte", get_pos(*callPtr));
            }
            if (funcName == "`to_nibble" && !call.arg_nodes.empty()) {
                llvm::Value* arg = emitExpr(call.arg_nodes.front());
                if (!arg) return nullptr;
                return emitBuiltinConversion(arg, "nibble", get_pos(*callPtr));
            }
            if (funcName == "`mapped_ptr" && !call.arg_nodes.empty()) {
                llvm::Value* val = emitExpr(call.arg_nodes.front());
                if (!val || !(val->getType()->isIntegerTy())) {
                    cg_error((*varAccess)->var_name_tok.pos, "arg 1 must be a integer: " + funcName, "QC-S212");
                    return nullptr;
                }
                if (!(val->getType()->isIntegerTy(getPtrSize()))) {
                    cg_error((*varAccess)->var_name_tok.pos,
                             "arg 1 must be a integer the size of a pointer (" + std::to_string(getPtrSize()) + ") (addr_t or " +
                                 (getPtrSize() == 32 ? "int" : "long int") + ", got a " + std::to_string(val->getType()->getIntegerBitWidth()) +
                                 " bit integer (" +
                                 ((val->getType()->getIntegerBitWidth() == 32)
                                      ? "int"
                                      : ((val->getType()->getIntegerBitWidth() == 64) ? "long int" : "short int")) +
                                 ": " + funcName,
                             "QC-S213");
                    return nullptr;
                }
                return builder->CreateIntToPtr(val, builder->getPtrTy());
            }
            if (funcName == "`to_address" && !call.arg_nodes.empty()) {
                llvm::Value* val = emitExpr(call.arg_nodes.front());
                if (!val || !(val->getType()->isPointerTy())) {
                    cg_error((*varAccess)->var_name_tok.pos, "arg 1 must be a pointer: " + funcName, "QC-S214");
                    return nullptr;
                }
                return builder->CreatePtrToInt(val, builder->getIntNTy(getPtrSize()), "addr");
            }
            if (funcName == "`ternary" && !call.arg_nodes.empty()) {
                if (call.arg_nodes.size() != 3) {
                    cg_error((*varAccess)->var_name_tok.pos, "must have exactly 3 args: " + funcName, "QC-S215");
                    return nullptr;
                }
                auto condIt = call.arg_nodes.begin();
                auto trIt = std::next(condIt);
                auto flIt = std::next(trIt);
                llvm::Value* cond = emitExpr(*condIt);
                llvm::Value* is_tr = emitExpr(*trIt);
                llvm::Value* is_fl = emitExpr(*flIt);
                if (!cond || !is_tr || !is_fl) return nullptr;
                cond = toTruthiness(cond, get_pos(*condIt));
                if (!cond) return nullptr;
                if (!cond->getType()->isIntegerTy(1)) {
                    cg_error((*varAccess)->var_name_tok.pos, "arg 1 must be a boolean: " + funcName, "QC-S216");
                    return nullptr;
                }
                llvm::Type* trTy = is_tr->getType();
                llvm::Type* flTy = is_fl->getType();
                if (trTy != flTy) {
                    if (trTy->isIntegerTy() && flTy->isIntegerTy()) {
                        unsigned trBits = trTy->getIntegerBitWidth();
                        unsigned flBits = flTy->getIntegerBitWidth();
                        unsigned commonBits = std::max(trBits, flBits);
                        llvm::Type* commonTy = llvm::IntegerType::get(context, commonBits);
                        if (trTy != commonTy) { is_tr = builder->CreateSExt(is_tr, commonTy, "ternary_tr_promote"); }
                        if (flTy != commonTy) { is_fl = builder->CreateSExt(is_fl, commonTy, "ternary_fl_promote"); }
                    } else {
                        cg_error((*varAccess)->var_name_tok.pos, "arg 2 and 3 must have compatible types: " + funcName, "QC-T046");
                        return nullptr;
                    }
                }
                return builder->CreateSelect(cond, is_tr, is_fl, "select_val");
            }
            if (funcName == "`inline" && !call.arg_nodes.empty()) {
                StringNode* data = std::get_if<StringNode>(&call.arg_nodes.front());
                if (data == nullptr) {
                    cg_error((*varAccess)->var_name_tok.pos, "arg 1 must be a compile-time string: " + funcName, "QC-S217");
                    return nullptr;
                }
                int outputs = 0;
                int inputs = 0;
                std::string asm_text = data->tok.value;
                std::vector<AsmOp> output_ops;
                std::vector<AsmOp> input_ops;
                std::vector<std::string> clobbers;
                /*
                struct AsmOp {
      bool isOutput;
      int index;
      char kind;
      };  */
                bool isATT = false;
                std::string finalized = "";
                for (int i = 0; i < asm_text.length(); i++) {
                    char c = asm_text[i];
                    if (i == 0 && c == ';') {
                        if (!(i + 2 >= asm_text.size())) { isATT = asm_text[i + 1] == 'A' && asm_text[i + 2] == 'T' && asm_text[i + 3] == 'T'; }
                    }
                    if (c != '$') {
                        finalized += c;
                        continue;
                    } else {
                        if (asm_text.length() <= i + 1) {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid operand placeholder: " + funcName, "QC-S218");
                            return nullptr;
                        }
                        finalized += c;
                        i++;
                        AsmOp op;
                        op.isOutput = false;
                        op.isRW = false;
                        try {
                            if (i >= asm_text.size() || !std::isdigit(asm_text[i])) {
                                cg_error((*varAccess)->var_name_tok.pos, "expected number after $: " + funcName, "QC-S219");
                                return nullptr;
                            }
                            int index = 0;
                            while (i < asm_text.size() && std::isdigit(asm_text[i])) {
                                finalized += asm_text[i];
                                index = index * 10 + (asm_text[i] - '0');
                                i++;
                            }
                            op.index = index;
                        } catch (...) {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid operand index: " + funcName, "QC-S220");
                            return nullptr;
                        }
                        if (asm_text.length() <= i + 1) {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid operand placeholder: " + funcName, "QC-S218");
                            return nullptr;
                        }
                        if (i < asm_text.size() && asm_text[i] == '=') {
                            op.isOutput = true;
                            i++;
                        } else if (i < asm_text.size() && asm_text[i] == '+') {
                            op.isRW = true;
                            i++;
                        }
                        if (i >= asm_text.size()) {
                            cg_error((*varAccess)->var_name_tok.pos,
                                     "expected operand kind after asm "
                                     "operand index: " +
                                         funcName,
                                     "QC-S221");
                            return nullptr;
                        }
                        char kind = asm_text[i];
                        if (kind != 'r' && kind != 'm' && kind != 'i' && kind != 'g') {
                            cg_error((*varAccess)->var_name_tok.pos, "invalid asm operand kind: " + funcName, "QC-S222");
                            return nullptr;
                        }
                        op.kind = kind;
                        if (op.isOutput || op.isRW) {
                            outputs++;
                            output_ops.push_back(op);
                        } else {
                            inputs++;
                            input_ops.push_back(op);
                        }
                    }
                }
                StringNode* clobber_string_node = std::get_if<StringNode>(&call.arg_nodes.back());
                if (clobber_string_node == nullptr) {
                    cg_error((*varAccess)->var_name_tok.pos, "final arg must be a compile-time string: " + funcName, "QC-S223");
                    return nullptr;
                }
                std::string clobber_string = clobber_string_node->tok.value;
                size_t i = 0;
                while (i < clobber_string.size()) {
                    if (clobber_string[i] != '~') {
                        i++;
                        continue;
                    }
                    i++;
                    while (i < clobber_string.size() && isspace(clobber_string[i])) i++;
                    if (i >= clobber_string.size() || clobber_string[i] != '{') {
                        cg_error((*varAccess)->var_name_tok.pos, "invalid clobber syntax: expected '{'", "QC-S224");
                        return nullptr;
                    }
                    i++;
                    while (i < clobber_string.size()) {
                        while (i < clobber_string.size() && isspace(clobber_string[i])) { i++; }
                        std::string reg;
                        while (i < clobber_string.size() && clobber_string[i] != ',' && clobber_string[i] != '}') {
                            if (!isspace(clobber_string[i])) reg += clobber_string[i];
                            i++;
                        }
                        if (!reg.empty()) {
                            if (reg == "rsp" || reg == "esp" || reg == "rbp" || reg == "ebp") {
                                warn("asm-clobber-stack-pointer", (*varAccess)->var_name_tok.pos,
                                     reg + " is the stack pointer and cannot be clobbered because the compiler "
                                           "relies on it to track local variables and manage function calls and "
                                           "returns; modifying it may corrupt the stack and cause undefined behavior",
                                     "QC-W007");
                                return nullptr;
                            }
                            clobbers.push_back("~{" + reg + "}");
                        }
                        while (i < clobber_string.size() && isspace(clobber_string[i])) { i++; }
                        if (i < clobber_string.size() && clobber_string[i] == ',') {
                            i++;
                            continue;
                        }
                        if (i < clobber_string.size() && clobber_string[i] == '}') {
                            i++;
                            break;
                        }
                        cg_error((*varAccess)->var_name_tok.pos, "invalid clobber syntax: expected ',' or '}'", "QC-S226");
                        return nullptr;
                    }
                }
                std::unordered_set<int> output_indices;
                std::unordered_set<int> input_indices;
                for (const auto& op : output_ops) output_indices.insert(op.index);
                for (const auto& op : input_ops) input_indices.insert(op.index);

                for (int idx : input_indices) {
                    if (output_indices.contains(idx)) {
                        cg_error((*varAccess)->var_name_tok.pos, "asm operand " + std::to_string(idx) + " used as both input and output", "QC-S227");
                        return nullptr;
                    }
                }

                int output_count = (int)output_indices.size();
                for (int i = 0; i < output_count; i++) {
                    if (!output_indices.contains(i)) {
                        cg_error((*varAccess)->var_name_tok.pos,
                                 "output operands must be contiguous "
                                 "starting at index 0",
                                 "QC-S228");
                        return nullptr;
                    }
                }

                for (int idx : input_indices) {
                    if (idx < output_count) {
                        cg_error((*varAccess)->var_name_tok.pos, "input operands must come after all outputs", "QC-S229");
                        return nullptr;
                    }
                }
                std::map<int, AsmOp> unique_outputs;
                std::map<int, AsmOp> unique_inputs;
                for (const auto& op : output_ops) {
                    if (!unique_outputs.contains(op.index)) { unique_outputs[op.index] = op; }
                }
                for (const auto& op : input_ops) {
                    if (!unique_inputs.contains(op.index)) { unique_inputs[op.index] = op; }
                }
                std::vector<llvm::Type*> input_types;
                std::vector<llvm::Value*> input_values;
                std::vector<llvm::Value*> output_ptrs;
                std::vector<llvm::Type*> output_types;
                for (auto& [idx, op] : unique_inputs) {
                    int arg_pos = idx + 1;
                    if (arg_pos >= call.arg_nodes.size() - 1) {
                        cg_error((*varAccess)->var_name_tok.pos, "asm input index out of range", "QC-S230");
                        return nullptr;
                    }
                    auto it = std::next(call.arg_nodes.begin(), arg_pos);
                    llvm::Value* val = (op.kind == 'm' ? emitLValue(*it) : emitExpr(*it));
                    if (!val) return nullptr;
                    input_values.push_back(val);
                    input_types.push_back(val->getType());
                }
                std::vector<std::pair<unsigned, llvm::Type*>> memory_element_types;
                for (auto& [idx, op] : unique_outputs) {
                    int arg_pos = idx + 1;
                    auto it = std::next(call.arg_nodes.begin(), arg_pos);
                    llvm::Value* out_ptr = emitLValue(*it);
                    if (!out_ptr) return nullptr;
                    if (op.kind == 'm') {
                        input_values.push_back(out_ptr);
                        input_types.push_back(out_ptr->getType());
                        memory_element_types.push_back({(unsigned)input_values.size() - 1, llvmTypeFor(getExpressionType(*it))});
                    }
                    if (op.kind == 'r') {
                        auto type = getExpressionType(*it);
                        output_types.push_back(llvmTypeFor(type));
                        output_ptrs.push_back(out_ptr);
                    }
                }
                llvm::Type* return_ty = builder->getVoidTy();
                if (output_types.size() == 1) {
                    return_ty = output_types[0];
                } else if (output_types.size() > 1) {
                    return_ty = llvm::StructType::get(context, output_types);
                }
                llvm::FunctionType* fn_ty = llvm::FunctionType::get(return_ty, input_types, false);
                std::string constraints;
                bool first = true;
                for (auto& [idx, op] : unique_outputs) {
                    if (!first) constraints += ",";
                    if (op.kind == 'm') {
                        constraints += (op.isRW ? "+*m" : "=*m");
                    } else {
                        constraints += (op.isRW ? "+" : "=");
                        constraints += op.kind;
                    }
                    first = false;
                }
                for (auto& [idx, op] : unique_inputs) {
                    if (!first) constraints += ",";
                    constraints += op.kind;
                    first = false;
                }
                for (const auto& clobber : clobbers) {
                    if (!first) constraints += ",";
                    constraints += clobber;
                    first = false;
                }
                llvm::InlineAsm* asm_fn;
                if (isATT) {
                    asm_fn = llvm::InlineAsm::get(fn_ty, finalized, constraints, true);
                } else {
                    asm_fn = llvm::InlineAsm::get(fn_ty, finalized, constraints, true, false, llvm::InlineAsm::AD_Intel);
                }
                llvm::CallInst* asm_call = builder->CreateCall(fn_ty, asm_fn, input_values);
                for (auto& [idx, ty] : memory_element_types) {
                    llvm::Attribute attr = llvm::Attribute::get(context, llvm::Attribute::ElementType, ty);
                    asm_call->addParamAttr(idx, attr);
                }
                llvm::Value* asm_result = asm_call;
                if (output_types.empty()) { return nullptr; }
                if (output_types.size() == 1) {
                    builder->CreateStore(asm_result, output_ptrs[0]);
                    return asm_result;
                }
                for (unsigned i = 0; i < output_types.size(); ++i) {
                    llvm::Value* value = builder->CreateExtractValue(asm_result, {i}, "asm_output");
                    builder->CreateStore(value, output_ptrs[i]);
                }
                return asm_result;
            }
            if (funcName == "`next" && !call.arg_nodes.empty()) {
                if (auto acc = std::get_if<VarAccessNode*>(&call.arg_nodes.front())) {
                    std::string var_name = (*acc)->var_name_tok.value;
                    if (resolveVarType(var_name) != "...") {
                        cg_error((*varAccess)->var_name_tok.pos, "argument one must be a variadic argument: " + funcName, "QC-S231");
                        return nullptr;
                    }
                    StringNode* expectedType = std::get_if<StringNode>(&call.arg_nodes.back());
                    TypeValueNode* otherExpType = std::get_if<TypeValueNode>(&call.arg_nodes.back());
                    if (!expectedType && !otherExpType) {
                        cg_error((*varAccess)->var_name_tok.pos, "argument two must be a string storing the type or the type. (" + funcName + ")",
                                 "QC-T047");
                        return nullptr;
                    }
                    llvm::Value* ConvertedValue = nullptr;
                    llvm::Function* nextElem = module->getFunction("qc_variadic_next");
                    if (!nextElem) {
                        llvm::FunctionType* nextElemFnTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0),
                                                                                   {llvm::PointerType::get(context, 0)}, false);
                        nextElem = llvm::Function::Create(nextElemFnTy, llvm::Function::InternalLinkage, "qc_variadic_next", module);
                    }
                    llvm::Value* VariableAddr = resolveVariable(var_name);
                    llvm::Value* RawSlot = builder->CreateCall(nextElem, builder->CreateLoad(builder->getPtrTy(), VariableAddr, "variad"),
                                                               "variadc_arg");
                    llvm::Type* TargetType = llvmTypeFor(expectedType ? expectedType->tok.value : otherExpType->tok.value);
                    if (!TargetType) {
                        cg_error((*varAccess)->var_name_tok.pos, "argument two must be a valid type", "QC-T048");
                        return nullptr;
                    }
                    if (TargetType->isIntegerTy()) {
                        ConvertedValue = builder->CreatePtrToInt(RawSlot, TargetType, "vararg_int");
                    } else if (TargetType->isPointerTy()) {
                        ConvertedValue = builder->CreateBitCast(RawSlot, TargetType, "vararg_ptr");
                    } else if (TargetType->isFloatingPointTy()) {
                        llvm::Type* Int64Ty = builder->getIntNTy(getPtrSize());
                        llvm::Value* RawInt = builder->CreatePtrToInt(RawSlot, Int64Ty, "vararg_fp_bits");

                        if (TargetType->isFloatTy()) {
                            llvm::Value* Int32Trunc = builder->CreateTrunc(RawInt, builder->getInt32Ty());
                            ConvertedValue = builder->CreateBitCast(Int32Trunc, TargetType, "vararg_float");
                        } else {
                            ConvertedValue = builder->CreateBitCast(RawInt, TargetType, "vararg_double");
                        }
                    } else if (TargetType->isStructTy()) {
                        ConvertedValue = builder->CreateLoad(TargetType, RawSlot, "vararg_struct");
                    }
                    return ConvertedValue;
                } else {
                    cg_error((*varAccess)->var_name_tok.pos, "argument one must be a direct variadic argument: " + funcName, "QC-S232");
                }
                return nullptr;
            }
            if (funcName == "`is_empty" && !call.arg_nodes.empty()) {
                if (auto acc = std::get_if<VarAccessNode*>(&call.arg_nodes.back())) {
                    std::string var_name = (*acc)->var_name_tok.value;
                    if (resolveVarType(var_name) != "...") {
                        cg_error((*acc)->var_name_tok.pos, "argument must be a variadic argument: " + funcName, "QC-S233");
                        return nullptr;
                    }
                    llvm::Function* isEmpty = module->getFunction("qc_variadic_is_empty");
                    if (!isEmpty) {
                        llvm::FunctionType* isEmptyFnTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0),
                                                                                  {llvm::PointerType::get(context, 0)}, false);
                        isEmpty = llvm::Function::Create(isEmptyFnTy, llvm::Function::InternalLinkage, "qc_variadic_is_empty", module);
                    }
                    llvm::Value* VariableAddr = resolveVariable(var_name);
                    return builder->CreateCall(isEmpty, builder->CreateLoad(builder->getPtrTy(), VariableAddr, "variad"), "variadc_is_empty");
                } else {
                    cg_error(get_pos(call.arg_nodes.back()), "argument must be a direct variadic argument: " + funcName, "QC-S234");
                }
                return nullptr;
            }
            llvm::Function* fn = module->getFunction(runtimeName);
            if (!fn) {
                cg_error((*varAccess)->var_name_tok.pos, "built-in function not found in runtime: " + runtimeName, "QC-S235");
                return nullptr;
            }
            llvm::FunctionType* builtinFnTy = fn->getFunctionType();
            std::vector<std::string> emptyMetadata;
            std::vector<llvm::Value*> args = emitAdaptedArgs(call.arg_nodes, builtinFnTy, emptyMetadata);
            if (call.arg_nodes.size() != args.size()) return nullptr;
            llvm::Type* retTy = fn->getReturnType();
            return builder->CreateCall(fn, args, retTy->isVoidTy() ? "" : "builtin_call");
        }
    }
    if (llvm::Value* v = emitExpr(call.node_to_call)) {
        if (std::string className = getExpressionType(call.node_to_call); !className.empty()) {
            if (classTypes.find(className) != classTypes.end()) {
                std::vector<std::string> argTypes;
                for (auto& arg : call.arg_nodes) { argTypes.push_back(getExpressionType(arg)); }
                ClassMethodInfo* info = findMethodInfo(className, "operator()", argTypes);
                if (!info) {
                    cg_error(get_pos(*callPtr), "no matching operator() overload for class " + className, "QC-O002");
                    addMethodNotes(className, "operator()", argTypes, get_pos(&call));
                    return nullptr;
                }
                MethodCallNode* n = methodCallFromCall(*callPtr, "operator()");
                auto args = prepareArgs(info, n->args);
                delete n;
                bool isVariadic = !info->params.empty() && info->params.back().type.value == "...";
                if (isVariadic) {
                    size_t numFixedParams = info->params.size() - 1;
                    std::vector<llvm::Value*> varVals;
                    if (args.size() > numFixedParams) {
                        varVals.assign(args.begin() + numFixedParams, args.end());
                        args.resize(numFixedParams);
                    }
                    args.push_back(packVariadicArgs(varVals));
                }
                llvm::Function* opMethod = findMethodOverload(className, "operator()", args);
                if (!opMethod) {
                    cg_error(get_pos(*callPtr), "no matching operator() overload for class " + className, "QC-O002");
                    addMethodNotes(className, "operator()", args, get_pos(&call));
                    return nullptr;
                }
                return emitMethodCall(opMethod, v, args, "operator()");
            }
        }
    }
    llvm::Value* calleeVal = nullptr;
    llvm::FunctionType* fnTy = nullptr;
    std::string funcName = "";
    if (auto* varAccess = std::get_if<VarAccessNode*>(&call.node_to_call)) {
        std::string name = (*varAccess)->var_name_tok.value;
        llvm::Value* varAddr = getVarAddress(name);
        if (varAddr) {
            if (auto lmbt = resolveLambdaType(name)) {
                fnTy = lmbt;
                calleeVal = emitExpr(call.node_to_call);
            }
        } else {
            llvm::Function* resolved = resolveFunction(name);
            if (resolved) {
                calleeVal = resolved;
                fnTy = resolved->getFunctionType();
            }
        }
        if (!calleeVal) {
            cg_error((*varAccess)->var_name_tok.pos, "undeclared function or variable: " + name, "QC-S236");
            return nullptr;
        }
        funcName = name;
    }
    if (!fnTy) {
        cg_error(get_pos(&call), "could not determine function type", "QC-T049");
        return nullptr;
    }
    bool hasSpread = false;
    for (auto& argNode : call.arg_nodes) {
        if (std::holds_alternative<SpreadNode*>(argNode)) {
            hasSpread = true;
            break;
        }
    }

    if (hasSpread) {
        cg_error(get_pos(&call), "spread is no longer allowed in function calls.", "QC-S170");
        return nullptr;
    }

    std::vector<std::string> paramTypeStrings;
    std::string lastVarName = "";
    auto defIt = resolveFuncDefIt(funcName);
    if (defIt != functionDefs.end()) {
        for (auto& p : defIt->second->params) {
            paramTypeStrings.push_back(p.type.value);
            lastVarName = p.name.value;
        }
    }
    std::vector<llvm::Value*> args = emitAdaptedArgs(call.arg_nodes, fnTy, paramTypeStrings);
    if (call.arg_nodes.size() < args.size()) {
        cg_error(get_pos(&call),
                 "too few arguments to function: got " + std::to_string(call.arg_nodes.size()) + ", expected " + std::to_string(args.size()),
                 "QC-S237");

        cg_note(get_pos(&call), "missing " + std::to_string(args.size() - call.arg_nodes.size()) + " argument" +
                                    (args.size() - call.arg_nodes.size() == 1 ? "" : "s"));

        return nullptr;
    }
    if (!paramTypeStrings.empty() && paramTypeStrings.back() == "...") {
        if (lastVarName == "<varadic>") {

        } else {
            size_t num_fixed_args = paramTypeStrings.size() - 1;
            std::vector<llvm::Value*> var_vals(args.begin() + num_fixed_args, args.end());
            args.resize(num_fixed_args);
            llvm::Value* args_cnt = builder->getInt32(var_vals.size());
            llvm::Value* items_array = builder->CreateAlloca(builder->getPtrTy(), args_cnt, "varadics_array");
            for (size_t i = 0; i < var_vals.size(); ++i) {
                llvm::Value* index = builder->getInt32(i);
                llvm::Value* element_ptr = builder->CreateGEP(builder->getPtrTy(), items_array, index);
                llvm::Value* ValueToStore = var_vals[i];
                llvm::Type* valTy = ValueToStore->getType();
                if (valTy->isIntegerTy()) {
                    ValueToStore = builder->CreateIntToPtr(ValueToStore, builder->getPtrTy(), "vararg_int_to_ptr");
                } else if (valTy->isFloatingPointTy()) {
                    llvm::Value* Int64Bits = nullptr;
                    if (valTy->isFloatTy()) {
                        llvm::Value* Int32Bits = builder->CreateBitCast(ValueToStore, builder->getInt32Ty(), "float_to_i32");
                        Int64Bits = builder->CreateZExt(Int32Bits, builder->getIntNTy(getPtrSize()), "i32_to_i64");
                    } else {
                        Int64Bits = builder->CreateBitCast(ValueToStore, builder->getIntNTy(getPtrSize()), "double_to_i64");
                    }
                    ValueToStore = builder->CreateIntToPtr(Int64Bits, builder->getPtrTy(), "fp_bits_to_ptr");
                }
                builder->CreateStore(ValueToStore, element_ptr);
            }
            llvm::StructType* VaradicStructTy = llvm::StructType::get(context, {builder->getPtrTy(), builder->getInt32Ty(), builder->getInt32Ty()});
            llvm::Value* variadic_struct = builder->CreateAlloca(VaradicStructTy, nullptr, "variadic_struct");
            llvm::Value* Field0Ptr = builder->CreateStructGEP(VaradicStructTy, variadic_struct, 0);
            builder->CreateStore(items_array, Field0Ptr);
            llvm::Value* Field1Ptr = builder->CreateStructGEP(VaradicStructTy, variadic_struct, 1);
            builder->CreateStore(args_cnt, Field1Ptr);
            llvm::Value* Field2Ptr = builder->CreateStructGEP(VaradicStructTy, variadic_struct, 2);
            builder->CreateStore(builder->getInt32(0), Field2Ptr);
            args.push_back(variadic_struct);
        }
    }
    if (defIt != functionDefs.end()) {
        auto& fnDef = defIt->second;
        size_t paramIdx = 0;

        for (auto& param : fnDef->params) {
            if (paramIdx >= args.size()) {
                if (param.default_value.has_value()) {
                    AnyNode& defaultRef = const_cast<AnyNode&>(param.default_value.value());
                    llvm::Value* defVal = emitExpr(defaultRef);
                    if (!defVal) {
                        cg_error(get_pos(&call), "failed to evaluate default parameter", "QC-S168");
                        return nullptr;
                    }
                    args.push_back(defVal);
                } else {
                    cg_error(get_pos(&call), "missing required argument at position " + std::to_string(paramIdx), "QC-S169");
                    return nullptr;
                }
            }
            paramIdx++;
        }
    }
    llvm::Type* retTy = fnTy->getReturnType();
    if (insideTry()) {
        auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
        auto* invokeInst = builder->CreateInvoke(fnTy, calleeVal, contBB, currentLandingPad(), args, retTy->isVoidTy() ? "" : "calltmp");
        builder->SetInsertPoint(contBB);
        return retTy->isVoidTy() ? nullptr : invokeInst;
    }
    auto* callInst = builder->CreateCall(fnTy, calleeVal, args, retTy->isVoidTy() ? "" : "calltmp");
    return retTy->isVoidTy() ? nullptr : callInst;
}
llvm::Value* LLVMCompiler::emitArrAcc(ArrayAccessNode* arrAcc) {
    std::string ptrTy = getExpressionType(arrAcc->base);
    if (ptrTy.ends_with("*") || ptrTy == "@nullptr" || ptrTy == "string") {
        if (ptrTy == "@nullptr") {
            warn("null-deref", get_pos(arrAcc), "attempted to dereference nullptr", "QC-W008");
            return nullptr;
        }
        if (ptrTy == "void*") {
            cg_error(get_pos(arrAcc), "pointer arithmetic cannot be preformed on void pointers", "QC-S130");
            return nullptr;
        }
        llvm::Value* value = emitExpr(arrAcc->indices[0]);
        if (!value || !value->getType()->isIntegerTy()) {
            cg_error(get_pos(arrAcc->indices[0]), "attempted to index a pointer with a non-integer value.", "QC-S239");
            return nullptr;
        }
        if (ptrTy == "string") {
            ptrTy = "char";
        } else {
            ptrTy.pop_back();
        }
        llvm::Value* addr = builder->CreateGEP(llvmTypeFor(ptrTy), emitExpr(arrAcc->base), value, "ptr_arr_addr");
        return builder->CreateLoad(llvmTypeFor(ptrTy), addr, "ptr_arr_val");
    }
    if (genericiseOrFindClass(ptrTy)) {
        llvm::Value* obj = emitLValue(arrAcc->base, true);
        llvm::Value* idx = emitExpr(arrAcc->indices[0]);
        llvm::Value* ref = emitVirtualOrDirectCall(ptrTy, "operator[]", obj, {idx});
        if (!ref) {
            cg_error(get_pos(arrAcc), ptrTy + " does not have operator[]", "QC-S240");
            addMethodNotes(ptrTy, "operator[]", {idx}, get_pos(arrAcc));
            return nullptr;
        }
        return ref;
    }
    if (auto varAcc = safe_get<VarAccessNode>(arrAcc->base)) {
        std::string name = varAcc->var_name_tok.value;
        llvm::Value* alloc = getVarAddress(name);
        if (!alloc) {
            cg_error(get_pos(varAcc), "unknown array: " + name, "QC-S242");
            return nullptr;
        }

        llvm::Value* arrAlloc = alloc;
        llvm::Type* arrTy = getPointeeType(name);

        if (arrTy->isPointerTy()) {
            llvm::Value* ptr = builder->CreateLoad(arrTy, arrAlloc, resolveVolatileVar(name), "arr_ptr");
            llvm::Value* indexVal = emitExpr(arrAcc->indices[0]);
            if (!indexVal) return nullptr;
            auto it = findArrayType(name);
            if (it == arrayTypeStrings.end()) {
                cg_error(get_pos(varAcc), "failed to find array access type", "QC-T050");
                return nullptr;
            }
            std::string baseType = it->second;
            llvm::Type* elemTy = llvmTypeFor(baseType);
            llvm::Value* elemPtr = builder->CreateGEP(elemTy, ptr, indexVal, "arr_elem_ptr");
            return builder->CreateLoad(elemTy, elemPtr, resolveVolatileVar(name), "arr_elem");
        } else if (arrTy->isArrayTy()) {
            std::vector<llvm::Value*> indices = {builder->getInt32(0)};
            for (size_t i = 0; i < arrAcc->indices.size(); i++) {
                llvm::Value* indexVal = emitExpr(arrAcc->indices[i]);
                if (!indexVal) return nullptr;
                indices.push_back(indexVal);
            }

            llvm::Value* elemPtr = builder->CreateInBoundsGEP(arrTy, arrAlloc, indices, "arr_elem_ptr");
            llvm::Type* elemTy = arrTy;
            for (size_t i = 0; i < arrAcc->indices.size(); i++) {
                if (elemTy->isArrayTy()) { elemTy = elemTy->getArrayElementType(); }
            }
            return builder->CreateLoad(elemTy, elemPtr, resolveVolatileVar(name), "arr_elem");
        }
    }
    llvm::Value* base;
    llvm::Value* val = emitExpr(arrAcc->base);
    llvm::Type* elemTy;
    if (!val) {
        cg_error(get_pos(arrAcc->base), "failed to emit base of array access", "QC-S243");
        return nullptr;
    }
    if (val->getType()->isArrayTy()) {
        base = emitLValue(arrAcc->base);
        elemTy = llvm::cast<llvm::ArrayType>(val->getType())->getElementType();
        if (!elemTy) {
            cg_error(get_pos(arrAcc), "cannot determine element type for array access", "QC-T051");
            return nullptr;
        }
        llvm::Value* idx = emitExpr(arrAcc->indices[arrAcc->indices.size() - 1]);
        llvm::Value* elemPtr = builder->CreateInBoundsGEP(val->getType(), base, {builder->getInt32(0), idx}, "arr_elem_ptr");
        return builder->CreateLoad(elemTy, elemPtr, "arr_elem");
    } else {
        base = val;
        elemTy = llvmTypeFor(ptrTy.ends_with("*") ? ptrTy.substr(0, ptrTy.size() - 1) : ptrTy.substr(0, ptrTy.size() - 2));
        if (!elemTy) {
            cg_error(get_pos(arrAcc), "cannot determine element type for array access", "QC-T051");
            return nullptr;
        }
        llvm::Value* idx = emitExpr(arrAcc->indices[arrAcc->indices.size() - 1]);
        llvm::Value* elemPtr = builder->CreateGEP(elemTy, base, idx, "arr_elem_ptr");
        return builder->CreateLoad(elemTy, elemPtr, "arr_elem");
    }
}
llvm::Value* LLVMCompiler::emitPropAcc(PropertyAccessNode* const* propAccess) {
    std::string propName = (*propAccess)->property_name.value;
    std::string baseName = "";
    bool isEnum = false;
    if (auto varAccess = std::get_if<VarAccessNode*>(&*(*propAccess)->base)) {
        baseName = (*varAccess)->var_name_tok.value;
        std::string resolved = resolveTypeName(baseName, false);
        llvmTypeFor(resolved);
        auto enumIt = enumTypes.find(resolved);
        if (enumIt != enumTypes.end()) {
            isEnum = true;
            std::string fullName = resolved + "." + propName;
            auto memberIt = enumMemberInfo.find(fullName);
            if (memberIt != enumMemberInfo.end()) {
                std::pair<std::string, std::vector<std::string>> values = memberIt->second;
                llvm::Value* enumVal = llvm::ConstantAggregateZero::get(enumIt->second);
                enumVal = builder->CreateInsertValue(enumVal,
                                                     llvm::ConstantInt::get(enumIt->second->getElementType(0), std::stoull(values.first),
                                                                            userTypes.at(baseTypeName(resolved)).enumType.contains("int")),
                                                     {0});
                return enumVal;
            } else {
                cg_error(get_pos(*varAccess), "enum " + baseName + " has no member " + propName, "QC-S244");
                std::vector<std::pair<int, std::string>> suggestions;
                for (auto& entry : userTypes[resolved].enumEntries) {
                    int distance = levenshteinDistance(propName, entry.memberName);
                    if (distance <= 2) { suggestions.push_back({distance, entry.memberName}); }
                }
                std::sort(suggestions.begin(), suggestions.end());
                if (!suggestions.empty()) {
                    std::string note = "similar entries:";
                    for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                    cg_note(get_pos(*varAccess), note);
                }
                return nullptr;
            }
        }
    }

    if (isEnum) {
        cg_error(get_pos(*propAccess), "enum member not found", "QC-S245");
        return nullptr;
    }
    if (propName == "length") {
        if (hasArrayLength(baseName)) {
            auto lenIt = findArrayLength(baseName);
            return builder->getInt32(lenIt->second);
        }
        auto runtimeIt = runtimeArraySizes.find(baseName);
        if (runtimeIt != runtimeArraySizes.end()) { return builder->CreateLoad(builder->getInt32Ty(), runtimeIt->second, "runtime_len"); }
        if (hasLocal(baseName)) {
            llvm::Type* allocTy = getPointeeType(baseName);
            if (allocTy && allocTy->isArrayTy()) { return builder->getInt32(allocTy->getArrayNumElements()); }
        }
    }
    std::string exprType = getExpressionType(*(*propAccess)->base, false);
    llvm::Value* baseVal = emitLValue(*(*propAccess)->base, true);
    if (!baseVal) return nullptr;
    while (exprType.ends_with("*&") || exprType.ends_with("**")) {
        baseVal = builder->CreateLoad(builder->getPtrTy(), baseVal, "load_object_ptr");
        if (exprType.ends_with("*&")) {
            exprType.erase(exprType.size() - 2);
        } else {
            exprType.pop_back();
        }
    }
    if (exprType.ends_with("*") || exprType.ends_with("&")) { exprType.pop_back(); }
    llvm::Type* baseTy = llvmTypeFor(exprType);
    bool isPtr = baseVal->getType()->isPointerTy();
    if (auto structTy = llvm::dyn_cast<llvm::StructType>(baseTy)) {
        if (structTy->isLiteral()) {
            if (!std::all_of(propName.begin(), propName.end(), [](unsigned char c) -> bool { return std::isdigit(c); })) {
                cg_error(get_pos(*propAccess), "tuple indicies must be a integer", "QC-TPL0");
                return nullptr;
            }
            size_t fieldIdx = std::stoull(propName);
            if (structTy->getNumElements() <= fieldIdx) {
                cg_error(get_pos(*propAccess), "tuple indice too big for tuple", "QC-TPL1");
                cg_note(get_pos(*propAccess), "tuple has " + std::to_string(structTy->getNumElements()) + " fields, got " + propName);
                return nullptr;
            }
            if (isPtr) {
                llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, baseVal, fieldIdx, propName + "_ptr");
                return builder->CreateLoad(structTy->getElementType(fieldIdx), fieldPtr, propName);
            } else {
                return builder->CreateExtractValue(baseVal, fieldIdx, propName);
            }
        }
        std::string structName = structTy->getName().str();
        auto userTypeIt = userTypes.find(baseTypeName(structName));
        if (userTypeIt != userTypes.end() && userTypeIt->second.kind == UserTypeKind::Struct) {
            int fieldIdx = -1;
            for (size_t i = 0; i < userTypeIt->second.fields.size(); i++) {
                if (userTypeIt->second.fields[i].name == propName) {
                    fieldIdx = i;
                    break;
                }
            }

            if (fieldIdx == -1) {
                cg_error(get_pos(*propAccess), "struct " + structName + " has no field " + propName, "QC-S246");
                if (propName.length() > 3) {
                    std::vector<std::pair<int, std::string>> suggestions;
                    for (auto& field : userTypes[baseTypeName(structName)].fields) {
                        int distance = levenshteinDistance(propName, field.name);
                        if (distance <= 2) { suggestions.push_back({distance, field.name}); }
                    }
                    std::sort(suggestions.begin(), suggestions.end());
                    if (!suggestions.empty()) {
                        std::string note = "similar fields:";
                        for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                        cg_note(get_pos(*propAccess), note);
                    }
                }

                return nullptr;
            }
            if (isPtr) {
                llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, baseVal, fieldIdx, propName + "_ptr");
                return builder->CreateLoad(structTy->getElementType(fieldIdx), fieldPtr, propName);
            } else {
                return builder->CreateExtractValue(baseVal, fieldIdx, propName);
            }
        }
    }
    for (auto& [className, classTy] : classTypes) {
        if (baseTy == classTy) {
            int fieldIdx = getFlattenedFieldIndex(baseTypeName(className), propName);

            if (fieldIdx == -1) {
                cg_error(get_pos(*propAccess), "field " + propName + " not found in class " + baseTypeName(className), "QC-S247");
                if (propName.length() > 3) {
                    std::vector<std::pair<int, std::string>> suggestions;
                    for (auto& field : userTypes[baseTypeName(className)].classFields) {
                        int distance = levenshteinDistance(propName, field.name);
                        if (distance <= 2) { suggestions.push_back({distance, field.name}); }
                    }
                    std::sort(suggestions.begin(), suggestions.end());
                    if (!suggestions.empty()) {
                        std::string note = "similar fields:";
                        for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                        cg_note(get_pos(*propAccess), note);
                    }
                }
                return nullptr;
            }
            auto [fieldOwnerClass, fieldAccess] = getFieldOwner(baseTypeName(className), propName);
            if (!canAccessField(currentClassName, fieldOwnerClass, fieldAccess)) {
                cg_error(get_pos(*propAccess), "cannot access " + fieldAccess + " field " + propName, "QC-S248");
                return nullptr;
            }
            llvm::Type* fieldTy = classTy->getElementType(fieldIdx);
            for (auto& [unionName, unionTy] : unionTypes) {
                if (fieldTy == unionTy) {
                    if (isPtr) {
                        llvm::Value* fieldPtr = builder->CreateStructGEP(classTy, baseVal, fieldIdx);
                        return builder->CreateLoad(unionTy, fieldPtr, "union_field");
                    }
                    return builder->CreateExtractValue(baseVal, fieldIdx, "union_field");
                }
            }
            if (isPtr) {
                llvm::Value* fieldPtr = builder->CreateStructGEP(classTy, baseVal, fieldIdx);
                return builder->CreateLoad(fieldTy, fieldPtr, propName);
            } else {
                return builder->CreateExtractValue(baseVal, fieldIdx, propName);
            }
        }
    }
    for (auto& [unionName, unionTy] : unionTypes) {
        if (baseTy == unionTy) {
            auto unionInfo = genericiseOrFindUnion(unionName);

            for (auto& member : unionInfo.members) {
                std::string resolvedBaseType = resolveTypeName(member.type, false);
                std::string resolvedVariant = resolveTypeName(member.type);
                if (classTypes.find(resolvedVariant) != classTypes.end()) {
                    int fieldIdx = getFlattenedFieldIndex(resolvedVariant, propName);
                    if (fieldIdx != -1) {
                        llvm::Value* varAlloc = nullptr;
                        if (auto varAcc = *std::get_if<VarAccessNode*>(&*(*propAccess)->base)) {
                            varAlloc = getVarAddress(varAcc->var_name_tok.value);
                        }
                        if (!varAlloc) return nullptr;

                        llvm::Value* dataFieldPtr = builder->CreateStructGEP(unionTy, varAlloc, 1, "union_data_ptr");
                        llvm::Value* dataPtr = builder->CreateLoad(llvm::PointerType::get(context, 0), dataFieldPtr, "union_data");
                        std::string inner = resolvedBaseType.substr(resolvedBaseType.find('<') + 1,
                                                                    resolvedBaseType.size() - resolvedBaseType.find('<') - 2);
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

                        llvm::StructType* classTy;
                        if (genericClasses[resolvedVariant]) {
                            classTy = generateGenericClass(resolvedVariant, userTypes.find(resolvedVariant)->second, genericParams);
                            if (classTy == nullptr) {
                                cg_error(get_pos(*propAccess), "failed to create specialized version of class " + resolvedVariant, "QC-S249");
                                return nullptr;
                            }
                        } else {
                            classTy = genericiseOrFindClass(resolvedBaseType);
                        }
                        llvm::Value* castedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));

                        llvm::Type* fieldTy = classTy->getElementType(fieldIdx);
                        llvm::Value* fieldPtr = builder->CreateStructGEP(classTy, castedPtr, fieldIdx);
                        return builder->CreateLoad(fieldTy, fieldPtr, propName);
                    }
                }
                if (structTypes.find(resolvedVariant) != structTypes.end()) {
                    auto& structInfo = userTypes.at(baseTypeName(resolvedVariant));
                    int fieldIdx = -1;
                    for (size_t i = 0; i < structInfo.fields.size(); i++) {
                        if (structInfo.fields[i].name == propName) {
                            fieldIdx = i;
                            break;
                        }
                    }
                    if (fieldIdx != -1) {
                        llvm::Value* varAlloc = nullptr;
                        if (auto varAcc = safe_get<VarAccessNode>(*(*propAccess)->base)) { varAlloc = getVarAddress(varAcc->var_name_tok.value); }
                        if (!varAlloc) return nullptr;

                        llvm::Value* dataFieldPtr = builder->CreateStructGEP(unionTy, varAlloc, 1, "union_data_ptr");
                        llvm::Value* dataPtr = builder->CreateLoad(llvm::PointerType::get(context, 0), dataFieldPtr, "union_data");

                        llvm::StructType* structTy = genericiseOrFindStruct(resolvedVariant);
                        llvm::Value* castedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));

                        llvm::Type* fieldTy = structTy->getElementType(fieldIdx);
                        llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, castedPtr, fieldIdx);
                        return builder->CreateLoad(fieldTy, fieldPtr, propName);
                    }
                }
            }
        }
    }
    cg_error((*propAccess)->property_name.pos, "unknown property: " + propName, "QC-S250");
    return nullptr;
}
llvm::Value* LLVMCompiler::emitMthdCall(MethodCallNode* const* methodCall) {
    auto* call = methodCall;
    std::string methodName = (*call)->method_name.value;
    llvm::Value* thisPtr = nullptr;
    std::string targetClass = "";
    if (auto varAccess = std::get_if<VarAccessNode*>(&(*call)->base)) {
        std::string varName = (*varAccess)->var_name_tok.value;
        std::string resolved = resolveTypeName(varName, false);
        llvmTypeFor(resolved);
        auto enumIt = enumTypes.find(resolved);
        if (enumIt != enumTypes.end()) {
            std::string fullName = resolved + "." + methodName;
            auto memberIt = enumMemberInfo.find(fullName);
            if (memberIt != enumMemberInfo.end()) {
                std::pair<std::string, std::vector<std::string>> values = memberIt->second;
                llvm::Value* enumVal = llvm::ConstantAggregateZero::get(enumIt->second);
                enumVal = builder->CreateInsertValue(enumVal,
                                                     llvm::ConstantInt::get(enumIt->second->getElementType(0), std::stoull(values.first),
                                                                            userTypes.at(baseTypeName(resolved)).enumType.contains("int")),
                                                     {0});
                if ((*call)->args.size() != values.second.size()) {
                    cg_error(get_pos(*varAccess), "incorrect number of tag values for enum member " + fullName, "QC-EM03");
                    cg_note(get_pos(*varAccess),
                            "expected " + std::to_string(values.second.size()) + ", got " + std::to_string((*call)->args.size()));
                    return nullptr;
                }
                for (unsigned int i = 0; i < values.second.size(); i++) {
                    AnyNode argNode = (*call)->args[i];
                    if (resolveTypeName(getExpressionType(argNode), false) != resolveTypeName(values.second[i], false)) {
                        cg_error(get_pos(argNode),
                                 "type mismatch: expected enum tag type to be " + values.second[i] + ", got " + getExpressionType(argNode));
                        return nullptr;
                    }
                    llvm::Value* arg = emitExpr(argNode);
                    llvm::Type* storageTy = enumIt->second->getElementType(i + 1);
                    auto* storageArrTy = llvm::cast<llvm::ArrayType>(storageTy);
                    llvm::Value* storage = builder->CreateAlloca(storageTy, nullptr, "enum_payload");
                    builder->CreateStore(llvm::ConstantAggregateZero::get(storageArrTy), storage);
                    llvm::Value* src = builder->CreateAlloca(arg->getType(), nullptr, "enum_arg");
                    builder->CreateStore(arg, src);
                    builder->CreateMemCpyInline(
                        storage, llvm::MaybeAlign(), src, llvm::MaybeAlign(),
                        llvm::ConstantInt::get(builder->getInt64Ty(), module->getDataLayout().getTypeStoreSize(arg->getType())));
                    llvm::Value* payload = builder->CreateLoad(storageTy, storage, "enum_payload");
                    enumVal = builder->CreateInsertValue(enumVal, payload, {i + 1});
                }
                return enumVal;
            } else {
                cg_error(get_pos(*varAccess), "enum " + resolved + " has no member " + methodName, "QC-S244");
                std::vector<std::pair<int, std::string>> suggestions;
                for (auto& entry : userTypes[resolved].enumEntries) {
                    int distance = levenshteinDistance(methodName, entry.memberName);
                    if (distance <= 2) { suggestions.push_back({distance, entry.memberName}); }
                }
                std::sort(suggestions.begin(), suggestions.end());
                if (!suggestions.empty()) {
                    std::string note = "similar entries:";
                    for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                    cg_note(get_pos(*varAccess), note);
                }
                return nullptr;
            }
        }
        if (varName == "this") {
            thisPtr = currentThis;
            targetClass = currentClassName;
        } else {
            thisPtr = getVarAddress(varName);
            llvm::Type* pTy = getPointeeType(varName);
            if (pTy) {
                auto* st = llvm::dyn_cast<llvm::StructType>(pTy);
                std::string typeName = st ? st->getName().str() : "";
                auto unionIt = unionTypes.find(typeName);
                if (unionIt != unionTypes.end()) {
                    llvm::Function* F = builder->GetInsertBlock()->getParent();
                    llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(context, "union.bad", F);
                    llvm::BasicBlock* joinBB = llvm::BasicBlock::Create(context, "union.join", F);
                    llvm::Value* tagPtr = builder->CreateStructGEP(unionIt->second, thisPtr, 0);
                    llvm::Value* tagVal = builder->CreateLoad(builder->getInt32Ty(), tagPtr);
                    llvm::SwitchInst* sw = builder->CreateSwitch(tagVal, defaultBB);
                    llvm::Value* unionPtr = thisPtr;
                    builder->SetInsertPoint(defaultBB);
                    builder->CreateUnreachable();
                    builder->SetInsertPoint(joinBB);
                    llvm::Value* result = nullptr;
                    int idx = 0;
                    for (auto m : genericiseOrFindUnion(typeName).members) {
                        std::string ty = resolveTypeName(m.type, false);
                        if (!classTypes.count(ty) && !genericClasses[ty]) {
                            idx++;
                            continue;
                        }
                        llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "union.case", F);
                        sw->addCase(builder->getInt32(idx), caseBB);
                        builder->SetInsertPoint(caseBB);
                        llvm::Value* payloadPtr = builder->CreateStructGEP(unionIt->second, unionPtr, 1);
                        llvm::Value* payload = builder->CreateLoad(builder->getPtrTy(), payloadPtr);
                        std::vector<std::string> argTypes;
                        for (auto& arg : (*call)->args) { argTypes.push_back(getExpressionType(arg)); }
                        ClassMethodInfo* info = findMethodInfo(ty, (*call)->method_name.value, argTypes);
                        if (!info) {
                            cg_error((*call)->method_name.pos, "no overload found", "QC-O003");
                            struct Candidate {
                                int score;
                                ClassMethodInfo* method;
                            };
                            std::vector<Candidate> candidates;
                            for (auto& method : userTypes.at(baseTypeName(baseTypeName(ty))).classMethods) {
                                if (method.is_constructor || (method.name_tok.value != methodName)) continue;
                                int score = 0;
                                size_t argCount = (*call)->args.size();
                                size_t paramCount = method.params.size();
                                score -= std::abs((int)argCount - (int)paramCount) * 5;
                                size_t count = std::min(argCount, paramCount);
                                for (size_t i = 0; i < count; i++) {
                                    llvm::Type* argTy = emitExpr((*call)->args[i])->getType();
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
                            if (candidates.empty()) {
                                if (methodName.length() < 3) return nullptr;
                                std::vector<std::pair<int, std::string>> suggestions;
                                for (auto& method : userTypes[baseTypeName(ty)].classMethods) {
                                    int distance = levenshteinDistance(methodName, method.name_tok.value);
                                    if (distance <= 2) { suggestions.push_back({distance, method.name_tok.value}); }
                                }
                                std::sort(suggestions.begin(), suggestions.end());
                                if (!suggestions.empty()) {
                                    std::string note = "similar methods:";
                                    for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                                    cg_note(get_pos(*varAccess), note);
                                }
                                return nullptr;
                            }
                            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                            if (candidates[0].score > 0) {
                                cg_note(get_pos(*varAccess), "closest matching overload: " + candidates[0].method->print());
                            }
                            if (candidates.size() <= 5) {
                                std::string note = "available overloads:";
                                for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
                                cg_note(get_pos(*varAccess), note);
                            } else {
                                std::string note = "other overloads:";
                                size_t shown = 0;
                                for (auto& candidate : candidates) {
                                    if (shown >= 3) break;
                                    note += "\n  - " + candidate.method->print();
                                    shown++;
                                }
                                cg_note(get_pos(*varAccess), note);
                            }
                            return nullptr;
                        }
                        auto args = prepareArgs(info, (*call)->args);
                        llvm::Value* callResult = emitVirtualOrDirectCall(ty, methodName, payload, args);
                        result = callResult;
                        builder->CreateBr(joinBB);
                        idx++;
                    }
                    builder->SetInsertPoint(joinBB);
                    return result;
                } else if (st) {
                    std::string typeStr = resolveVarType(varName);
                    if (typeStr.ends_with("*")) {
                        thisPtr = builder->CreateLoad(builder->getPtrTy(), getVarAddress(varName), "loaded_ptr");
                    } else {
                        thisPtr = getVarAddress(varName);
                    }
                    targetClass = typeName;
                }
            }
        }
    } else if (auto propAcc = safe_get<PropertyAccessNode>((*call)->base)) {
        llvm::Value* baseAddr = emitLValue(*(propAcc->base), true);
        std::string ownerClass = getExpressionType(*(propAcc->base));
        llvm::Type* baseTy = baseAddr->getType();
        if (auto* st = llvm::dyn_cast<llvm::StructType>(baseTy)) {
            std::string unionName = st->getName().str();
            auto unionIt = unionTypes.find(unionName);
            if (unionIt != unionTypes.end()) {
                llvm::Function* F = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(context, "union.bad", F);
                llvm::BasicBlock* joinBB = llvm::BasicBlock::Create(context, "union.join", F);
                llvm::Value* tagPtr = builder->CreateStructGEP(unionIt->second, baseAddr, 0);
                llvm::Value* tagVal = builder->CreateLoad(builder->getInt32Ty(), tagPtr);
                llvm::SwitchInst* sw = builder->CreateSwitch(tagVal, defaultBB);
                llvm::Value* unionPtr = baseAddr;
                builder->SetInsertPoint(defaultBB);
                builder->CreateUnreachable();
                builder->SetInsertPoint(joinBB);
                llvm::Value* result = nullptr;
                int idx = 0;
                for (auto& m : genericiseOrFindUnion(unionName).members) {
                    std::string ty = resolveTypeName(m.type, false);
                    if (!classTypes.count(ty) && !genericClasses[ty]) {
                        idx++;
                        continue;
                    }
                    llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "union.case", F);
                    sw->addCase(builder->getInt32(idx), caseBB);
                    builder->SetInsertPoint(caseBB);
                    llvm::Value* payloadPtr = builder->CreateStructGEP(unionIt->second, unionPtr, 1);
                    llvm::Value* payload = builder->CreateLoad(builder->getPtrTy(), payloadPtr);
                    std::vector<std::string> argTypes;
                    for (auto& arg : (*call)->args) { argTypes.push_back(getExpressionType(arg)); }
                    ClassMethodInfo* info = findMethodInfo(ty, (*call)->method_name.value, argTypes);
                    if (!info) {
                        cg_error((*call)->method_name.pos, "no overload found", "QC-O003");
                        struct Candidate {
                            int score;
                            ClassMethodInfo* method;
                        };
                        std::vector<Candidate> candidates;
                        for (auto& method : userTypes.at(baseTypeName(baseTypeName(ty))).classMethods) {
                            if (method.is_constructor || (method.name_tok.value != methodName)) continue;
                            int score = 0;
                            size_t argCount = (*call)->args.size();
                            size_t paramCount = method.params.size();
                            score -= std::abs((int)argCount - (int)paramCount) * 5;
                            size_t count = std::min(argCount, paramCount);
                            for (size_t i = 0; i < count; i++) {
                                llvm::Type* argTy = emitExpr((*call)->args[i])->getType();
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
                        if (candidates.empty()) {
                            if (methodName.length() < 3) return nullptr;
                            std::vector<std::pair<int, std::string>> suggestions;
                            for (auto& method : userTypes[baseTypeName(ty)].classMethods) {
                                int distance = levenshteinDistance(methodName, method.name_tok.value);
                                if (distance <= 2) { suggestions.push_back({distance, method.name_tok.value}); }
                            }
                            std::sort(suggestions.begin(), suggestions.end());
                            if (!suggestions.empty()) {
                                std::string note = "similar methods:";
                                for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                                cg_note((*call)->method_name.pos, note);
                            }
                            return nullptr;
                        }
                        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                        if (candidates[0].score > 0) {
                            cg_note((*call)->method_name.pos, "closest matching overload: " + candidates[0].method->print());
                        }
                        if (candidates.size() <= 5) {
                            std::string note = "available overloads:";
                            for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
                            cg_note((*call)->method_name.pos, note);
                        } else {
                            std::string note = "other overloads:";
                            size_t shown = 0;
                            for (auto& candidate : candidates) {
                                if (shown >= 3) break;
                                note += "\n  - " + candidate.method->print();
                                shown++;
                            }
                            cg_note((*call)->method_name.pos, note);
                        }
                        return nullptr;
                    }
                    auto args = prepareArgs(info, (*call)->args);
                    llvm::Value* callResult = emitVirtualOrDirectCall(ty, methodName, payload, args);
                    result = callResult;
                    builder->CreateBr(joinBB);
                    idx++;
                }
                builder->SetInsertPoint(joinBB);
                return result;
            } else if (st) {
                targetClass = unionName;
            }
        }
        llvm::StructType* structType;
        if (genericClasses[baseTypeName(ownerClass)]) {
            structType = generateGenericClass(baseTypeName(ownerClass), userTypes.find(baseTypeName(ownerClass))->second,
                                              genericParamsFromName(ownerClass));
            if (structType == nullptr) {
                cg_error(get_pos(*call), "failed to create specialized version of class " + baseTypeName(ownerClass), "QC-S249");
                return nullptr;
            }
        } else {
            structType = llvm::StructType::getTypeByName(context, baseTypeName(ownerClass));
        }
        unsigned fieldIndex = 0;
        bool found = false;
        const auto& uType = userTypes.at(baseTypeName(baseTypeName(ownerClass)));
        if (uType.kind == UserTypeKind::Struct) {
            for (size_t i = 0; i < uType.fields.size(); ++i) {
                if (uType.fields[i].name == propAcc->property_name.value) {
                    fieldIndex = (unsigned)i;
                    found = true;
                    break;
                }
            }
        } else {
            int idx = getFlattenedFieldIndex(baseTypeName(ownerClass), propAcc->property_name.value);
            if (idx >= 0) {
                fieldIndex = static_cast<unsigned>(idx);
                found = true;
            }
        }
        if (!found) { return (cg_error((*call)->method_name.pos, "field not found", "QC-S251"), nullptr); }
        llvm::Value* fieldAddr = builder->CreateStructGEP(structType, baseAddr, fieldIndex);
        thisPtr = fieldAddr;
        AnyNode temp = AnyNode(propAcc);
        targetClass = getExpressionType(temp);
    } else {
        llvm::Value* baseVal = emitLValue((*call)->base, true);
        if (!baseVal) {
            cg_error(get_pos(*call), "Failed to emit base of callnode", "QC-S252");
            return nullptr;
        }
        llvm::Type* baseTy = baseVal->getType();
        if (auto* st = llvm::dyn_cast<llvm::StructType>(baseTy)) {
            std::string unionName = st->getName().str();
            auto unionIt = unionTypes.find(unionName);
            if (unionIt != unionTypes.end()) {
                llvm::Function* F = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(context, "union.bad", F);
                llvm::BasicBlock* joinBB = llvm::BasicBlock::Create(context, "union.join", F);
                llvm::Value* tagPtr = builder->CreateStructGEP(unionIt->second, baseVal, 0);
                llvm::Value* tagVal = builder->CreateLoad(builder->getInt32Ty(), tagPtr);
                llvm::SwitchInst* sw = builder->CreateSwitch(tagVal, defaultBB);
                builder->SetInsertPoint(defaultBB);
                builder->CreateUnreachable();
                builder->SetInsertPoint(joinBB);
                llvm::Value* result = nullptr;
                int idx = 0;
                for (auto& m : genericiseOrFindUnion(unionName).members) {
                    std::string ty = resolveTypeName(m.type);
                    if (!classTypes.count(ty)) {
                        idx++;
                        continue;
                    }
                    llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "union.case", F);
                    sw->addCase(builder->getInt32(idx), caseBB);
                    builder->SetInsertPoint(caseBB);
                    llvm::Value* payloadPtr = builder->CreateStructGEP(unionIt->second, baseVal, 1);
                    llvm::Value* payload = builder->CreateLoad(builder->getPtrTy(), payloadPtr);
                    std::vector<std::string> argTypes;
                    for (auto& arg : (*call)->args) { argTypes.push_back(getExpressionType(arg)); }
                    ClassMethodInfo* info = findMethodInfo(ty, (*call)->method_name.value, argTypes);
                    if (!info) {
                        cg_error((*call)->method_name.pos, "no overload found", "QC-O003");
                        struct Candidate {
                            int score;
                            ClassMethodInfo* method;
                        };
                        std::vector<Candidate> candidates;
                        for (auto& method : userTypes.at(baseTypeName(baseTypeName(ty))).classMethods) {
                            if (method.is_constructor || (method.name_tok.value != methodName)) continue;
                            int score = 0;
                            size_t argCount = (*call)->args.size();
                            size_t paramCount = method.params.size();
                            score -= std::abs((int)argCount - (int)paramCount) * 5;
                            size_t count = std::min(argCount, paramCount);
                            for (size_t i = 0; i < count; i++) {
                                llvm::Type* argTy = emitExpr((*call)->args[i])->getType();
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
                        if (candidates.empty()) {
                            if (methodName.length() < 3) return nullptr;
                            std::vector<std::pair<int, std::string>> suggestions;
                            for (auto& method : userTypes[baseTypeName(ty)].classMethods) {
                                int distance = levenshteinDistance(methodName, method.name_tok.value);
                                if (distance <= 2) { suggestions.push_back({distance, method.name_tok.value}); }
                            }
                            std::sort(suggestions.begin(), suggestions.end());
                            if (!suggestions.empty()) {
                                std::string note = "similar methods:";
                                for (auto& [distance, name] : suggestions) { note += "\n  - `" + name + "`"; }
                                cg_note((*call)->method_name.pos, note);
                            }
                            return nullptr;
                        }
                        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                        if (candidates[0].score > 0) {
                            cg_note((*call)->method_name.pos, "closest matching overload: " + candidates[0].method->print());
                        }
                        if (candidates.size() <= 5) {
                            std::string note = "available overloads:";
                            for (auto& candidate : candidates) { note += "\n  - " + candidate.method->print(); }
                            cg_note((*call)->method_name.pos, note);
                        } else {
                            std::string note = "other overloads:";
                            size_t shown = 0;
                            for (auto& candidate : candidates) {
                                if (shown >= 3) break;
                                note += "\n  - " + candidate.method->print();
                                shown++;
                            }
                            cg_note((*call)->method_name.pos, note);
                        }
                        return nullptr;
                    }
                    auto args = prepareArgs(info, (*call)->args);
                    llvm::Value* callResult = emitVirtualOrDirectCall(ty, methodName, payload, args);
                    result = callResult;
                    builder->CreateBr(joinBB);
                    idx++;
                }
                builder->SetInsertPoint(joinBB);
                return result;
            }
        }
        std::string exprTy = getExpressionType((*call)->base, false);
        if (exprTy.ends_with("*")) {
            targetClass = exprTy.substr(0, exprTy.size() - 1);
            thisPtr = baseVal;
        } else if (auto* sTy = llvm::dyn_cast<llvm::StructType>(baseVal->getType())) {
            targetClass = sTy->getName().str();
            if (baseVal->getType()->isPointerTy()) {
                thisPtr = baseVal;
            } else if (auto* sTy = llvm::dyn_cast<llvm::StructType>(baseVal->getType())) {
                targetClass = sTy->getName().str();
                thisPtr = createEntryAlloca("temp_this", sTy);
                builder->CreateStore(baseVal, thisPtr);
            }
        } else {
            if (baseVal->getType()->isPointerTy()) {
                thisPtr = baseVal;
                targetClass = exprTy;
            } else {
                llvm::AllocaInst* alloc = createEntryAlloca("base_ptr", baseTy);
                builder->CreateStore(alloc, baseVal);
                thisPtr = alloc;
            }
        }
    }
    if (targetClass.empty()) return (cg_error((*call)->method_name.pos, "cannot resolve target", "QC-S253"), nullptr);
    if (userTypes.count(targetClass) && userTypes.at(targetClass).kind != UserTypeKind::Class || !userTypes.count(targetClass)) {
        std::string funcName = targetClass + "_" + methodName;
        auto funcDefIt = functionDefs.find(baseTypeName(funcName));
        if (funcDefIt != functionDefs.end()) {
            FuncDefNode* funcDef = funcDefIt->second;
            std::vector<llvm::Value*> argValues = {thisPtr};
            auto paramIt = funcDef->params.begin();
            bool hasSpread = false;
            for (auto& argNode : (*call)->args) {
                if (std::holds_alternative<SpreadNode*>(argNode)) { hasSpread = true; }
                std::string ptype = (paramIt != funcDef->params.end()) ? paramIt->type.value : "...";
                llvm::Value* argVal;
                if (ptype.ends_with("&")) {
                    argVal = emitLValue(argNode);
                } else {
                    argVal = emitExpr(argNode);
                }
                if (auto paramTy = llvmTypeFor(resolveTypeName(ptype, false))) {
                    argVal = adaptArgumentForParam(argVal, (*call)->args[std::distance(funcDef->params.begin(), paramIt)], paramTy,
                                                   std::distance(funcDef->params.begin(), paramIt));
                }
                argValues.push_back(argVal);
                if (paramIt != funcDef->params.end()) ++paramIt;
            }
            size_t paramNo = 0;
            for (auto& param : funcDef->params) {
                if (paramNo >= (*call)->args.size()) {
                    if (param.default_value.has_value()) {
                        AnyNode& defaultRef = const_cast<AnyNode&>(param.default_value.value());
                        llvm::Value* defVal = emitExpr(defaultRef);
                        if (!defVal) {
                            cg_error(get_pos(*call), "failed to evaluate default parameter", "QC-S168");
                            return nullptr;
                        }
                        argValues.push_back(defVal);
                    } else {
                        cg_error(get_pos(*call), "missing required argument at position " + std::to_string(paramNo), "QC-S169");
                        return nullptr;
                    }
                }
                paramNo++;
            }
            if (hasSpread) {
                cg_error(get_pos(*call), "spread is no longer allowed in function calls.", "QC-S170");
                return nullptr;
            }
            if (!funcDef->generics.empty()) {
                funcName = fixMangling(funcName);
                if (specializedFunctions.find(funcName) == specializedFunctions.end()) {
                    llvm::Function* specializedFn = generateSpecializedFunction(funcDef, funcName);
                    if (!specializedFn) return nullptr;
                    specializedFunctions[funcName] = specializedFn;
                }
                llvm::Function* fn = specializedFunctions[funcName];
                if (funcDef->params.size() > 0 && funcDef->params.back().type.value == "...") {
                    size_t fixedCount = funcDef->params.size() - 1;
                    std::vector<llvm::Value*> varVals(argValues.begin() + fixedCount, argValues.end());
                    argValues.resize(fixedCount);
                    argValues.push_back(packVariadicArgs(varVals));
                }
                if (insideTry()) {
                    auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                    llvm::InvokeInst* invoke = builder->CreateInvoke(fn, contBB, currentLandingPad(), argValues);
                    builder->SetInsertPoint(contBB);
                    return invoke;
                } else {
                    return builder->CreateCall(fn, argValues);
                }
            }
            llvm::Function* fn = module->getFunction(funcName);
            if (!fn) {
                cg_error((*call)->method_name.pos, "method '" + methodName + "' not found on type '" + targetClass + "'", "QC-T052");
                return nullptr;
            }
            if (insideTry()) {
                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                llvm::Value* invoke = builder->CreateInvoke(fn->getFunctionType(), fn, contBB, currentLandingPad(), argValues);
                builder->SetInsertPoint(contBB);
                return invoke;
            }
            return builder->CreateCall(fn, argValues);
        }
    }
    if (llvm::Value* specializedCall = tryHandleSpecialized(targetClass, methodName, *call, thisPtr)) { return specializedCall; }
    std::vector<std::string> argTypes;
    for (auto& arg : (*call)->args) { argTypes.push_back(getExpressionType(arg)); }
    ClassMethodInfo* info = findMethodInfo(targetClass, methodName, argTypes);
    auto args = prepareArgs(info, (*call)->args);
    llvm::StructType* VariadicStructTy = llvm::StructType::get(context, {builder->getPtrTy(), builder->getInt32Ty(), builder->getInt32Ty()});
    bool isVariadic = (info && !info->params.empty() && info->params.back().type.value == "...");
    if (isVariadic) {
        size_t numFixedParams = info->params.size() - 1;
        std::vector<llvm::Value*> varVals;
        if (args.size() > numFixedParams) {
            varVals.assign(args.begin() + numFixedParams, args.end());
            args.resize(numFixedParams);
        }
        llvm::Value* args_cnt = builder->getInt32(varVals.size());
        llvm::Value* items_array = builder->CreateAlloca(builder->getPtrTy(), args_cnt, "var_array");
        for (size_t i = 0; i < varVals.size(); ++i) {
            llvm::Value* element_ptr = builder->CreateGEP(builder->getPtrTy(), items_array, builder->getInt32(i));
            llvm::Value* val = varVals[i];
            if (val->getType()->isStructTy()) {
                llvm::Value* tempAlloc = builder->CreateAlloca(val->getType(), nullptr, "var_struct_tmp");
                builder->CreateStore(val, tempAlloc);
                val = tempAlloc;
            } else if (val->getType()->isIntegerTy()) {
                val = builder->CreateIntToPtr(val, builder->getPtrTy());
            } else if (val->getType()->isFloatingPointTy()) {
                llvm::Value* asInt = builder->CreateBitCast(val, builder->getIntNTy(getPtrSize()));
                val = builder->CreateIntToPtr(asInt, builder->getPtrTy());
            }
            builder->CreateStore(val, element_ptr);
        }
        llvm::Value* varStructAlloc = builder->CreateAlloca(VariadicStructTy, nullptr, "var_struct_alloc");
        llvm::Value* ptrField = builder->CreateStructGEP(VariadicStructTy, varStructAlloc, 0);
        builder->CreateStore(items_array, ptrField);
        llvm::Value* lenField = builder->CreateStructGEP(VariadicStructTy, varStructAlloc, 1);
        builder->CreateStore(args_cnt, lenField);
        llvm::Value* capField = builder->CreateStructGEP(VariadicStructTy, varStructAlloc, 2);
        builder->CreateStore(builder->getInt32(0), capField);
        args.push_back(varStructAlloc);
    }
    std::string dispatchClass = targetClass;
    targetClass = resolveVirtualTargetClass(targetClass, methodName, (*call)->args.size());
    llvm::Function* method = findMethodOverload(targetClass, methodName, args);
    if (!method) return (cg_error((*call)->method_name.pos, "no overload found", "QC-O003"), nullptr);
    auto vtableIt = vtables.find(targetClass);
    auto slotIt = vtableSlotIndex.find(targetClass);
    if (vtableIt != vtables.end() && slotIt != vtableSlotIndex.end()) {
        std::string mangledName = targetClass + "_" + methodName;
        if (info && classMethods[targetClass][methodName].size() > 1) {
            for (auto& param : info->params) { mangledName += "_" + (param.signature.has_value() ? std::string("fn") : param.type.value); }
        }
        auto indexIt = slotIt->second.find(mangledName);
        if (indexIt != slotIt->second.end()) {
            int slotIndex = indexIt->second;
            llvm::StructType* classTy = genericiseOrFindClass(targetClass);
            llvm::Value* vptrField = builder->CreateStructGEP(classTy, thisPtr, 0, "vptr_field");
            llvm::Value* vptr = builder->CreateLoad(builder->getPtrTy(), vptrField, "vptr");
            llvm::Value* fnPtrAddr = builder->CreateGEP(builder->getPtrTy(), vptr, builder->getInt32(slotIndex), "vtable_slot");
            llvm::Value* fnPtr = builder->CreateLoad(builder->getPtrTy(), fnPtrAddr, "fn_ptr");
            std::vector<llvm::Value*> allArgs = reconcileArgs(method, thisPtr, args);
            if (insideTry()) {
                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                llvm::InvokeInst* invoke = builder->CreateInvoke(method->getFunctionType(), fnPtr, contBB, currentLandingPad(), allArgs);
                builder->SetInsertPoint(contBB);
                return invoke;
            }
            return builder->CreateCall(method->getFunctionType(), fnPtr, allArgs);
        }
    }

    return emitMethodCall(method, thisPtr, args, methodName);
}
llvm::Value* LLVMCompiler::emitFieldAssign(FieldAssignNode* const* fieldAssign) {
    std::string fieldName = (*fieldAssign)->field_name.value;
    std::string targetTypeStr = "";
    if (auto varAccess = std::get_if<VarAccessNode*>(&(*fieldAssign)->base)) {
        if ((*varAccess)->var_name_tok.value == "this" && !currentClassName.empty()) {
            targetTypeStr = getFieldType(baseTypeName(currentClassName), fieldName);
        }
    }
    llvm::Value* valueVal = nullptr;
    if (!valueVal) { valueVal = emitExpr((*fieldAssign)->value); }
    if (!valueVal) return nullptr;
    if (valueVal->getType()->isPointerTy()) {
        if (getExpressionType((*fieldAssign)->value, false).ends_with("&")) {
            valueVal = builder->CreateLoad(llvmTypeFor(getExpressionType((*fieldAssign)->value)), valueVal, "load_ref");
        }
    }
    if (auto varAccess = std::get_if<VarAccessNode*>(&(*fieldAssign)->base)) {
        if ((*varAccess)->var_name_tok.value == "this" && currentThis && !currentClassName.empty()) {
            int fieldIdx = getFlattenedFieldIndex(baseTypeName(currentClassName), fieldName);
            if (fieldIdx == -1) {
                cg_error(get_pos(*varAccess), "field not found: " + fieldName, "QC-S255");
                return nullptr;
            }

            auto [fieldOwnerClass, fieldAccess] = getFieldOwner(currentClassName, fieldName);
            if (!canAccessField(currentClassName, fieldOwnerClass, fieldAccess)) {
                cg_error(get_pos(*varAccess), "cannot access " + fieldAccess + " field", "QC-S256");
                return nullptr;
            }
            llvm::StructType* classTy = genericiseOrFindClass(currentClassName);
            llvm::Value* fieldPtr = builder->CreateStructGEP(classTy, currentThis, fieldIdx);
            builder->CreateStore(valueVal, fieldPtr);
            return builder->getInt32(0);
        }
    }
    PropertyAccessNode tempProp((*fieldAssign)->base, Token(), (*fieldAssign)->field_name);
    llvm::Value* fieldPtr = emitPropertyAddress(tempProp);
    AnyNode tempVariant = new PropertyAccessNode(tempProp);
    std::string fieldTypeStr = getExpressionType(tempVariant, true);
    if (auto propPtr = std::get_if<PropertyAccessNode*>(&tempVariant)) { (*fieldAssign)->base = *(*propPtr)->base; }
    if (!fieldPtr) return nullptr;
    llvm::Type* destTy = llvmTypeFor(fieldTypeStr);
    builder->CreateStore(valueVal, fieldPtr);
    return valueVal;
}
llvm::Value* LLVMCompiler::emitExpr(const AnyNode& node) {
    if (auto num = std::get_if<NumberNode>(&node)) {
        const std::string& text = num->tok.value;

        switch (num->tok.type) {
        case TokenType::INT: {
            intptr_t v = std::stoll(text);
            return builder->getInt32(static_cast<int32_t>(v));
        }
        case TokenType::LONG_INT: {
            intptr_t v = std::stoll(text);
            return getPtrSize() == 32 ? builder->getInt32(static_cast<int32_t>(v)) : builder->getInt64(static_cast<int64_t>(v));
        }
        case TokenType::SHORT_INT: {
            intptr_t v = std::stoi(text);
            return builder->getInt16(static_cast<int16_t>(v));
        }
        case TokenType::ADDR_T: {
            llvm::APInt value(getPtrSize(), text, 10);
            return llvm::ConstantInt::get(builder->getContext(), value);
        }
        case TokenType::BYTE: {
            llvm::APInt value(8, text, 10);
            return llvm::ConstantInt::get(builder->getContext(), value);
        }
        case TokenType::NIBBLE: {
            llvm::APInt value(4, text, 10);
            return llvm::ConstantInt::get(builder->getContext(), value);
        }
        case TokenType::FLOAT: {
            std::string t = text;
            if (!t.empty() && (std::tolower(t.back()) == 'f')) t.pop_back();
            float v = std::stof(t);
            return llvm::ConstantFP::get(builder->getFloatTy(), v);
        }
        case TokenType::DOUBLE: {
            long double v = std::stold(text);
            return llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(v));
        }
        default: int v = std::stoi(text); return builder->getInt32(v);
        }
    } else if (auto chr = std::get_if<CharNode>(&node)) {
        char c = chr->tok.value.empty() ? '\0' : chr->tok.value[0];
        return builder->getInt8((uint8_t)c);
    } else if (auto boolNode = std::get_if<BoolNode>(&node)) {
        bool value = (boolNode->tok.value == "true");
        return builder->getInt1(value ? 1 : 0);
    } else if (auto qbool = std::get_if<QBoolNode>(&node)) {
        uint8_t value;
        if (qbool->tok.value == "none") {
            value = 0b00;
        } else if (qbool->tok.value == "qfalse") {
            value = 0b01;
        } else if (qbool->tok.value == "qtrue") {
            value = 0b10;
        } else if (qbool->tok.value == "both") {
            value = 0b11;
        } else {
            cg_error(qbool->tok.pos, "invalid qbool value '" + qbool->tok.value + "'", "QC-S116");
            return nullptr;
        }
        return llvm::ConstantInt::get(builder->getIntNTy(2), value);
    } else if (auto str = std::get_if<StringNode>(&node)) {
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(context, str->tok.value, true);
        llvm::GlobalVariable* globalStr = new llvm::GlobalVariable(*module, strConstant->getType(), true, llvm::GlobalValue::PrivateLinkage,
                                                                   strConstant, ".str");
        std::vector<llvm::Constant*> indices = {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)};
        return llvm::ConstantExpr::getInBoundsGetElementPtr(strConstant->getType(), globalStr, indices);
    } else if (auto tv = std::get_if<TypeValueNode>(&node)) {
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(context, tv->tok.value, true);
        llvm::GlobalVariable* globalStr = new llvm::GlobalVariable(*module, strConstant->getType(), true, llvm::GlobalValue::PrivateLinkage,
                                                                   strConstant, ".str");
        std::vector<llvm::Constant*> indices = {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)};
        return llvm::ConstantExpr::getInBoundsGetElementPtr(strConstant->getType(), globalStr, indices);
    } else if (auto bin = std::get_if<BinOpNode*>(&node)) {
        return emitBinOp(bin);
    } else if (auto va = std::get_if<VarAssignNode*>(&node)) {
        return emitVarAssign(va);
    } else if (auto acc = std::get_if<VarAccessNode*>(&node)) {
        return emitVarAccess(acc);
    } else if (auto asn = std::get_if<AssignExprNode*>(&node)) {
        return emitAssignExpr(asn);
    } else if (auto unary = std::get_if<UnaryOpNode*>(&node)) {
        return emitUnaryOp(unary);
    } else if (auto fnPtr = std::get_if<FuncDefNode*>(&node)) {
        llvm::Function* f = emitFuncDef(*(*fnPtr));
        return f;
    } else if (auto mapLit = std::get_if<MapLiteralNode*>(&node)) {
        return emitMapLit(mapLit);
    } else if (auto arrLit = std::get_if<ArrayLiteralNode*>(&node)) {
        return emitArrLit(arrLit);
    } else if (auto callPtr = std::get_if<CallNode*>(&node)) {
        return emitCall(callPtr);
    } else if (auto arrAcc = safe_get<ArrayAccessNode>(node)) {
        return emitArrAcc(arrAcc);
    } else if (auto propAccess = std::get_if<PropertyAccessNode*>(&node)) {
        return emitPropAcc(propAccess);
    } else if (auto methodCall = std::get_if<MethodCallNode*>(&node)) {
        return emitMthdCall(methodCall);
    } else if (auto spread = std::get_if<SpreadNode*>(&node)) {
        cg_error(get_pos(*spread),
                 "spread operator can only be used in array "
                 "literals",
                 "QC-S254");
        return nullptr;
    } else if (auto fieldAssign = std::get_if<FieldAssignNode*>(&node)) {
        return emitFieldAssign(fieldAssign);
    } else if (auto ref = safe_get<RefVarDeclNode>(node)) {
        std::string fullName = (getCurrentNamespace().empty() ? "" : getCurrentNamespace() + "::") + ref->var_name_tok.value;
        std::string baseType = ref->type_tok.value;
        if (baseType.starts_with("volatile ")) { baseType = baseType.substr(9); }
        if (!baseType.ends_with("&")) { baseType += "&"; }
        varTypes[fullName] = baseType;
        volatileVars[fullName] = ref->type_tok.value.starts_with("volatile ");
        if (llvm::Value* value = emitLValue(ref->target)) {
            llvm::AllocaInst* refStore = builder->CreateAlloca(builder->getPtrTy(), nullptr, fullName);
            builder->CreateStore(value, refStore);
            locals[fullName] = refStore;
        } else {
            cg_error(get_pos(ref), "Failed to emit RHS for reference decl", "QC-S257");
            return nullptr;
        }
        return nullptr;
    } else if (auto ref = std::get_if<NullptrNode>(&node)) {
        return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
    } else if (auto mn = std::get_if<ModifierNode*>(&node)) {
        return emitModifierNode(*mn);
    } else if (TupleValueNode* tuple = safe_get<TupleValueNode>(node)) {
        std::vector<llvm::Type*> llvmTypes;
        std::vector<llvm::Value*> values;
        for (AnyNode& member : tuple->members) {
            values.push_back(emitExpr(member));
            llvmTypes.push_back(values.back()->getType());
        }
        llvm::Value* instance = llvm::UndefValue::get(llvm::StructType::get(context, llvmTypes));
        for (size_t i = 0; i < values.size(); i++) { instance = builder->CreateInsertValue(instance, values[i], i); }
        return instance;
    }
    return nullptr;
}
llvm::Value* LLVMCompiler::storeAndGetPointer(llvm::Value* val) {
    llvm::Type* ty = val->getType();

    if (ty->isPointerTy()) { return builder->CreateBitCast(val, llvm::PointerType::get(context, 0)); }

    llvm::Function* mallocFn = module->getFunction("qc_malloc");
    if (!mallocFn) {
        llvm::FunctionType* mallocTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {builder->getIntNTy(getPtrSize())}, false);
        mallocFn = llvm::Function::Create(mallocTy, llvm::Function::InternalLinkage, "qc_malloc", module);
    }

    const llvm::DataLayout& DL = module->getDataLayout();
    uint64_t size = DL.getTypeAllocSize(ty);

    llvm::Value* heapPtr = builder->CreateCall(mallocFn, {builder->getInt64(size)}, "union_heap");
    llvm::Value* typedPtr = builder->CreateBitCast(heapPtr, llvm::PointerType::get(context, 0));
    builder->CreateStore(val, typedPtr);

    return heapPtr;
}
int LLVMCompiler::findUnionVariantTag(const std::string& unionName, AnyNode& valueNode, llvm::Value* val) {
    auto typeIt = userTypes.find(unionName);
    if (typeIt == userTypes.end()) return -1;

    auto& members = typeIt->second.members;

    for (size_t i = 0; i < members.size(); i++) {
        auto& member = members[i];

        const std::string& m = member.type;
        size_t colonPos = m.find(':');

        if (colonPos != std::string::npos) {
            std::string kind = m.substr(0, colonPos);
            std::string valueStr = m.substr(colonPos + 1);

            if (kind == "int") {
                if (auto numNode = std::get_if<NumberNode>(&valueNode)) {
                    if (numNode->tok.value == valueStr) { return i; }
                }
            } else if (kind == "string") {
                if (auto strNode = std::get_if<StringNode>(&valueNode)) {
                    std::string lit = "\"" + strNode->tok.value + "\"";
                    if (lit == valueStr) { return i; }
                }
            } else if (kind == "char") {
                if (auto charNode = std::get_if<CharNode>(&valueNode)) {
                    std::string lit = "'" + charNode->tok.value + "'";
                    if (lit == valueStr) { return i; }
                }
            } else if (kind == "bool") {
                if (auto boolNode = std::get_if<BoolNode>(&valueNode)) {
                    if (boolNode->tok.value == valueStr) { return i; }
                }
            } else if (kind == "qbool") {
                if (auto qBoolNode = std::get_if<QBoolNode>(&valueNode)) {
                    if (qBoolNode->tok.value == valueStr) { return i; }
                }
            }
        } else {
            llvm::Type* valTy = val->getType();
            llvm::Type* memberTy = llvmTypeFor(m);

            if (valTy == memberTy) { return i; }
        }
    }

    return -1;
}
llvm::Value* LLVMCompiler::callStringConcat(llvm::Value* a, llvm::Value* b) {
    llvm::Function* concatFn = module->getFunction("qc_string_concat");
    if (!concatFn) {
        llvm::FunctionType* ty = llvm::FunctionType::get(llvm::PointerType::get(context, 0),
                                                         {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)}, false);
        concatFn = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, "qc_string_concat", module);
    }
    return builder->CreateCall(concatFn, {a, b});
}
void LLVMCompiler::generateStructReprFunctions() {
    llvm::BasicBlock* savedBB = builder->GetInsertBlock();

    for (auto& [name, info] : userTypes) {
        if (info.kind != UserTypeKind::Struct) continue;
        if (!info.generics.empty()) continue;
        llvm::StructType* structTy = structTypes[name];
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

            if (field.type == "int") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_int");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (field.type == "float") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_float");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (field.type == "double") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_double");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (field.type == "bool") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_bool");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (field.type == "char") {
                llvm::Function* toStrFn = module->getFunction("qc_to_string_char");
                fieldStr = builder->CreateCall(toStrFn, {fieldVal});
            } else if (field.type == "string") {
                fieldStr = fieldVal;
            } else if (structTypes.find(field.type) != structTypes.end()) {
                llvm::Function* nestedReprFn = module->getFunction(field.type + "_repr");
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
    }
    if (savedBB) { builder->SetInsertPoint(savedBB); }
}
llvm::Value* LLVMCompiler::convertToString(llvm::Value* val, AnyNode& expr, Position pos) {
    val = derefIfReference(val, expr);
    if (!val) { return nullptr; }
    llvm::Type* ty = val->getType();
    if (ty->isPointerTy()) {
        if (auto varAccess = std::get_if<VarAccessNode*>(&expr)) {
            std::string varName = (*varAccess)->var_name_tok.value;
            if (varName == "this") {
                if (currentThis) {
                    return currentThis;
                } else {
                    cg_error((*varAccess)->var_name_tok.pos, "'this' used outside of class method", "QC-S258");
                    return nullptr;
                }
            }
        }
        return val;
    }
    std::string fnName;
    if (ty->isIntegerTy(32))
        fnName = "qc_to_string_int";
    else if (ty->isIntegerTy(64))
        fnName = "qc_to_string_long_int";
    else if (ty->isIntegerTy(16))
        fnName = "qc_to_string_short_int";
    else if (ty->isFloatTy())
        fnName = "qc_to_string_float";
    else if (ty->isDoubleTy())
        fnName = "qc_to_string_double";
    else if (ty->isIntegerTy(1))
        fnName = "qc_to_string_bool";
    else if (ty->isIntegerTy(2))
        fnName = "qc_to_string_qbool";
    else if (ty->isIntegerTy(4))
        fnName = "qc_to_string_nibble";
    else if (ty->isIntegerTy(8))
        fnName = "qc_to_string_char";
    else {
        cg_error(pos, "cannot convert type to string", "QC-T053");
        return nullptr;
    }
    if (!fnName.empty()) {
        llvm::Function* fn = module->getFunction(fnName);
        if (!fn) {
            llvm::FunctionType* fty = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {val->getType()}, false);
            fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fnName, module);
        }
        return builder->CreateCall(fn, {val}, "to_str");
    }
    if (auto structTy = llvm::dyn_cast<llvm::StructType>(ty)) {
        if (structTy->hasName()) {
            std::string className = structTy->getName().str();
            if (classTypes.find(className) != classTypes.end()) {
                auto [reprMethod, ownerClass] = findMethodInHierarchy(className, "_repr");
                if (reprMethod) {
                    std::vector<llvm::Value*> args;
                    llvm::AllocaInst* temp = createEntryAlloca("temp_repr", ty);
                    builder->CreateStore(val, temp);
                    args.push_back(temp);
                    if (insideTry()) {
                        auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                        llvm::InvokeInst* invk = builder->CreateInvoke(reprMethod, contBB, currentLandingPad(), args);
                        builder->SetInsertPoint(contBB);
                        return invk;
                    }
                    return builder->CreateCall(reprMethod, args, "repr_result");
                }
            }
        }
    }
    std::string unionName;
    if (isUnionType(ty, &unionName)) {
        auto members = genericiseOrFindUnion(unionName).members;
        llvm::Value* tag = builder->CreateExtractValue(val, 0, "union_tag");
        llvm::Value* payload = builder->CreateExtractValue(val, 1, "union_payload");
        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "fstr_union_end", currentFunction);
        llvm::AllocaInst* resultAlloc = createEntryAlloca("fstr_union_result", llvm::PointerType::get(context, 0));
        llvm::SwitchInst* sw = builder->CreateSwitch(tag, endBB, members.size());
        for (size_t i = 0; i < members.size(); i++) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "fstr_union_case_" + std::to_string(i), currentFunction);
            sw->addCase(builder->getInt32(i), caseBB);
            builder->SetInsertPoint(caseBB);
            std::string ts = members[i].type;
            size_t c = ts.find(':');
            if (c != std::string::npos) { ts = ts.substr(0, c); }
            llvm::Type* memberTy = llvmTypeFor(ts);
            if (!memberTy) {
                cg_error(pos, "cannot determine union member type: " + ts, "QC-T053");
                llvm::Value* fallback = builder->CreateGlobalString("?");
                builder->CreateStore(fallback, resultAlloc);
                builder->CreateBr(endBB);
                continue;
            }
            llvm::Value* memberVal;
            if (memberTy->isPointerTy()) {
                memberVal = builder->CreateBitCast(payload, memberTy);
            } else {
                llvm::Value* typedPtr = builder->CreateBitCast(payload, llvm::PointerType::get(context, 0));
                memberVal = builder->CreateLoad(memberTy, typedPtr, "union_member");
            }
            AnyNode fakeNode = std::monostate{};
            llvm::Value* strVal = convertToString(memberVal, fakeNode, pos);
            if (!strVal) { strVal = builder->CreateGlobalString("?"); }
            builder->CreateStore(strVal, resultAlloc);
            builder->CreateBr(endBB);
        }
        builder->SetInsertPoint(endBB);
        return builder->CreateLoad(llvm::PointerType::get(context, 0), resultAlloc, "fstr_union_result");
    }
    cg_error(pos, "f-string: unsupported type in compiled mode", "QC-T013");
    return nullptr;
}
void LLVMCompiler::expandSpreadIntoVector(llvm::Value* collVal, AnyNode& collExpr, std::vector<llvm::Value*>& elements) {
    llvm::Value* lengthVal = getCollectionLength(collVal, collExpr);
    if (!lengthVal) return;

    if (auto* constLen = llvm::dyn_cast<llvm::ConstantInt>(lengthVal)) {
        int length = constLen->getSExtValue();
        int elemTypeCode = 0;

        if (auto varAccess = std::get_if<VarAccessNode*>(&collExpr)) {
            std::string collName = (*varAccess)->var_name_tok.value;
            if (hasArrayType(collName)) {
                auto typeIt = findArrayType(collName);
                elemTypeCode = getTypeCode(typeIt->second);
            }
        }

        for (int i = 0; i < length; i++) {
            llvm::Value* elemVal;
            llvm::Type* elemTy = getTypeFromCode(elemTypeCode);
            llvm::Value* gepPtr = builder->CreateGEP(elemTy, collVal, builder->getInt32(i));
            elemVal = builder->CreateLoad(elemTy, gepPtr);
            elements.push_back(elemVal);
        }
    } else {
        cg_error(get_pos(collExpr), "cannot spread runtime-sized collection into array literal", "QC-S259");
    }
}
llvm::Value* LLVMCompiler::expandSpreadIntoArrays(llvm::Value* collVal, AnyNode& collExpr, llvm::AllocaInst* argsArray, llvm::AllocaInst* typesArray,
                                                  llvm::Value* startIndex) {
    llvm::Value* lengthVal = getCollectionLength(collVal, collExpr);
    if (!lengthVal) return startIndex;

    int elemTypeCode = 0;

    if (auto varAccess = std::get_if<VarAccessNode*>(&collExpr)) {
        std::string collName = (*varAccess)->var_name_tok.value;
        if (hasArrayType(collName)) {
            auto typeIt = findArrayType(collName);
            elemTypeCode = getTypeCode(typeIt->second);
        }
    }

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(context, "expand_loop", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "expand_body", currentFunction);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "expand_end", currentFunction);

    llvm::AllocaInst* iAlloc = createEntryAlloca("expand_i", builder->getInt32Ty());
    llvm::AllocaInst* currentIdxAlloc = createEntryAlloca("expand_idx", builder->getInt32Ty());

    builder->CreateStore(builder->getInt32(0), iAlloc);
    builder->CreateStore(startIndex, currentIdxAlloc);
    builder->CreateBr(loopBB);

    builder->SetInsertPoint(loopBB);
    llvm::Value* iVal = builder->CreateLoad(builder->getInt32Ty(), iAlloc);
    llvm::Value* cond = builder->CreateICmpSLT(iVal, lengthVal);
    builder->CreateCondBr(cond, bodyBB, endBB);

    builder->SetInsertPoint(bodyBB);
    llvm::Value* currentIdx = builder->CreateLoad(builder->getInt32Ty(), currentIdxAlloc);

    llvm::Value* elemPtr;

    llvm::Type* elemTy = getTypeFromCode(elemTypeCode);
    llvm::Value* arrayPtr = collVal;
    if (auto varAccess = std::get_if<VarAccessNode*>(&collExpr)) {
        std::string collName = (*varAccess)->var_name_tok.value;
        llvm::Value* locAlloc = getVarAddress(collName);
        if (locAlloc) {
            llvm::Type* allocTy = getPointeeType(collName);
            if (allocTy->isPointerTy()) { arrayPtr = builder->CreateLoad(allocTy, locAlloc, "arr_ptr"); }
        }
    }

    llvm::Value* gepPtr = builder->CreateGEP(elemTy, arrayPtr, iVal, "arr_elem_ptr");
    elemPtr = builder->CreateBitCast(gepPtr, llvm::PointerType::get(context, 0));
    llvm::Value* argSlot = builder->CreateGEP(llvm::PointerType::get(context, 0), argsArray, currentIdx);
    builder->CreateStore(elemPtr, argSlot);

    llvm::Value* typeSlot = builder->CreateGEP(builder->getInt32Ty(), typesArray, currentIdx);
    builder->CreateStore(builder->getInt32(elemTypeCode), typeSlot);

    llvm::Value* nextI = builder->CreateAdd(iVal, builder->getInt32(1));
    builder->CreateStore(nextI, iAlloc);
    llvm::Value* nextIdx = builder->CreateAdd(currentIdx, builder->getInt32(1));
    builder->CreateStore(nextIdx, currentIdxAlloc);
    builder->CreateBr(loopBB);

    builder->SetInsertPoint(endBB);
    llvm::Value* finalIdx = builder->CreateLoad(builder->getInt32Ty(), currentIdxAlloc);
    return finalIdx;
}
llvm::Value* LLVMCompiler::getCollectionLength(llvm::Value* collVal, AnyNode& collExpr) {
    if (auto varAccess = std::get_if<VarAccessNode*>(&collExpr)) {
        std::string collName = (*varAccess)->var_name_tok.value;
        llvm::Value* locAlloc = getVarAddress(collName);
        if (locAlloc) {
            llvm::Type* allocTy = getPointeeType(collName);

            if (allocTy && allocTy->isArrayTy()) { return builder->getInt32(allocTy->getArrayNumElements()); }
        }
    }

    if (auto arrLit = std::get_if<ArrayLiteralNode*>(&collExpr)) { return builder->getInt32((*arrLit)->elements.size()); }
    cg_error(get_pos(collExpr), "cannot determine collection length for spread", "QC-S260");
    return nullptr;
}
llvm::Value* LLVMCompiler::copySpreadToArray(llvm::Value* collVal, AnyNode& collExpr, llvm::Value* destArray, llvm::Value* startIndex,
                                             llvm::Type* elemTy, int elemTypeCode) {
    llvm::Value* lengthVal = getCollectionLength(collVal, collExpr);
    if (!lengthVal) return startIndex;
    if (auto varAccess = std::get_if<VarAccessNode*>(&collExpr)) {
        std::string rawName = (*varAccess)->var_name_tok.value;
        std::string resolvedName = resolveMetadataName(rawName);
    }

    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(context, "copy_loop", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "copy_body", currentFunction);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "copy_end", currentFunction);

    llvm::AllocaInst* iAlloc = createEntryAlloca("copy_i", builder->getInt32Ty());
    llvm::AllocaInst* destIdxAlloc = createEntryAlloca("copy_dest_idx", builder->getInt32Ty());
    builder->CreateStore(builder->getInt32(0), iAlloc);
    builder->CreateStore(startIndex, destIdxAlloc);
    builder->CreateBr(loopBB);
    builder->SetInsertPoint(loopBB);
    llvm::Value* iVal = builder->CreateLoad(builder->getInt32Ty(), iAlloc);
    llvm::Value* cond = builder->CreateICmpSLT(iVal, lengthVal);
    builder->CreateCondBr(cond, bodyBB, endBB);
    builder->SetInsertPoint(bodyBB);
    llvm::Value* destIdx = builder->CreateLoad(builder->getInt32Ty(), destIdxAlloc);
    llvm::Value* elemVal;
    llvm::Value* srcPtr = builder->CreateGEP(elemTy, collVal, iVal);
    elemVal = builder->CreateLoad(elemTy, srcPtr);
    llvm::Value* destPtr = builder->CreateGEP(elemTy, destArray, destIdx);
    builder->CreateStore(elemVal, destPtr);

    llvm::Value* nextI = builder->CreateAdd(iVal, builder->getInt32(1));
    builder->CreateStore(nextI, iAlloc);

    llvm::Value* nextDestIdx = builder->CreateAdd(destIdx, builder->getInt32(1));
    builder->CreateStore(nextDestIdx, destIdxAlloc);

    builder->CreateBr(loopBB);
    builder->SetInsertPoint(endBB);
    llvm::Value* finalDestIdx = builder->CreateLoad(builder->getInt32Ty(), destIdxAlloc);
    return finalDestIdx;
}
llvm::Value* LLVMCompiler::createRuntimeSizedArray(std::vector<AnyNode>& elements, llvm::Value* totalSize) {
    llvm::Type* elemTy = nullptr;
    int elemTypeCode = 0;
    for (auto& elem : elements) {
        if (auto spread = std::get_if<SpreadNode*>(&elem)) {
            if (auto varAccess = std::get_if<VarAccessNode*>(&(*spread)->expr)) {
                std::string collName = resolveTypeName((*varAccess)->var_name_tok.value);
                if (hasArrayType(collName)) {
                    auto typeIt = findArrayType(collName);
                    elemTypeCode = getTypeCode(typeIt->second);
                    elemTy = getTypeFromCode(elemTypeCode);
                    break;
                }
            }
        } else {
            llvm::Value* v = emitExpr(elem);
            if (v) {
                elemTy = v->getType();
                elemTypeCode = getTypeCodeFromLLVM(elemTy);
                break;
            }
        }
    }

    if (!elemTy) {
        cg_error(Position(), "cannot determine element type for runtime array", "QC-T054");
        return nullptr;
    }
    llvm::Function* mallocFn = module->getFunction("qc_malloc");
    if (!mallocFn) {
        llvm::FunctionType* mallocTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {builder->getIntNTy(getPtrSize())}, false);
        mallocFn = llvm::Function::Create(mallocTy, llvm::Function::InternalLinkage, "qc_malloc", module);
    }

    const llvm::DataLayout& DL = module->getDataLayout();
    uint64_t elemSize = DL.getTypeAllocSize(elemTy);
    llvm::Value* totalSizeExt = builder->CreateZExt(totalSize, builder->getIntNTy(getPtrSize()));
    llvm::Value* sizeBytes = builder->CreateMul(totalSizeExt, builder->getInt64(elemSize));

    llvm::Value* mallocCall = builder->CreateCall(mallocFn, {sizeBytes}, "runtime_arr");
    llvm::Value* arrPtr = builder->CreateBitCast(mallocCall, llvm::PointerType::get(context, 0));

    llvm::Value* currentIndex = builder->getInt32(0);

    for (auto& elem : elements) {
        if (auto spread = std::get_if<SpreadNode*>(&elem)) {
            llvm::Value* collVal = emitExpr((*spread)->expr);
            currentIndex = copySpreadToArray(collVal, (*spread)->expr, arrPtr, currentIndex, elemTy, elemTypeCode);
        } else {
            llvm::Value* elemVal = emitExpr(elem);
            if (!elemVal) continue;

            llvm::Value* elemPtr = builder->CreateGEP(elemTy, arrPtr, currentIndex);
            builder->CreateStore(elemVal, elemPtr);

            currentIndex = builder->CreateAdd(currentIndex, builder->getInt32(1));
        }
    }
    return arrPtr;
}
llvm::AllocaInst* LLVMCompiler::createEntryAlloca(const std::string& name, llvm::Type* ty) {
    if (!currentFunction) {
        llvm::Constant* initVal = llvm::Constant::getNullValue(ty);

        llvm::GlobalVariable* gv = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage, initVal, name);
        return reinterpret_cast<llvm::AllocaInst*>(gv);
    }
    llvm::IRBuilder<> tmp(&currentFunction->getEntryBlock(), currentFunction->getEntryBlock().begin());
    llvm::AllocaInst* alloc = tmp.CreateAlloca(ty, nullptr, name);
    return alloc;
}
llvm::Function* LLVMCompiler::emitFuncDef(const FuncDefNode& fn) {
    std::string name;
    if (fn.name_tok) {
        name = fn.name_tok->value;
        if (!namespaceStack.empty()) { name = getCurrentNamespace() + "::" + name; }
    } else {
        name = lambdaName();
    }
    if (!fn.modifiers.empty() && !fn.is_foreign && !fn.is_header) {
        FuncDefNode implFn = fn;
        implFn.modifiers.clear();
        std::string implName = "_impl_" + name;
        Token implNameTok = *fn.name_tok;
        implNameTok.value = implName;
        implFn.name_tok = implNameTok;

        llvm::Function* currentTarget = emitFuncDef(implFn);
        if (!currentTarget) return nullptr;
        currentTarget->setLinkage(llvm::Function::InternalLinkage);
        llvm::FunctionType* fTy = llvmFuncTypeFor(fn.return_types, fn.params);
        for (int i = (int)fn.modifiers.size() - 1; i >= 0; --i) {
            Token modTok = fn.modifiers[i];
            if (!modifiers.count(modTok.value)) {
                cg_error(modTok.pos, "unknown modifier '" + modTok.value + "'", "QC-S112");
                return nullptr;
            }
            ModifierInfo& modInfo = modifiers[modTok.value];

            bool isOutermost = (i == 0);
            std::string layerName = isOutermost ? name : ("_mod_" + std::to_string(i) + "_" + name);
            auto linkage = isOutermost ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
            std::string proceedName = "_proceed_" + std::to_string(i) + "_" + name;
            llvm::Function* proceedFunc = synthesizeProceed(proceedName, currentTarget, modInfo.onReturn, fn);
            llvm::Function* layerFunc = module->getFunction(layerName);
            if (!layerFunc) { layerFunc = llvm::Function::Create(fTy, linkage, layerName, module); }
            auto savedDefers = defersStack;
            defersStack.clear();
            enterScope();
            llvm::BasicBlock* savedInsertBlock = builder->GetInsertBlock();
            auto* entryBB = llvm::BasicBlock::Create(context, "entry", layerFunc);
            builder->SetInsertPoint(entryBB);
            auto* oldFunction = currentFunction;
            currentFunction = layerFunc;
            std::vector<llvm::Value*> forwardArgs;
            unsigned idx = 0;
            for (auto& arg : layerFunc->args()) {
                auto& param = *std::next(fn.params.begin(), idx);
                arg.setName(param.name.value);
                auto* alloca = createEntryAlloca(arg.getName().str(), arg.getType());
                builder->CreateStore(&arg, alloca);
                locals[param.name.value] = alloca;
                varTypes[param.name.value] = param.type.value;
                forwardArgs.push_back(&arg);
                idx++;
            }
            functions["proceed"] = proceedFunc;
            if (modInfo.onCall) {
                for (auto& stmt : modInfo.onCall->statements) { emitStmt(stmt); }
            } else {
                if (fTy->getReturnType()->isVoidTy()) {
                    builder->CreateCall(proceedFunc, forwardArgs);
                    builder->CreateRetVoid();
                } else {
                    llvm::Value* retVal = builder->CreateCall(proceedFunc, forwardArgs);
                    builder->CreateRet(retVal);
                }
            }
            if (!builder->GetInsertBlock()->getTerminator()) {
                if (fTy->getReturnType()->isVoidTy()) {
                    builder->CreateRetVoid();
                } else {
                    warn("missing-return", implNameTok.pos, "control reaches end of non-void function without return", "QC-W017");
                    builder->CreateRet(llvm::ConstantAggregateZero::get(fTy->getReturnType()));
                }
            }
            if (savedInsertBlock) { builder->SetInsertPoint(savedInsertBlock); }
            currentFunction = oldFunction;
            functions.erase("proceed");
            exitScope();
            defersStack = savedDefers;
            currentTarget = layerFunc;
        }
        functions[name] = currentTarget;
        return currentTarget;
    }
    llvm::FunctionType* fTy = llvmFuncTypeFor(fn.return_types, fn.params);
    llvm::GlobalValue::LinkageTypes linkage = llvm::Function::ExternalLinkage;
    auto* func = module->getFunction(name);
    functionSignatures[name] = {fTy, {}};
    if (func) {
        if (func->getFunctionType() != fTy) { warn("fn-conflict", fn.getPos(), "conflicting declaration for function " + name, "W004"); }
        if (fn.is_foreign || fn.is_header) return func;
        if (!func->empty()) { warn("fn-redecl", fn.getPos(), "redefinition of function " + name, "W005"); }
        func->setLinkage(linkage);
    } else {
        func = llvm::Function::Create(fTy, linkage, name, module);
    }
    llvm::SmallVector<llvm::Metadata*, 4> retTypes;
    for (auto& ret : fn.return_types) { retTypes.push_back(llvm::MDString::get(context, ret.value)); }
    func->setMetadata("qc.return_types", llvm::MDNode::get(context, retTypes));
    if (fn.is_volatile) {
        func->addFnAttr(llvm::Attribute::NoInline);
        func->addFnAttr(llvm::Attribute::OptimizeNone);
        func->addFnAttr("noipa");
    }
    for (int i = 0; i < fTy->getNumParams(); i++) {
        auto it = fn.params.begin();
        std::advance(it, i);
        if (it->type.value.starts_with("out ")) {
            func->addParamAttr(i, llvm::Attribute::WriteOnly);
            func->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
        } else if (it->type.value.starts_with("inout ")) {
            func->addParamAttr(i, llvm::Attribute::getWithCaptureInfo(context, llvm::CaptureInfo::none()));
        }
        if (it->type.value.ends_with("restrict")) { func->addParamAttr(i, llvm::Attribute::NoAlias); }
    }
    if (fn.is_foreign || fn.is_header) return func;
    auto savedDefers = defersStack;
    defersStack.clear();
    enterScope();
    auto savedLambdaTypes = lambdaTypes;
    llvm::BasicBlock* savedInsertBlock = builder->GetInsertBlock();
    auto* entryBB = llvm::BasicBlock::Create(context, "entry", func);
    builder->SetInsertPoint(entryBB);
    auto* oldFunction = currentFunction;
    currentFunction = func;
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        auto& param = *std::next(fn.params.begin(), idx);
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
                if (dims > 0 && name != entrypointName) {
                    std::string decayed_type = t;
                    size_t pos = 0;
                    while ((pos = decayed_type.find("[]", pos)) != std::string::npos) {
                        decayed_type.replace(pos, 2, "*");
                        ++pos;
                    }
                    warn("array-param-decay", param.type.pos,
                         "Using type " + t + " as parameter to function, which will degrade to " + decayed_type +
                             ". Please consider changing the type of this parameter to that type "
                             "instead, and if you need the length property (which won't exist on "
                             "pointers), add an additional length parameter.",
                         "W002");
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

    for (auto& stmt : fn.body->statements) { emitStmt(stmt); }

    if (!builder->GetInsertBlock()->getTerminator()) {
        if (fn.is_multi_return()) {
            warn("missing-return", fn.getPos(), "control reaches end of non-void function without return", "QC-W017");
            llvm::Type* retTy = fTy->getReturnType();
            builder->CreateRet(llvm::ConstantAggregateZero::get(retTy));
        } else {
            llvm::Type* retTy = fTy->getReturnType();
            if (retTy->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                warn("missing-return", fn.getPos(), "control reaches end of non-void function without return", "QC-W017");
                if (retTy->isIntegerTy()) {
                    builder->CreateRet(llvm::ConstantInt::get(retTy, 0));
                } else if (retTy->isFloatingPointTy()) {
                    builder->CreateRet(llvm::ConstantFP::get(retTy, 0.0));
                } else {
                    builder->CreateRet(llvm::ConstantAggregateZero::get(retTy));
                }
            }
        }
    }

    if (savedInsertBlock) { builder->SetInsertPoint(savedInsertBlock); }

    currentFunction = oldFunction;
    functions[name] = func;
    lambdaTypes = savedLambdaTypes;
    exitScope();
    defersStack = savedDefers;
    return func;
}
std::string LLVMCompiler::mangleName(const FuncDefNode& fn) {
    std::string base = fn.name_tok ? fn.name_tok->value : "lambda";
    if (!fn.namespace_path.empty()) { base = fn.namespace_path + "::" + base; }
    return base;
}
std::string LLVMCompiler::lambdaName() {
    static int counter = 0;
    return "__lambda_" + std::to_string(counter++);
}
void LLVMCompiler::emitMultiRet(MultiReturnNode* mret) {
    llvm::Type* retTy = currentFunction->getReturnType();
    if (!retTy->isStructTy()) {
        cg_error(mret->pos, "multi-return in non-multi-return function", "QC-S262");
        return;
    }
    emitDefersDownTo(0);
    llvm::Value* agg = llvm::ConstantAggregateZero::get(retTy);
    llvm::StructType* retStructTy = llvm::cast<llvm::StructType>(retTy);
    for (size_t i = 0; i < mret->values.size(); ++i) {
        llvm::Value* val = nullptr;
        if (auto varAccess = std::get_if<VarAccessNode*>(&mret->values[i])) {
            std::string name = (*varAccess)->var_name_tok.value;
            llvm::Value* alloc = getVarAddress(name);
            if (alloc) {
                llvm::Type* allocatedTy = getPointeeType(name);
                if (allocatedTy->isArrayTy()) { val = builder->CreateBitCast(alloc, llvm::PointerType::get(context, 0), "array_ret_ptr"); }
            }
        }
        if (auto call = std::get_if<CallNode*>(&mret->values[i])) {
            if (auto varAccess = std::get_if<VarAccessNode*>(&(*call)->node_to_call)) {
                std::string funcName = (*varAccess)->var_name_tok.value;

                if (classTypes.find(funcName) != classTypes.end()) {
                    llvm::StructType* classTy = genericiseOrFindClass(funcName);

                    std::vector<llvm::Value*> ctorArgs;
                    for (auto& argNode : (*call)->arg_nodes) {
                        llvm::Value* arg = emitExpr(argNode);
                        if (!arg) return;
                        ctorArgs.push_back(arg);
                    }

                    llvm::Function* ctor = findMethodOverload(funcName, funcName, ctorArgs);

                    if (ctor) {
                        llvm::AllocaInst* retVal = createEntryAlloca("mret_val_" + std::to_string(i), classTy);

                        std::vector<llvm::Value*> allArgs = {retVal};
                        allArgs.insert(allArgs.end(), ctorArgs.begin(), ctorArgs.end());
                        if (insideTry()) {
                            auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                            llvm::InvokeInst* invoke = builder->CreateInvoke(ctor, contBB, currentLandingPad(), allArgs);
                            builder->SetInsertPoint(contBB);
                        } else {
                            builder->CreateCall(ctor, allArgs);
                        }
                        val = builder->CreateLoad(classTy, retVal);
                    }
                }
            }
        }
        if (auto arrayLit = std::get_if<ArrayLiteralNode*>(&mret->values[i])) {
            val = emitExpr(mret->values[i]);
            if (!val) return;

            llvm::Type* srcTy = val->getType();
            llvm::Type* destTy = retStructTy->getElementType(i);
            if (destTy->isPointerTy() && srcTy->isArrayTy()) {
                llvm::ArrayType* arrayType = llvm::cast<llvm::ArrayType>(srcTy);
                llvm::Type* iTy = builder->getIntNTy(getPtrSize());
                llvm::Value* size = llvm::ConstantInt::get(iTy, module->getDataLayout().getTypeAllocSize(arrayType));

                llvm::Function* mallocFn = module->getFunction("qc_malloc");
                if (!mallocFn) {
                    llvm::FunctionType* mallocTy = llvm::FunctionType::get(builder->getPtrTy(), {iTy}, false);
                    mallocFn = llvm::Function::Create(mallocTy, llvm::Function::InternalLinkage, "qc_malloc", module);
                }

                llvm::Value* heapPtr = builder->CreateCall(mallocFn, {size});
                llvm::Value* typedPtr = builder->CreateBitCast(heapPtr, llvm::PointerType::get(context, 0));

                builder->CreateStore(val, typedPtr);

                val = builder->CreateBitCast(typedPtr, destTy);
            }
        }
        if (this->returnsRef(i)) { val = this->emitLValue(mret->values[i]); }
        if (!val) { val = emitExpr(mret->values[i]); }
        if (!val) return;
        llvm::Type* srcTy = val->getType();
        llvm::Type* destTy = retStructTy->getElementType(i);
        if (isUnionType(srcTy) && !isUnionType(destTy)) {
            llvm::Value* dataPtr = builder->CreateExtractValue(val, 1, "union_data");
            if (destTy->isPointerTy()) {
                val = builder->CreateBitCast(dataPtr, destTy);
            } else {
                llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                val = builder->CreateLoad(destTy, typedPtr);
            }
            srcTy = destTy;
            break;
        }
        if (!isUnionType(srcTy)) {
            std::string unionName;
            if (isUnionType(destTy, &unionName)) {
                int tag = findUnionVariantTag(unionName, mret->values[i], val);
                if (tag == -1) {
                    cg_error(mret->pos, "return value doesn't match union variant", "QC-S263");
                    return;
                }

                llvm::Value* unionVal = llvm::ConstantAggregateZero::get(destTy);
                unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);
                llvm::Value* dataPtr = storeAndGetPointer(val);
                val = builder->CreateInsertValue(unionVal, dataPtr, 1);

                srcTy = destTy;
                break;
            }
        }
        agg = builder->CreateInsertValue(agg, val, i);
    }

    builder->CreateRet(agg);
    return;
}
void LLVMCompiler::emitRet(ReturnNode* ret) {
    emitDefersDownTo(0);
    if (auto varAccess = std::get_if<VarAccessNode*>(&ret->value)) {
        std::string name = (*varAccess)->var_name_tok.value;
        llvm::Value* alloc = getVarAddress(name);
        if (alloc) {
            llvm::Type* allocatedTy = getPointeeType(name);
            if (allocatedTy && allocatedTy->isArrayTy()) {
                llvm::Value* arrayPtr = builder->CreateBitCast(alloc, llvm::PointerType::get(context, 0), "array_ret_ptr");
                builder->CreateRet(arrayPtr);
                return;
            }
        }
    }
    if (auto call = std::get_if<CallNode*>(&ret->value)) {
        if (auto varAccess = std::get_if<VarAccessNode*>(&(*call)->node_to_call)) {
            std::string funcName = (*varAccess)->var_name_tok.value;

            if (classTypes.find(funcName) != classTypes.end()) {
                llvm::StructType* classTy = genericiseOrFindClass(funcName);
                std::vector<llvm::Value*> ctorArgs;
                for (auto& argNode : (*call)->arg_nodes) {
                    llvm::Value* arg = emitExpr(argNode);
                    if (!arg) return;
                    ctorArgs.push_back(arg);
                }

                std::string ctorName = funcName;
                llvm::Function* ctor = findMethodOverload(funcName, ctorName, ctorArgs);

                if (ctor) {
                    llvm::AllocaInst* retVal = createEntryAlloca("ret_val", classTy);

                    std::vector<llvm::Value*> allArgs = {retVal};
                    allArgs.insert(allArgs.end(), ctorArgs.begin(), ctorArgs.end());
                    if (insideTry()) {
                        auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                        llvm::InvokeInst* invoke = builder->CreateInvoke(ctor, contBB, currentLandingPad(), allArgs);
                        builder->SetInsertPoint(contBB);
                    } else {
                        builder->CreateCall(ctor, allArgs);
                    }
                    llvm::Value* result = llvm::ConstantAggregateZero::get(classTy);
                    for (unsigned i = 0; i < classTy->getNumElements(); i++) {
                        std::vector<llvm::Value*> indices = {builder->getInt32(0), builder->getInt32(i)};
                        llvm::Value* fieldPtr = builder->CreateInBoundsGEP(classTy, retVal, indices);
                        llvm::Type* fieldTy = classTy->getElementType(i);
                        llvm::Value* fieldVal = builder->CreateLoad(fieldTy, fieldPtr);

                        result = builder->CreateInsertValue(result, fieldVal, i);
                    }

                    builder->CreateRet(result);
                    return;
                }
            }
        }
    }
    llvm::Value* v = nullptr;
    llvm::Type* destTy = currentFunction->getReturnType();
    if (this->returnsRef()) { v = this->emitLValue(ret->value); }
    if (!v) { v = emitExpr(ret->value); }

    if (!v) {
        if (currentFunction->getReturnType()->isVoidTy()) {
            builder->CreateRetVoid();
        } else {
            cg_error(ret->pos, "return without value in non-void function", "QC-S265");
        }
        return;
    }
    llvm::Type* srcTy = v->getType();
    destTy = currentFunction->getReturnType();
    if (auto arrayLit = std::get_if<ArrayLiteralNode*>(&ret->value)) {
        if (destTy->isPointerTy() && srcTy->isArrayTy()) {
            llvm::ArrayType* arrayType = llvm::cast<llvm::ArrayType>(srcTy);
            llvm::Type* iTy = builder->getIntNTy(getPtrSize());
            llvm::Value* size = llvm::ConstantInt::get(iTy, module->getDataLayout().getTypeAllocSize(arrayType));
            llvm::Function* mallocFn = module->getFunction("qc_malloc");
            if (!mallocFn) {
                llvm::FunctionType* mallocTy = llvm::FunctionType::get(builder->getPtrTy(), {iTy}, false);
                mallocFn = llvm::Function::Create(mallocTy, llvm::Function::InternalLinkage, "qc_malloc", module);
            }

            llvm::Value* heapPtr = builder->CreateCall(mallocFn, {size});
            llvm::Value* typedPtr = builder->CreateBitCast(heapPtr, llvm::PointerType::get(context, 0));
            builder->CreateStore(v, typedPtr);
            llvm::Value* retPtr = builder->CreateBitCast(typedPtr, destTy);

            builder->CreateRet(retPtr);
            return;
        }
    }
    for (auto& [unionName, unionTy] : unionTypes) {
        if (srcTy == unionTy && !isUnionType(destTy)) {
            llvm::Value* dataPtr = builder->CreateExtractValue(v, 1, "union_data");
            if (destTy->isPointerTy()) {
                v = builder->CreateBitCast(dataPtr, destTy);
            } else {
                llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                v = builder->CreateLoad(destTy, typedPtr);
            }
            srcTy = destTy;
            break;
        }
        if (!isUnionType(srcTy) && destTy == unionTy) {
            int tag = findUnionVariantTag(unionName, ret->value, v);
            if (tag == -1) {
                cg_error(ret->pos, "return value doesn't match union variant", "QC-S263");
                return;
            }

            llvm::Value* unionVal = llvm::ConstantAggregateZero::get(unionTy);
            unionVal = builder->CreateInsertValue(unionVal, builder->getInt32(tag), 0);
            llvm::Value* dataPtr = storeAndGetPointer(v);
            unionVal = builder->CreateInsertValue(unionVal, dataPtr, 1);

            builder->CreateRet(unionVal);
            return;
        }
    }
    builder->CreateRet(v);
    return;
}
void LLVMCompiler::emitMultiVar(MultiVarDeclNode* mv) {
    llvm::Value* callVal = emitExpr(mv->value);
    if (!callVal) {
        cg_error(mv->var_names[0].pos, "failed to compile multi-var initializer", "QC-S266");
        return;
    }

    llvm::Type* retTy = callVal->getType();
    if (!retTy->isStructTy() || retTy->getStructNumElements() != mv->var_names.size()) {
        cg_error(mv->var_names[0].pos, "multi-return arity/type mismatch", "QC-T055");
        return;
    }

    for (size_t i = 0; i < mv->var_names.size(); ++i) {
        llvm::Value* field = builder->CreateExtractValue(callVal, i);
        std::string name = mv->var_names[i].value;
        std::string typeStr = mv->type_toks[i].value;
        if (typeStr.find("[]") != std::string::npos) {
            std::string baseType = typeStr;
            while (baseType.ends_with("[]")) { baseType = baseType.substr(0, baseType.length() - 2); }
            arrayTypeStrings[name] = baseType;
        }
        llvm::Type* srcTy = field->getType();
        llvm::Type* destTy = llvmTypeFor(typeStr);
        for (auto& [unionName, unionTy] : unionTypes) {
            if (srcTy == unionTy && !isUnionType(destTy)) {
                llvm::Value* dataPtr = builder->CreateExtractValue(field, 1, "union_data");
                if (destTy->isPointerTy()) {
                    field = builder->CreateBitCast(dataPtr, destTy);
                } else {
                    llvm::Value* typedPtr = builder->CreateBitCast(dataPtr, llvm::PointerType::get(context, 0));
                    field = builder->CreateLoad(destTy, typedPtr);
                }
                srcTy = destTy;
                break;
            }
        }
        llvm::AllocaInst* alloc = createEntryAlloca(name, destTy);
        builder->CreateStore(field, alloc);
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = alloc;
    }

    return;
}
void LLVMCompiler::emitIf(IfNode* if_node) {
    size_t outerDepth = defersStack.size();
    enterScope();
    if (if_node->init.has_value()) { emitStmt(if_node->init.value()); }
    llvm::Value* cond = emitExpr(if_node->condition);
    if (!cond) {
        exitScope();
        return;
    }
    llvm::ConstantInt* comptimeValue = nullptr;
    if (if_node->is_comptime) {
        comptimeValue = llvm::dyn_cast<llvm::ConstantInt>(cond);
        if (!comptimeValue) {
            cg_error(get_pos(if_node->condition), "comptime if condition must be evaluatable at compile time.", "QC-S267");
            return;
        }
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont", currentFunction);
        enterScope();
        if (comptimeValue->getZExtValue() != 0) {
            for (auto& stmt : if_node->then_branch->statements) { emitStmt(stmt); }
        } else {
            bool emitedBranch = false;
            for (size_t i = 0; i < if_node->elif_branches.size(); i++) {
                llvm::Value* elifCond = emitExpr(if_node->elif_branches[i].first);
                comptimeValue = llvm::dyn_cast<llvm::ConstantInt>(elifCond);
                if (!comptimeValue) {
                    cg_error(get_pos(if_node->elif_branches[i].first),
                             "all conditions including else ifs in a comptime if must be evaluatable at compile time.", "QC-S268");
                    return;
                }
                if (comptimeValue->getZExtValue() != 0) {
                    for (auto& stmt : if_node->elif_branches[i].second->statements) { emitStmt(stmt); }
                    if (!builder->GetInsertBlock()->getTerminator()) {
                        emitDefersDownTo(outerDepth + 2);
                        builder->CreateBr(mergeBB);
                    }
                    emitedBranch = true;
                    break;
                }
            }
            if (!emitedBranch && if_node->else_branch) {
                for (auto& stmt : if_node->else_branch->statements) { emitStmt(stmt); }
                if (!builder->GetInsertBlock()->getTerminator()) {
                    emitDefersDownTo(outerDepth + 2);
                    builder->CreateBr(mergeBB);
                }
            }
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 2);
            builder->CreateBr(mergeBB);
        }
        exitScope();
        builder->SetInsertPoint(mergeBB);
    } else {
        comptimeValue = llvm::dyn_cast<llvm::ConstantInt>(cond);
        if (comptimeValue) {
            warn("constant-condition", get_pos(if_node),
                 std::string("condition to if is compile-time, always will be ") + (comptimeValue->getZExtValue() == 0 ? "false" : "true"),
                 "QC-W015");
        }
        cond = normalizeValue(cond, if_node->condition);
        cond = toTruthiness(cond, get_pos(if_node->condition));
        if (!cond) {
            exitScope();
            return;
        }
        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", currentFunction);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont", currentFunction);
        std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> elifBlocks;
        for (size_t i = 0; i < if_node->elif_branches.size(); i++) {
            llvm::BasicBlock* elifCondBB = llvm::BasicBlock::Create(context, "elif.cond", currentFunction);
            llvm::BasicBlock* elifBodyBB = llvm::BasicBlock::Create(context, "elif.body", currentFunction);
            elifBlocks.push_back({elifCondBB, elifBodyBB});
        }

        llvm::BasicBlock* elseBB = nullptr;
        if (if_node->else_branch) { elseBB = llvm::BasicBlock::Create(context, "else", currentFunction); }
        llvm::BasicBlock* nextBB = elifBlocks.empty() ? (elseBB ? elseBB : mergeBB) : elifBlocks[0].first;
        builder->CreateCondBr(cond, thenBB, nextBB);
        builder->SetInsertPoint(thenBB);
        enterScope();
        for (auto& stmt : if_node->then_branch->statements) { emitStmt(stmt); }
        if (if_node->then_branch->statements.empty()) { warn("empty-body", get_pos(if_node), "if body is empty", "QC-W16"); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 2);
            builder->CreateBr(mergeBB);
        }
        exitScope();
        for (size_t i = 0; i < elifBlocks.size(); i++) {
            builder->SetInsertPoint(elifBlocks[i].first);
            llvm::Value* elifCond = emitExpr(if_node->elif_branches[i].first);
            elifCond = normalizeValue(elifCond, if_node->elif_branches[i].first);
            elifCond = toTruthiness(elifCond, get_pos(if_node->elif_branches[i].first));
            comptimeValue = llvm::dyn_cast<llvm::ConstantInt>(elifCond);
            if (comptimeValue) {
                warn("constant-condition", get_pos(if_node),
                     std::string("condition to else if is compile-time, always will be ") + (comptimeValue->getZExtValue() == 0 ? "false" : "true"),
                     "QC-W015");
            }
            llvm::BasicBlock* nextElifBB = (i + 1 < elifBlocks.size()) ? elifBlocks[i + 1].first : (elseBB ? elseBB : mergeBB);
            builder->CreateCondBr(elifCond, elifBlocks[i].second, nextElifBB);

            builder->SetInsertPoint(elifBlocks[i].second);
            enterScope();
            for (auto& stmt : if_node->elif_branches[i].second->statements) { emitStmt(stmt); }
            if (if_node->elif_branches[i].second->statements.empty()) { warn("empty-body", get_pos(if_node), "else if body is empty", "QC-W16"); }
            if (!builder->GetInsertBlock()->getTerminator()) {
                emitDefersDownTo(outerDepth + 2);
                builder->CreateBr(mergeBB);
            }
            exitScope();
        }
        if (elseBB) {
            builder->SetInsertPoint(elseBB);
            enterScope();
            for (auto& stmt : if_node->else_branch->statements) { emitStmt(stmt); }
            if (if_node->else_branch->statements.empty()) { warn("empty-body", get_pos(if_node), "else body is empty", "QC-W16"); }
            if (!builder->GetInsertBlock()->getTerminator()) {
                emitDefersDownTo(outerDepth + 2);
                builder->CreateBr(mergeBB);
            }
            exitScope();
        }
        exitScope();
        builder->SetInsertPoint(mergeBB);
    }
}
void LLVMCompiler::emitWhile(WhileNode* while_node) {
    size_t outerDepth = defersStack.size();
    loopStack.push_back(outerDepth);
    enterScope();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "while.cond", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "while.body", currentFunction);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "while.end", currentFunction);
    llvm::BasicBlock* oldBreakBB = currentBreakBB;
    llvm::BasicBlock* oldContinueBB = currentContinueBB;
    currentBreakBB = endBB;
    currentContinueBB = condBB;
    if (while_node->is_dowhile) {
        builder->CreateBr(bodyBB);
    } else {
        builder->CreateBr(condBB);
    }
    builder->SetInsertPoint(condBB);
    llvm::Value* cond = emitExpr(while_node->condition);
    if (!cond) return;
    cond = normalizeValue(cond, while_node->condition);
    cond = toTruthiness(cond, get_pos(while_node->condition));
    if (!cond) return;
    builder->CreateCondBr(cond, bodyBB, endBB);
    builder->SetInsertPoint(bodyBB);
    for (auto& stmt : while_node->body->statements) { emitStmt(stmt); }
    if (while_node->body->statements.empty()) { warn("empty-body", get_pos(while_node), "while body is empty", "QC-W16"); }

    if (!builder->GetInsertBlock()->getTerminator()) {
        emitDefersDownTo(outerDepth + 1);
        builder->CreateBr(condBB);
    }
    currentBreakBB = oldBreakBB;
    currentContinueBB = oldContinueBB;
    exitScope();
    loopStack.pop_back();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitFor(ForNode* for_node) {
    size_t outerDepth = defersStack.size();
    loopStack.push_back(outerDepth);
    enterScope();
    if (for_node->init.has_value()) { emitStmt(for_node->init.value()); }
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "for.cond", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "for.body", currentFunction);
    llvm::BasicBlock* incBB = llvm::BasicBlock::Create(context, "for.inc", currentFunction);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "for.end", currentFunction);
    llvm::BasicBlock* oldBreakBB = currentBreakBB;
    llvm::BasicBlock* oldContinueBB = currentContinueBB;
    currentBreakBB = endBB;
    currentContinueBB = incBB;
    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);
    llvm::Value* cond = emitExpr(for_node->condition);
    if (!cond) return;
    cond = normalizeValue(cond, for_node->condition);
    cond = toTruthiness(cond, get_pos(for_node->condition));
    if (!cond) return;
    builder->CreateCondBr(cond, bodyBB, endBB);
    builder->SetInsertPoint(bodyBB);
    for (auto& stmt : for_node->body->statements) { emitStmt(stmt); }
    if (!builder->GetInsertBlock()->getTerminator()) {
        emitDefersDownTo(outerDepth + 1);
        builder->CreateBr(incBB);
    }
    builder->SetInsertPoint(incBB);
    if (for_node->update.has_value()) { emitStmt(for_node->update.value()); }
    builder->CreateBr(condBB);
    currentBreakBB = oldBreakBB;
    currentContinueBB = oldContinueBB;
    exitScope();
    loopStack.pop_back();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitSwitch(SwitchNode* switch_node) {
    size_t outerDepth = defersStack.size();
    enterScope();
    llvm::Value* switchVal = emitExpr(switch_node->value);
    if (!switchVal) return;
    switchVal = normalizeValue(switchVal, switch_node->value);
    llvm::Type* switchTy = switchVal->getType();
    bool canUseSwitch = switchTy->isIntegerTy();
    for (auto& [unionName, unionTy] : unionTypes) {
        if (switchTy == unionTy) {
            canUseSwitch = false;
            break;
        }
    }
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "switch.end", currentFunction);
    std::vector<llvm::BasicBlock*> sectionBlocks;
    llvm::BasicBlock* defaultBB = nullptr;
    for (auto& section : switch_node->sections) {
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "switch.case", currentFunction);
        sectionBlocks.push_back(bb);
        if (section.is_default) { defaultBB = bb; }
    }
    if (!defaultBB) { defaultBB = endBB; }
    if (canUseSwitch) {
        llvm::SwitchInst* switchInst = builder->CreateSwitch(switchVal, defaultBB, switch_node->sections.size());
        for (size_t i = 0; i < switch_node->sections.size(); i++) {
            auto& section = switch_node->sections[i];
            if (section.is_default) continue;
            for (auto& caseLabel : section.cases) {
                llvm::Value* caseVal = emitExpr(caseLabel.expr);
                if (!caseVal || !(caseVal = normalizeValue(caseVal, caseLabel.expr), caseVal)) {
                    cg_error(get_pos(caseLabel.expr), "Failed to emit switch case value", "QC-S278");
                    return;
                }
                if (auto constInt = llvm::dyn_cast<llvm::ConstantInt>(caseVal)) { switchInst->addCase(constInt, sectionBlocks[i]); }
            }
        }
    } else {
        llvm::BasicBlock* currentCheckBB = builder->GetInsertBlock();
        for (size_t i = 0; i < switch_node->sections.size(); i++) {
            auto& section = switch_node->sections[i];
            if (section.is_default) { continue; }
            llvm::BasicBlock* nextCheckBB = (i + 1 < switch_node->sections.size())
                                                ? llvm::BasicBlock::Create(context, "switch.check", currentFunction)
                                                : defaultBB;
            builder->SetInsertPoint(currentCheckBB);
            llvm::Value* matches = nullptr;
            for (auto& caseLabel : section.cases) {
                llvm::Value* caseVal = emitExpr(caseLabel.expr);
                llvm::Value* cmp = nullptr;
                if (switchTy->isPointerTy() && caseVal->getType()->isPointerTy()) {
                    llvm::Function* strcmp_fn = module->getFunction("qc_string_eq");
                    cmp = builder->CreateCall(strcmp_fn, {switchVal, caseVal});
                } else if (switchTy->isIntegerTy()) {
                    cmp = builder->CreateICmpEQ(switchVal, caseVal);
                } else if (switchTy->isFloatingPointTy()) {
                    cmp = builder->CreateFCmpOEQ(switchVal, caseVal);
                } else {
                    llvm::Value* switchTag = builder->CreateExtractValue(switchVal, 0);
                    llvm::Value* caseTag = builder->CreateExtractValue(caseVal, 0);
                    llvm::Value* tagMatch = builder->CreateICmpEQ(switchTag, caseTag);
                    llvm::Value* switchData = builder->CreateExtractValue(switchVal, 1);
                    llvm::Value* caseData = builder->CreateExtractValue(caseVal, 1);
                    llvm::Value* dataMatch = builder->CreateICmpEQ(switchData, caseData);
                    cmp = builder->CreateAnd(tagMatch, dataMatch);
                }
                if (matches) {
                    matches = builder->CreateOr(matches, cmp);
                } else {
                    matches = cmp;
                }
            }
            builder->CreateCondBr(matches, sectionBlocks[i], nextCheckBB);
            currentCheckBB = nextCheckBB;
        }
    }
    llvm::BasicBlock* oldBreakBB = currentBreakBB;
    currentBreakBB = endBB;
    for (size_t i = 0; i < switch_node->sections.size(); i++) {
        builder->SetInsertPoint(sectionBlocks[i]);
        for (auto& stmt : switch_node->sections[i].body->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            if (i + 1 < sectionBlocks.size()) {
                builder->CreateBr(sectionBlocks[i + 1]);
            } else {
                emitDefersDownTo(outerDepth + 1);
                builder->CreateBr(endBB);
            }
        }
    }
    currentBreakBB = oldBreakBB;
    exitScope();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitQIf(QIfNode* qif_node) {
    size_t outerDepth = defersStack.size();
    enterScope();
    if (qif_node->init.has_value()) { emitStmt(qif_node->init.value()); }
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "qif.end", currentFunction);

    llvm::Value* qifCond = emitExpr(qif_node->condition);
    qifCond = normalizeValue(qifCond, qif_node->condition);
    llvm::Value* qifBit1 = builder->CreateAnd(qifCond, builder->getIntN(2, 0b10));
    llvm::Value* qif_is_true = builder->CreateICmpNE(qifBit1, builder->getIntN(2, 0));

    llvm::BasicBlock* qifBodyBB = llvm::BasicBlock::Create(context, "qif.body", currentFunction);
    llvm::BasicBlock* nextBB = (qif_node->qelif_branches.empty() && !qif_node->qelse_branch)
                                   ? endBB
                                   : llvm::BasicBlock::Create(context, "qelif.check", currentFunction);

    builder->CreateCondBr(qif_is_true, qifBodyBB, nextBB);

    builder->SetInsertPoint(qifBodyBB);
    enterScope();
    for (auto& stmt : qif_node->then_branch->statements) { emitStmt(stmt); }
    if (!builder->GetInsertBlock()->getTerminator()) {
        emitDefersDownTo(outerDepth + 2);
        builder->CreateBr(endBB);
    }
    exitScope();
    for (size_t i = 0; i < qif_node->qelif_branches.size(); i++) {
        builder->SetInsertPoint(nextBB);

        llvm::Value* elifCond = emitExpr(qif_node->qelif_branches[i].first);
        llvm::Value* elifBit1 = builder->CreateAnd(elifCond, builder->getIntN(2, 0b10));
        llvm::Value* elif_is_true = builder->CreateICmpNE(elifBit1, builder->getIntN(2, 0));

        llvm::BasicBlock* elifBodyBB = llvm::BasicBlock::Create(context, "qelif.body", currentFunction);
        llvm::BasicBlock* nextElifBB = (i + 1 < qif_node->qelif_branches.size() || qif_node->qelse_branch)
                                           ? llvm::BasicBlock::Create(context, "qelif.check", currentFunction)
                                           : endBB;

        builder->CreateCondBr(elif_is_true, elifBodyBB, nextElifBB);
        builder->SetInsertPoint(elifBodyBB);
        enterScope();
        for (auto& stmt : qif_node->qelif_branches[i].second->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 2);
            builder->CreateBr(endBB);
        }
        exitScope();
        nextBB = nextElifBB;
    }

    if (qif_node->qelse_branch) {
        builder->SetInsertPoint(nextBB);
        llvm::Value* qifBit0 = builder->CreateAnd(qifCond, builder->getIntN(2, 0b01));
        llvm::Value* all_false = builder->CreateICmpNE(qifBit0, builder->getIntN(2, 0));
        for (auto& qelif : qif_node->qelif_branches) {
            llvm::Value* elifCond = emitExpr(qelif.first);
            llvm::Value* elifBit0 = builder->CreateAnd(elifCond, builder->getIntN(2, 0b01));
            llvm::Value* elif_false = builder->CreateICmpNE(elifBit0, builder->getIntN(2, 0));
            all_false = builder->CreateAnd(all_false, elif_false);
        }

        llvm::BasicBlock* qelseBodyBB = llvm::BasicBlock::Create(context, "qelse.body", currentFunction);
        builder->CreateCondBr(all_false, qelseBodyBB, endBB);

        builder->SetInsertPoint(qelseBodyBB);
        enterScope();
        for (auto& stmt : qif_node->qelse_branch->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 2);
            builder->CreateBr(endBB);
        }
        exitScope();
    }
    exitScope();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitQSwitch(QSwitchNode* qsw) {
    size_t outerDepth = defersStack.size();
    enterScope();
    llvm::Value* qb_val = emitExpr(qsw->value);
    if (!qb_val) {
        cg_error(get_pos(qsw), "failed to compile qswitch value", "QC-S271");
        return;
    }
    qb_val = normalizeValue(qb_val, qsw->value);
    if (qb_val->getType() != builder->getIntNTy(2)) {
        cg_error(get_pos(qsw), "qswitch requires qbool type", "QC-T056");
        return;
    }
    llvm::BasicBlock* check_true = llvm::BasicBlock::Create(context, "qsw.check_true", currentFunction);
    llvm::BasicBlock* check_false = llvm::BasicBlock::Create(context, "qsw.check_false", currentFunction);
    llvm::BasicBlock* case_t_block = nullptr;
    llvm::BasicBlock* case_f_block = nullptr;
    llvm::BasicBlock* case_n_block = nullptr;
    llvm::BasicBlock* case_b_block = nullptr;
    llvm::BasicBlock* qswitch_end = llvm::BasicBlock::Create(context, "qswitch.end", currentFunction);
    if (qsw->case_t) { case_t_block = llvm::BasicBlock::Create(context, "qsw.case_t", currentFunction); }
    if (qsw->case_f) { case_f_block = llvm::BasicBlock::Create(context, "qsw.case_f", currentFunction); }
    if (qsw->case_n) { case_n_block = llvm::BasicBlock::Create(context, "qsw.case_n", currentFunction); }
    if (qsw->case_b) { case_b_block = llvm::BasicBlock::Create(context, "qsw.case_b", currentFunction); }
    builder->CreateBr(check_true);
    builder->SetInsertPoint(check_true);
    llvm::Value* has_true = builder->CreateAnd(qb_val, builder->getIntN(2, 2), "has_true");
    llvm::Value* is_true = builder->CreateICmpNE(has_true, builder->getIntN(2, 0), "is_true");
    builder->CreateCondBr(is_true, check_false, check_false);
    builder->SetInsertPoint(check_false);
    llvm::Value* has_false = builder->CreateAnd(qb_val, builder->getIntN(2, 1), "has_false");
    llvm::Value* is_false = builder->CreateICmpNE(has_false, builder->getIntN(2, 0), "is_false");
    llvm::Value* is_both = builder->CreateAnd(is_true, is_false, "is_both");
    llvm::Value* not_false = builder->CreateNot(is_false, "not_false");
    llvm::Value* is_qtrue_only = builder->CreateAnd(is_true, not_false, "is_qtrue_only");
    llvm::Value* not_true = builder->CreateNot(is_true, "not_true");
    llvm::Value* is_qfalse_only = builder->CreateAnd(not_true, is_false, "is_qfalse_only");
    llvm::Value* is_none = builder->CreateAnd(not_true, not_false, "is_none");
    llvm::BasicBlock* check_qtrue = llvm::BasicBlock::Create(context, "qsw.check_qtrue", currentFunction);
    llvm::BasicBlock* check_qfalse = llvm::BasicBlock::Create(context, "qsw.check_qfalse", currentFunction);
    llvm::BasicBlock* check_none_final = llvm::BasicBlock::Create(context, "qsw.check_none_final", currentFunction);
    builder->CreateCondBr(is_both, case_b_block ? case_b_block : qswitch_end, check_qtrue);
    builder->SetInsertPoint(check_qtrue);
    builder->CreateCondBr(is_qtrue_only, case_t_block ? case_t_block : qswitch_end, check_qfalse);
    builder->SetInsertPoint(check_qfalse);
    builder->CreateCondBr(is_qfalse_only, case_f_block ? case_f_block : qswitch_end, check_none_final);
    builder->SetInsertPoint(check_none_final);
    builder->CreateCondBr(is_none, case_n_block ? case_n_block : qswitch_end, qswitch_end);
    if (case_t_block && qsw->case_t) {
        builder->SetInsertPoint(case_t_block);
        for (auto& stmt : qsw->case_t->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(qswitch_end);
        }
    }

    if (case_f_block && qsw->case_f) {
        builder->SetInsertPoint(case_f_block);
        for (auto& stmt : qsw->case_f->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(qswitch_end);
        }
    }

    if (case_n_block && qsw->case_n) {
        builder->SetInsertPoint(case_n_block);
        for (auto& stmt : qsw->case_n->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(qswitch_end);
        }
    }
    if (case_b_block && qsw->case_b) {
        builder->SetInsertPoint(case_b_block);
        for (auto& stmt : qsw->case_b->statements) { emitStmt(stmt); }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(qswitch_end);
        }
    }
    exitScope();
    builder->SetInsertPoint(qswitch_end);
}
void LLVMCompiler::emitArrDecl(ArrayDeclNode* arrDecl) {
    std::string name = arrDecl->var_name_tok.value;
    std::string elemType = arrDecl->type_tok.value;
    bool isVolatile = false;
    if (elemType.starts_with("volatile ")) {
        isVolatile = true;
        elemType.erase(0, 9);
    }
    llvm::Type* elemTy = llvmTypeFor(elemType);
    if (!elemTy) {
        cg_error(arrDecl->type_tok.pos, "unknown array element type: " + elemType, "QC-T057");
        return;
    }
    arrayTypeStrings[name] = elemType;
    if (!std::holds_alternative<ArrayLiteralNode*>(arrDecl->value) && !std::holds_alternative<std::monostate>(arrDecl->value)) {
        llvm::Value* arrPtr = emitExpr(arrDecl->value);
        if (!arrPtr) return;
        llvm::AllocaInst* alloc = createEntryAlloca(name, arrPtr->getType());
        builder->CreateStore(arrPtr, alloc, isVolatile);
        locals[name] = alloc;
        volatileVars[name] = isVolatile;
        return;
    }
    if (auto arrLit = std::get_if<ArrayLiteralNode*>(&arrDecl->value)) {
        if ((*arrLit)->elements.empty()) {
            llvm::Value* lengthValue = emitExpr((*arrLit)->length);
            auto* lengthConstant = llvm::dyn_cast<llvm::ConstantInt>(lengthValue);
            if (!lengthConstant) {
                cg_error(get_pos(*arrLit), "array length must be constant", "QC-S272");
                return;
            }
            uint64_t length = lengthConstant->getZExtValue();
            auto* arrTy = llvm::ArrayType::get(elemTy, length);
            auto* alloc = createEntryAlloca(name, arrTy);
            uint64_t bytes = module->getDataLayout().getTypeAllocSize(arrTy).getFixedValue();
            if (config.use_runtime) builder->CreateMemSet(alloc, builder->getInt8(0), builder->getInt64(bytes), llvm::MaybeAlign(1), isVolatile);
            locals[name] = alloc;
            arrayTypeStrings[name] = elemType;
            arrayLengths[name] = length;
            volatileVars[name] = isVolatile;
            return;
        }
        bool hasSpread = false;
        for (auto& elem : (*arrLit)->elements) {
            if (std::holds_alternative<SpreadNode*>(elem)) {
                hasSpread = true;
                break;
            }
        }
        if (hasSpread) {
            llvm::Value* arrPtr = emitExpr(arrDecl->value);
            if (!arrPtr) return;
            llvm::Value* totalSize = builder->getInt32(0);
            if (auto arrLit = std::get_if<ArrayLiteralNode*>(&arrDecl->value)) {
                for (auto& elem : (*arrLit)->elements) {
                    if (auto spread = std::get_if<SpreadNode*>(&elem)) {
                        llvm::Value* collVal = emitExpr((*spread)->expr);
                        llvm::Value* spreadLen = getCollectionLength(collVal, (*spread)->expr);
                        totalSize = builder->CreateAdd(totalSize, spreadLen);
                    } else {
                        totalSize = builder->CreateAdd(totalSize, builder->getInt32(1));
                    }
                }
            }
            if (!arrDecl->sizes.empty() && arrDecl->sizes[0].has_value()) {
                int userSize = *arrDecl->sizes[0];
                arrayLengths[name] = userSize;
            } else if (auto* constSize = llvm::dyn_cast<llvm::ConstantInt>(totalSize)) {
                arrayLengths[name] = constSize->getSExtValue();
            } else {
                llvm::AllocaInst* sizeAlloc = createEntryAlloca(name + "_size", builder->getInt32Ty());
                builder->CreateStore(totalSize, sizeAlloc);
                runtimeArraySizes[name] = sizeAlloc;
            }
            llvm::AllocaInst* alloc = createEntryAlloca(name, arrPtr->getType());
            builder->CreateStore(arrPtr, alloc, isVolatile);
            std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
            locals[fullName] = alloc;
            arrayTypeStrings[fullName] = elemType;
            volatileVars[fullName] = isVolatile;
            return;
        }
        size_t arraySize = (*arrLit)->elements.size();
        arrayLengths[name] = arraySize;
        std::vector<uint64_t> actualSizes;
        std::function<void(AnyNode&, int)> inferDims;
        inferDims = [&](AnyNode& node, int depth) {
            if (auto lit = std::get_if<ArrayLiteralNode*>(&node)) {
                if (actualSizes.size() <= depth) { actualSizes.push_back((*lit)->elements.size()); }
                if (!(*lit)->elements.empty()) { inferDims((*lit)->elements[0], depth + 1); }
            }
        };
        inferDims(arrDecl->value, 0);
        llvm::Type* arrTy = elemTy;
        for (int i = actualSizes.size() - 1; i >= 0; i--) { arrTy = llvm::ArrayType::get(arrTy, actualSizes[i]); }
        bool useHeap = (currentFunction != nullptr);
        llvm::AllocaInst* alloc;
        if (useHeap) {
            llvm::Function* mallocFn = module->getFunction("qc_malloc");
            if (!mallocFn) {
                llvm::FunctionType* mallocTy = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {builder->getIntNTy(getPtrSize())}, false);
                mallocFn = llvm::Function::Create(mallocTy, llvm::Function::InternalLinkage, "qc_malloc", module);
            }
            const llvm::DataLayout& DL = module->getDataLayout();
            uint64_t sizeBytes = DL.getTypeAllocSize(arrTy);
            llvm::Value* mallocCall = builder->CreateCall(mallocFn, {builder->getInt64(sizeBytes)}, "heap_arr");
            llvm::Value* arrPtr = builder->CreateBitCast(mallocCall, llvm::PointerType::get(context, 0), "arr_cast");
            alloc = createEntryAlloca(name, llvm::PointerType::get(context, 0));
            builder->CreateStore(arrPtr, alloc, isVolatile);
            arrayTypeStrings[name] = elemType;
        } else {
            alloc = createEntryAlloca(name, arrTy);
        }
        std::string fullName = getCurrentNamespace().empty() ? name : getCurrentNamespace() + "::" + name;
        locals[fullName] = alloc;
        volatileVars[fullName] = isVolatile;
        std::function<void(llvm::Value*, llvm::Type*, AnyNode&, std::vector<uint64_t>&)> initArray;
        initArray = [&](llvm::Value* ptr, llvm::Type* ty, AnyNode& node, std::vector<uint64_t>& indices) {
            if (auto lit = std::get_if<ArrayLiteralNode*>(&node)) {
                for (size_t i = 0; i < (*lit)->elements.size(); i++) {
                    indices.push_back(i);
                    if (std::holds_alternative<ArrayLiteralNode*>((*lit)->elements[i])) {
                        initArray(ptr, ty, (*lit)->elements[i], indices);
                    } else {
                        llvm::Value* elemVal = emitExpr((*lit)->elements[i]);
                        if (!elemVal) return;
                        std::vector<llvm::Value*> llvmIndices = useHeap ? std::vector<llvm::Value*>{builder->getInt32(0)}
                                                                        : std::vector<llvm::Value*>{builder->getInt32(0)};
                        for (auto idx : indices) { llvmIndices.push_back(builder->getInt32(idx)); }
                        llvm::Value* basePtr = useHeap ? builder->CreateLoad(llvm::PointerType::get(context, 0), alloc, "heap_ptr")
                                                       : static_cast<llvm::Value*>(alloc);
                        llvm::Value* elemPtr = builder->CreateInBoundsGEP(arrTy, basePtr, llvmIndices);
                        builder->CreateStore(elemVal, elemPtr);
                    }
                    indices.pop_back();
                }
            }
        };
        std::vector<uint64_t> indices;
        initArray(alloc, arrTy, arrDecl->value, indices);
    }
    if (!arrDecl->sizes.empty() && arrDecl->sizes[0].has_value()) {
        int arraySize = *arrDecl->sizes[0];
        if (std::holds_alternative<std::monostate>(arrDecl->value)) {
            llvm::ArrayType* arrTy = llvm::ArrayType::get(elemTy, arraySize);
            llvm::AllocaInst* alloc = createEntryAlloca(name, arrTy);
            const llvm::DataLayout& dl = module->getDataLayout();
            uint64_t sizeBytes = dl.getTypeAllocSize(arrTy).getFixedValue();
            if (config.use_runtime) builder->CreateMemSet(alloc, builder->getInt8(0), builder->getInt64(sizeBytes), llvm::MaybeAlign(1), isVolatile);
            locals[name] = alloc;
            arrayTypeStrings[name] = elemType;
            arrayLengths[name] = arraySize;
            volatileVars[name] = isVolatile;
            return;
        }
    }
    return;
}
void LLVMCompiler::emitArrAssign(ArrayAssignNode* arrAssign) {
    if (auto arrAcc = safe_get<ArrayAccessNode>(arrAssign->array_access)) {
        std::string ptrTy = getExpressionType(arrAcc->base);
        if (ptrTy.ends_with("*") || ptrTy == "@nullptr") {
            if (ptrTy == "@nullptr") {
                warn("null-deref", get_pos(arrAcc), "attempted to dereference nullptr", "QC-W008");
                return;
            }
            if (ptrTy == "void*") {
                cg_error(get_pos(arrAcc), "you cannot dereference or indice void*", "QC-S273");
                return;
            }
            std::string valueTy = getExpressionType(arrAcc->indices[0]);
            if (llvm::Type* ty = llvmTypeFor(valueTy); !ty || !ty->isIntegerTy()) {
                cg_error(get_pos(arrAcc->indices[0]),
                         "attempted to index a pointer with a "
                         "non-integer value.",
                         "QC-S239");
                return;
            }
            llvm::Value* value = emitExpr(arrAcc->indices[0]);
            if (!value) {
                cg_error(get_pos(arrAcc->indices[0]), "failed to emit index for pointer index.", "QC-S274");
                return;
            }
            ptrTy.pop_back();
            llvm::Value* addr = builder->CreateGEP(llvmTypeFor(ptrTy), emitExpr(arrAcc->base), value, "ptr_arr_asi");
            llvm::Value* valToStore = emitExpr(arrAssign->value);
            builder->CreateStore(valToStore, addr);
            return;
        }
        if (genericiseOrFindClass(ptrTy)) {
            llvm::Value* obj = emitLValue(arrAcc->base, true);
            llvm::Value* idx = emitExpr(arrAcc->indices[0]);
            llvm::Value* ref = emitVirtualOrDirectCall(ptrTy, "operator[]", obj, {idx});
            if (!ref) {
                cg_error(get_pos(arrAcc->base), ptrTy + " does not have operator[]", "QC-S240");
                addMethodNotes(ptrTy, "operator[]", {idx}, get_pos(arrAcc->base));
                return;
            }
            llvm::Value* val = emitExpr(arrAssign->value);
            builder->CreateStore(val, ref);
            return;
        }
        if (auto varAcc = safe_get<VarAccessNode>(arrAcc->base)) {
            std::string name = varAcc->var_name_tok.value;
            llvm::Value* alloc = getVarAddress(name);
            if (!alloc) {
                cg_error(get_pos(arrAcc->base), "unknown array: " + name, "QC-S242");
                return;
            }
            llvm::Value* arrAlloc = alloc;
            llvm::Type* arrTy = getPointeeType(name);
            llvm::Value* indexVal = emitExpr(arrAcc->indices[0]);
            if (!indexVal) return;
            llvm::Value* valueVal = emitExpr(arrAssign->value);
            if (!valueVal) return;
            if (arrTy->isPointerTy()) {
                llvm::Value* ptr = builder->CreateLoad(arrTy, arrAlloc, hasVolatileVar(name) ? findVolatileVar(name)->second : false, "arr_ptr");
                llvm::Type* elemTy = valueVal->getType();
                llvm::Value* elemPtr = builder->CreateGEP(elemTy, ptr, indexVal, "arr_elem_ptr");
                builder->CreateStore(valueVal, elemPtr, hasVolatileVar(name) ? findVolatileVar(name)->second : false);
            } else if (arrTy->isArrayTy()) {
                std::vector<llvm::Value*> indices = {builder->getInt32(0), indexVal};
                llvm::Value* elemPtr = builder->CreateInBoundsGEP(arrTy, arrAlloc, indices, "arr_elem_ptr");
                builder->CreateStore(valueVal, elemPtr, hasVolatileVar(name) ? findVolatileVar(name)->second : false);
            }
        }
    }
    return;
}
void LLVMCompiler::emitForeach(ForeachNode* foreach) {
    size_t outerDepth = defersStack.size();
    loopStack.push_back(outerDepth);
    std::string elemName = foreach->elem_name.value;
    std::string iterName = "__foreach_i_" + elemName;
    llvm::Value* lengthVal = nullptr;
    bool isArray = false;
    llvm::Value* arrayAlloc = nullptr;
    llvm::Type* arrayElemTy = nullptr;
    std::string collName = "";
    std::string collTypeName = "";
    llvm::Function* beginOverload = nullptr;
    llvm::Value* collVal;
    if (auto varAccess = std::get_if<VarAccessNode*>(&foreach->collection)) {
        collName = (*varAccess)->var_name_tok.value;
        collTypeName = resolveVarType(collName);
        llvm::Value* alloc = getVarAddress(collName);
        if (alloc) {
            llvm::Type* allocTy = getPointeeType(collName);

            if (allocTy->isArrayTy()) {
                isArray = true;
                arrayAlloc = alloc;
                lengthVal = builder->getInt32(allocTy->getArrayNumElements());
            } else if (allocTy->isPointerTy()) {
                if (hasArrayLength(collName)) {
                    auto lenIt = findArrayLength(collName);
                    isArray = true;
                    arrayAlloc = alloc;
                    lengthVal = builder->getInt32(lenIt->second);

                    if (hasArrayType(collName)) {
                        auto typeIt = findArrayType(collName);
                        arrayElemTy = llvmTypeFor(typeIt->second);
                    } else {
                    }
                } else if (runtimeArraySizes.find(collName) != runtimeArraySizes.end()) {
                    isArray = true;
                    arrayAlloc = alloc;
                    llvm::AllocaInst* sizeAlloc = runtimeArraySizes[collName];
                    lengthVal = builder->CreateLoad(builder->getInt32Ty(), sizeAlloc, "runtime_len");
                    if (hasArrayType(collName)) {
                        auto typeIt = findArrayType(collName);
                        arrayElemTy = llvmTypeFor(typeIt->second);
                    }
                } else {
                }
            }
        }
        beginOverload = findMethodOverload(collTypeName, "_begin", {});
        bool isIterator = !isArray && !collTypeName.empty() && beginOverload != nullptr;
        collVal = isIterator ? emitLValue(foreach->collection) : emitExpr(foreach->collection);
    } else {
        collVal = userTypes.find(baseTypeName(getExpressionType(foreach->collection))) == userTypes.end() ? emitExpr(foreach->collection)
                                                                                                          : emitLValue(foreach->collection);
        if (userTypes.find(baseTypeName(getExpressionType(foreach->collection))) != userTypes.end()) {
            if (userTypes[baseTypeName(getExpressionType(foreach->collection))].kind == UserTypeKind::Class) {
                collTypeName = getExpressionType(foreach->collection);
                isArray = false;
            }
        } else {
            if (collVal->getType()->isArrayTy()) {
                collTypeName = getExpressionType(foreach->collection);
                isArray = true;
            }
        }
    }
    beginOverload = findMethodOverload(collTypeName, "_begin", {});
    bool isIterator = !isArray && !collTypeName.empty() && beginOverload != nullptr;
    llvm::AllocaInst* iterObjAlloc = nullptr;
    std::string iterTypeName = "";
    llvm::Type* iterLLVMTy = nullptr;
    llvm::Value* iterLoaded = nullptr;
    if (!collVal) return;
    if (isIterator) {
        auto baseInfo = userTypes.find(baseTypeName(collTypeName));
        auto info = baseInfo->second;
        auto oldNamespaceStack = namespaceStack;
        namespaceStack.clear();

        if (!info.namespace_path.empty()) {
            size_t start = 0;
            size_t pos;

            while ((pos = info.namespace_path.find("::", start)) != std::string::npos) {
                namespaceStack.push_back(info.namespace_path.substr(start, pos - start));
                start = pos + 2;
            }

            namespaceStack.push_back(info.namespace_path.substr(start));
        }
        iterTypeName = resolveTypeName(getMethodReturnTypeName(collTypeName, "_begin"), false);
        auto concreteParams = genericParamsFromName(collTypeName);
        for (size_t i = 0; i < baseInfo->second.generics.size() && i < concreteParams.size(); i++) {
            std::string gname = baseInfo->second.generics[i].name;
            std::string gval = concreteParams[i];
            size_t pos;
            while ((pos = iterTypeName.find(gname, pos)) != std::string::npos) {
                size_t end = pos + gname.size();
                bool leftOk = pos == 0 || !(std::isalnum(static_cast<unsigned char>(iterTypeName[pos - 1])) || iterTypeName[pos - 1] == '_');
                bool rightOk = end == iterTypeName.size() ||
                               !(std::isalnum(static_cast<unsigned char>(iterTypeName[end])) || iterTypeName[end] == '_');
                if (leftOk && rightOk) {
                    iterTypeName.replace(pos, gname.size(), gval);
                    pos += gval.size();
                } else {
                    pos += gname.size();
                }
            }
        }
        iterLLVMTy = llvmTypeFor(iterTypeName);
        iterObjAlloc = createEntryAlloca("__iter_" + elemName, iterLLVMTy);
        llvm::Value* iterObj = emitMethodCall(beginOverload, collVal, {}, "_begin");
        builder->CreateStore(iterObj, iterObjAlloc);
        namespaceStack = oldNamespaceStack;
    }
    enterScope();
    llvm::BasicBlock* savedBreakBB = currentBreakBB;
    llvm::BasicBlock* savedContinueBB = currentContinueBB;
    llvm::Type* elemTy = llvmTypeFor(foreach->elem_type.value);
    llvm::AllocaInst* iterAlloc = createEntryAlloca(iterName, builder->getInt32Ty());
    llvm::AllocaInst* elemAlloc = createEntryAlloca(elemName, elemTy);
    locals[iterName] = iterAlloc;
    locals[elemName] = elemAlloc;
    varTypes[elemName] = foreach->elem_type.value;
    varTypes[iterName] = "int";
    builder->CreateStore(builder->getInt32(0), iterAlloc);
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "foreach.cond", currentFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "foreach.body", currentFunction);
    llvm::BasicBlock* incBB = nullptr;
    if (isArray) incBB = llvm::BasicBlock::Create(context, "foreach.inc", currentFunction);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "foreach.end", currentFunction);
    if (isIterator)
        currentContinueBB = condBB;
    else
        currentContinueBB = incBB;
    if (!builder->GetInsertBlock()->getTerminator()) { builder->CreateBr(condBB); }
    builder->SetInsertPoint(condBB);
    if (isArray) {
        llvm::Value* iVal = builder->CreateLoad(builder->getInt32Ty(), iterAlloc, iterName);
        llvm::Value* cmpVal = builder->CreateICmpSLT(iVal, lengthVal, "foreach_cmp");
        builder->CreateCondBr(cmpVal, bodyBB, endBB);
    } else if (isIterator) {
        llvm::Function* atEndFn = findMethodOverload(iterTypeName, "_atEnd", {});
        llvm::Value* atEnd = emitMethodCall(atEndFn, iterObjAlloc, {}, "_atEnd");
        builder->CreateCondBr(atEnd, endBB, bodyBB);
    }

    builder->SetInsertPoint(bodyBB);
    currentBreakBB = endBB;
    llvm::Value* elemVal = nullptr;
    if (isArray && arrayAlloc) {
        llvm::Value* iVal = builder->CreateLoad(builder->getInt32Ty(), iterAlloc, iterName);
        llvm::Type* allocTy = getPointeeType(collName);

        if (allocTy->isArrayTy()) {
            std::vector<llvm::Value*> indices = {builder->getInt32(0), iVal};
            llvm::Value* elemPtr = builder->CreateInBoundsGEP(allocTy, arrayAlloc, indices, "elem_ptr");
            elemVal = builder->CreateLoad(elemTy, elemPtr, "elem");
        } else if (allocTy->isPointerTy() && arrayElemTy) {
            llvm::Value* heapPtr = builder->CreateLoad(allocTy, arrayAlloc, "heap_ptr");
            llvm::Value* elemPtr = builder->CreateGEP(arrayElemTy, heapPtr, iVal, "heap_elem_ptr");

            elemVal = builder->CreateLoad(elemTy, elemPtr, "elem");
        }
        builder->CreateStore(elemVal, elemAlloc);
        emitStmt(foreach->body);
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(incBB);
        }
        builder->SetInsertPoint(incBB);
        llvm::Value* iVal2 = builder->CreateLoad(builder->getInt32Ty(), iterAlloc, iterName);
        llvm::Value* incVal = builder->CreateAdd(iVal2, builder->getInt32(1), "i_inc");
        builder->CreateStore(incVal, iterAlloc);
        builder->CreateBr(condBB);
    } else if (isIterator) {
        iterLoaded = builder->CreateLoad(iterLLVMTy, iterObjAlloc);
        llvm::Function* nextFn = findMethodOverload(iterTypeName, "_next", {});
        llvm::Value* cur = emitMethodCall(nextFn, iterObjAlloc, {}, "_next");
        builder->CreateStore(cur, elemAlloc);
        emitStmt(foreach->body);
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerDepth + 1);
            builder->CreateBr(condBB);
        }
    }
    builder->SetInsertPoint(endBB);
    exitScope();
    loopStack.pop_back();
    locals.erase(iterName);
    locals.erase(elemName);
    currentBreakBB = savedBreakBB;
    currentContinueBB = savedContinueBB;
    return;
}
void LLVMCompiler::emitTryCatch(TryCatchNode* trycatch) {
    if (insideTry()) {
        cg_error(get_pos(trycatch), "You cannot have nested try catch blocks. Why would you need them", "QC-TC01");
        return;
    }
    size_t outerScope = defersStack.size();
    enterScope();
    auto* tryBB = llvm::BasicBlock::Create(context, "try.start", currentFunction);
    auto* landingPadBB = llvm::BasicBlock::Create(context, "catch.landing", currentFunction);
    auto* endBB = llvm::BasicBlock::Create(context, "try.end", currentFunction);
    std::vector<llvm::BasicBlock*> catchBlocks;
    catchBlocks.reserve(trycatch->catch_bodys.size());
    EHScope thisScope{
        .landingPad = landingPadBB,
        .continuation = endBB,
        .handlers = {},
        .deferDepth = outerScope + 1,
    };
    for (size_t i = 0; i < trycatch->catch_bodys.size(); ++i) {
        auto* catchBB = llvm::BasicBlock::Create(context, "catch." + std::to_string(i), currentFunction);
        catchBlocks.push_back(catchBB);
        thisScope.handlers.push_back({
            .body = trycatch->catch_bodys[i],
            .block = catchBB,
        });
    }
    ehScopes.push_back(std::move(thisScope));
    builder->CreateBr(tryBB);
    builder->SetInsertPoint(tryBB);
    enterScope();
    emitStmt(trycatch->try_body);
    if (!builder->GetInsertBlock()->getTerminator()) {
        emitDefersDownTo(outerScope + 2);
        builder->CreateBr(endBB);
    }
    exitScope();
    builder->SetInsertPoint(landingPadBB);
    auto* exceptionType = llvm::StructType::get(context, {builder->getPtrTy(), builder->getInt32Ty()});
    std::vector<EHHandler> visibleHandlers;
    for (auto scopeIt = ehScopes.rbegin(); scopeIt != ehScopes.rend(); ++scopeIt) {
        for (const auto& handler : scopeIt->handlers) { visibleHandlers.push_back(handler); }
    }
    auto* lp = builder->CreateLandingPad(exceptionType, visibleHandlers.size(), "qc.exception");
    for (const auto& handler : visibleHandlers) {
        const auto& c = handler.body;
        llvm::Constant* typeInfo = c.var_type == "..." ? llvm::ConstantPointerNull::get(builder->getPtrTy()) : getStringConstant(c.var_type);
        lp->addClause(typeInfo);
    }
    if (!currentFunction->hasPersonalityFn()) { currentFunction->setPersonalityFn(module->getFunction("__qc_personality")); }
    auto* exception = builder->CreateExtractValue(lp, 0, "exception");
    auto* selector = builder->CreateExtractValue(lp, 1, "selector");
    auto* noMatchBB = llvm::BasicBlock::Create(context, "catch.no_match", currentFunction);
    auto* sw = builder->CreateSwitch(selector, noMatchBB, visibleHandlers.size());
    for (size_t i = 0; i < visibleHandlers.size(); ++i) {
        sw->addCase(llvm::ConstantInt::get(builder->getInt32Ty(), i + 1), visibleHandlers[i].block);
    }
    EHScope savedScope = std::move(ehScopes.back());
    ehScopes.pop_back();
    for (size_t i = 0; i < savedScope.handlers.size(); ++i) {
        const auto& c = savedScope.handlers[i].body;
        auto* catchBB = savedScope.handlers[i].block;
        builder->SetInsertPoint(catchBB);
        enterScope();
        if (!c.var_name.empty()) {
            std::string name = getCurrentNamespace().empty() ? c.var_name : getCurrentNamespace() + c.var_name;
            llvm::Value* payload = builder->CreateCall(module->getFunction("__qc_exception_get_value"), {exception});
            llvm::Type* catchLLVMType = llvmTypeFor(c.var_type);
            llvm::Value* catchValue = nullptr;
            if (catchLLVMType->isFloatingPointTy()) {
                llvm::Value* int64Val = builder->CreatePtrToInt(payload, builder->getInt64Ty());
                if (catchLLVMType->isFloatTy()) {
                    llvm::Value* int32Val = builder->CreateTrunc(int64Val, builder->getInt32Ty());
                    catchValue = builder->CreateBitCast(int32Val, builder->getFloatTy());
                } else {
                    catchValue = builder->CreateBitCast(int64Val, builder->getDoubleTy());
                }
            } else if (catchLLVMType->isIntegerTy()) {
                llvm::Value* int64Val = builder->CreatePtrToInt(payload, builder->getInt64Ty());
                catchValue = builder->CreateTruncOrBitCast(int64Val, catchLLVMType);

            } else {
                catchValue = payload;
            }
            auto* alloc = createEntryAlloca(name, catchLLVMType);
            builder->CreateStore(catchValue, alloc);
            locals[name] = alloc;
            varTypes[name] = resolveTypeName(c.var_type, false);
        }
        emitStmt(c.body);
        if (c.body->statements.empty()) {
            warn("empty-catch", get_pos(trycatch), "catch body is empty and silently consumes thrown errors", "QC-W021");
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            emitDefersDownTo(outerScope + 2);
            builder->CreateBr(endBB);
        }
        exitScope();
    }
    builder->SetInsertPoint(noMatchBB);
    builder->CreateResume(lp);
    exitScope();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitMatch(MatchNode* match_node) {
    size_t outerDepth = defersStack.size();
    enterScope();
    llvm::Value* matchVal = emitExpr(match_node->value);
    if (!matchVal) {
        exitScope();
        return;
    }
    llvm::Type* matchTy = matchVal->getType();
    std::string enumName = "";
    bool canUseMatch = matchTy->isIntegerTy() || ((match_node->is_enum || match_node->sections.empty() ||
                                                   (match_node->sections.size() == 1 && match_node->sections[0].is_default)) &&
                                                  isEnumType(matchTy, &enumName));
    if (!enumName.empty()) {
        if (std::ranges::none_of(match_node->sections, [](const MatchNode::Section& node) { return node.is_default; }) &&
            match_node->sections.size() != userTypes.at(baseTypeName(resolveTypeName(enumName))).enumEntries.size()) {
            cg_error(get_pos(match_node), "match must have a case for every enum member, or have a default branch", "QC-MCH1");
            return;
        }
    } else {
        if (!(std::ranges::any_of(match_node->sections, [](const MatchNode::Section& node) { return node.is_default; }))) {
            cg_error(get_pos(match_node), "match must have a default branch for non-enum values.", "QC-MCH1");
            cg_help(get_pos(match_node), "if you know it is impossible for any other value to be here, use `default => unreachable;`");
            return;
        }
    }
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(context, "match.end", currentFunction);
    std::vector<llvm::BasicBlock*> sectionBlocks;
    llvm::BasicBlock* defaultBB = nullptr;
    for (auto& section : match_node->sections) {
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "match.case", currentFunction);
        sectionBlocks.push_back(bb);
        if (section.is_default) { defaultBB = bb; }
    }
    if (!defaultBB) { defaultBB = endBB; }
    if (canUseMatch) {
        if (!match_node->is_enum) {
            llvm::SwitchInst* matchInst = builder->CreateSwitch(matchVal, defaultBB, match_node->sections.size());
            for (size_t i = 0; i < match_node->sections.size(); i++) {
                auto& section = match_node->sections[i];
                if (section.is_default) continue;
                llvm::Value* caseVal = emitExpr(section.normal_case.expr);
                if (!caseVal) {
                    cg_error(get_pos(section.normal_case.expr), "Failed to emit case value", "QC-S278");
                    return;
                }
                if (auto constInt = llvm::dyn_cast<llvm::ConstantInt>(caseVal)) { matchInst->addCase(constInt, sectionBlocks[i]); }
            }
        } else {
            llvm::SwitchInst* matchInst = builder->CreateSwitch(builder->CreateExtractValue(matchVal, {0}), defaultBB, match_node->sections.size());
            for (size_t i = 0; i < match_node->sections.size(); i++) {
                auto& section = match_node->sections[i];
                if (section.is_default) continue;
                std::string member_name = section.enum_case.first;
                if (enumMemberInfo.find(member_name) == enumMemberInfo.end()) {
                    cg_error(get_pos(match_node), "unknown enum member: " + member_name, "QC-MCH3");
                    return;
                }
                llvm::Value* caseVal = llvm::ConstantInt::get(llvm::dyn_cast<llvm::StructType>(matchTy)->getElementType(0),
                                                              std::stoull(enumMemberInfo[member_name].first));
                if (!caseVal) {
                    cg_error(get_pos(section.normal_case.expr), "Failed to emit case value", "QC-S278");
                    return;
                }
                if (auto constInt = llvm::dyn_cast<llvm::ConstantInt>(caseVal)) { matchInst->addCase(constInt, sectionBlocks[i]); }
            }
        }
    } else {
        cg_error(get_pos(match_node), "cannot use match on non enum non integer types", "QC-MCH2");
        return;
    }
    llvm::BasicBlock* oldBreakBB = currentBreakBB;
    currentBreakBB = endBB;
    for (size_t i = 0; i < match_node->sections.size(); i++) {
        builder->SetInsertPoint(sectionBlocks[i]);
        enterScope();

        for (auto& stmt : match_node->sections[i].body->statements) {
            if (match_node->is_enum) {
                auto& section = match_node->sections[i].enum_case;
                for (unsigned int i = 0; i < section.second.size(); i++) {
                    std::string fullName = getCurrentNamespace().empty() ? section.second[i] : getCurrentNamespace() + "::" + section.second[i];
                    std::vector<std::string> types = enumMemberInfo[section.first].second;
                    varTypes[fullName] = types[i];
                    llvm::Value* raw = builder->CreateExtractValue(matchVal, {i + 1});
                    llvm::AllocaInst* rawAlloc = builder->CreateAlloca(raw->getType(), nullptr, "match_raw");
                    builder->CreateStore(raw, rawAlloc);
                    llvm::AllocaInst* alloc = createEntryAlloca(fullName, llvmTypeFor(types[i]));
                    uint64_t size = module->getDataLayout().getTypeStoreSize(llvmTypeFor(types[i]));
                    builder->CreateMemCpyInline(alloc, llvm::MaybeAlign(), rawAlloc, llvm::MaybeAlign(),
                                                llvm::ConstantInt::get(builder->getInt64Ty(), size));
                    locals[fullName] = alloc;
                    varTypes[fullName] = types[i];
                    volatileVars[fullName] = false;
                }
            }
            emitStmt(stmt);
        }
        exitScope();
        if (!builder->GetInsertBlock()->getTerminator()) {
            if (i + 1 < sectionBlocks.size()) {
                builder->CreateBr(endBB);
            } else {
                emitDefersDownTo(outerDepth + 1);
                builder->CreateBr(endBB);
            }
        }
    }
    currentBreakBB = oldBreakBB;
    exitScope();
    builder->SetInsertPoint(endBB);
}
void LLVMCompiler::emitStmt(AnyNode node) {
    if (builder->GetInsertBlock()->getTerminator()) {
        warn("unreachable-code", get_pos(node), "attempted to emit into terminated basic block (unreachable code)", "QC-W001");
        return;
    }
    if (auto mret = safe_get<MultiReturnNode>(node)) {
        emitMultiRet(mret);
    } else if (auto ret = safe_get<ReturnNode>(node)) {
        emitRet(ret);
    } else if (auto mv = safe_get<MultiVarDeclNode>(node)) {
        emitMultiVar(mv);
    } else if (auto if_node = safe_get<IfNode>(node)) {
        emitIf(if_node);
    } else if (auto while_node = safe_get<WhileNode>(node)) {
        emitWhile(while_node);
    } else if (std::holds_alternative<BreakNode*>(node)) {
        if (currentBreakBB) {
            if (!loopStack.empty()) emitDefersDownTo(loopStack.back());
            builder->CreateBr(currentBreakBB);
        } else {
            cg_error(get_pos(node), "break outside of loop/switch", "QC-S269");
        }
    } else if (std::holds_alternative<UnreachableNode*>(node)) {
        emitDefersDownTo(defersStack.size());
        builder->CreateUnreachable();
    } else if (std::holds_alternative<ContinueNode*>(node)) {
        if (currentContinueBB) {
            if (!loopStack.empty()) emitDefersDownTo(loopStack.back());
            builder->CreateBr(currentContinueBB);
        } else {
            cg_error(get_pos(node), "continue outside of loop", "QC-S270");
        }
    } else if (auto for_node = safe_get<ForNode>(node)) {
        emitFor(for_node);
    } else if (auto switch_node = safe_get<SwitchNode>(node)) {
        emitSwitch(switch_node);
    } else if (auto qif_node = safe_get<QIfNode>(node)) {
        emitQIf(qif_node);
    } else if (auto qsw = safe_get<QSwitchNode>(node)) {
        emitQSwitch(qsw);
    } else if (auto arrDecl = safe_get<ArrayDeclNode>(node)) {
        emitArrDecl(arrDecl);
    } else if (auto arrAssign = safe_get<ArrayAssignNode>(node)) {
        emitArrAssign(arrAssign);
    } else if (auto foreach = safe_get<ForeachNode>(node)) {
        emitForeach(foreach);
    } else if (auto stmts = safe_get<StatementsNode>(node)) {
        if (stmts->is_scoped) enterScope();
        for (auto& stmt : stmts->statements) { emitStmt(stmt); }
        if (stmts->is_scoped) exitScope();
        return;
    } else if (auto ns = safe_get<NamespaceNode>(node)) {
        namespaceStack.push_back(ns->name);
        for (auto& decl : ns->body) { emitStmt(decl); }
        namespaceStack.pop_back();

        return;
    } else if (auto trycatch = safe_get<TryCatchNode>(node)) {
        emitTryCatch(trycatch);
    } else if (auto defer = safe_get<DeferNode>(node)) {
        defers.push_back(defer->block);
        return;
    } else if (auto match_node = safe_get<MatchNode>(node)) {
        emitMatch(match_node);
    } else {
        emitExpr(node);
        return;
    }
}
std::pair<bool, int> LLVMCompiler::checkJagged(AnyNode& node) {
    if (auto lit = std::get_if<ArrayLiteralNode*>(&node)) {
        if ((*lit)->elements.empty()) return {false, 0};

        auto [firstJagged, firstDepth] = checkJagged((*lit)->elements[0]);
        size_t firstSize = 0;
        if (auto firstLit = safe_get<ArrayLiteralNode>((*lit)->elements[0])) { firstSize = firstLit->elements.size(); }

        bool isJagged = firstJagged;
        int maxDepth = firstDepth;

        for (auto& elem : (*lit)->elements) {
            auto [elemJagged, elemDepth] = checkJagged(elem);
            maxDepth = std::max(maxDepth, elemDepth);
            isJagged = isJagged || elemJagged;

            if (auto elemLit = safe_get<ArrayLiteralNode>(elem)) {
                if (elemLit->elements.size() != firstSize) { isJagged = true; }
            }
        }

        return {isJagged, maxDepth + 1};
    }
    return {false, 0};
}
std::vector<CTError> LLVMCompiler::compile(
    StatementsNode* root, std::unordered_map<std::string, FunctionSignature> visibleFunctionSignatures,
    std::unordered_map<std::string, FuncDefNode*> visibleFunctionDefs, std::unordered_map<std::string, std::pair<int, int>> visibleJaggedArrays,
    std::unordered_map<std::string, std::string> visibleArrayTypeStrings, std::unordered_map<std::string, int> visibleArrayLengths,
    std::unordered_map<std::string, std::string> visibleVarTypes, std::unordered_map<std::string, llvm::AllocaInst*> visibleRuntimeArraySizes,
    std::unordered_map<std::string, llvm::FunctionType*> visibleLambdaTypes, std::map<std::string, llvm::Function*> visibleSpecializedFunctions,
    std::unordered_map<std::string, llvm::GlobalVariable*> visibleGlobals) {
    this->volatileVarsStack = {{}};
    this->functionSignatures = visibleFunctionSignatures;
    this->functionDefs = visibleFunctionDefs;
    this->globals = visibleGlobals;
    this->varTypesStack = {visibleVarTypes};
    this->arrayTypeStringsStack = {visibleArrayTypeStrings};
    this->arrayLengthsStack = {visibleArrayLengths};
    this->runtimeArraySizes = visibleRuntimeArraySizes;
    this->lambdaTypes = visibleLambdaTypes;
    this->localsStack = {{}};
    for (auto& [className, info] : userTypes) {
        if (info.kind != UserTypeKind::Class) continue;
        for (size_t methodIdx = 0; methodIdx < info.classMethods.size(); methodIdx++) {
            auto& method = info.classMethods[methodIdx];
            if (!method.generics.empty() && !method.is_static) { genericMethodIndices[className].push_back(methodIdx); }
        }
    }
    createUserTypes();
    auto createGlobal = [&](VarAssignNode* va) {
        const bool foreign = va->is_foreign;
        std::string name = foreign || getCurrentNamespace().empty() ? va->var_name_tok.value : getCurrentNamespace() + "::" + va->var_name_tok.value;
        llvm::Type* ty = llvmTypeFor(va->type_tok.value);
        llvm::GlobalVariable* gv = module->getGlobalVariable(name);
        if (!gv) {
            llvm::Constant* initializer = foreign ? nullptr : llvm::Constant::getNullValue(ty);
            gv = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage, initializer, name);
        }
        globals[name] = gv;
        varTypes[name] = va->type_tok.value;
    };
    std::function<void(NamespaceNode&)> createGlobals = [&](NamespaceNode& ns) {
        namespaceStack.push_back(ns.name);
        for (auto& decl : ns.body) {
            if (auto va = std::get_if<VarAssignNode*>(&decl)) {
                createGlobal(*va);
            } else if (auto nested = std::get_if<NamespaceNode*>(&decl)) {
                createGlobals(**nested);
            }
        }
        namespaceStack.pop_back();
    };
    if (root)
        for (auto& stmt : root->statements) {
            if (auto ns = std::get_if<NamespaceNode*>(&stmt)) {
                createGlobals(**ns);
            } else if (auto va = std::get_if<VarAssignNode*>(&stmt)) {
                createGlobal(*va);
            }
        }
    std::function<void(NamespaceNode&)> scanGenericFunctions = [&](NamespaceNode& ns) {
        namespaceStack.push_back(ns.name);

        for (auto& decl : ns.body) {
            if (auto fn = safe_get<FuncDefNode>(decl)) {
                if (fn->name_tok.has_value()) {
                    std::string funcName = fn->name_tok.value().value;
                    std::string fullName = getCurrentNamespace().empty() ? funcName : getCurrentNamespace() + "::" + funcName;
                    functionDefs[fullName] = fn;
                }
            } else if (auto nested = std::get_if<NamespaceNode*>(&decl)) {
                scanGenericFunctions(**nested);
            }
        }

        namespaceStack.pop_back();
    };

    if (root)
        for (auto& stmt : root->statements) {
            if (auto ns = std::get_if<NamespaceNode*>(&stmt)) {
                scanGenericFunctions(**ns);
            } else if (std::holds_alternative<FuncDefNode*>(stmt)) {
                auto fnPtr = std::get<FuncDefNode*>(stmt);
                if (!fnPtr->name_tok.has_value()) continue;
                std::string funcName = fnPtr->name_tok.value().value;
                if (funcName == entrypointName && !this->is_main) { continue; }
                functionDefs[funcName] = fnPtr;
            }
        }
    std::function<void(NamespaceNode&)> compileNamespaceFunctions = [&](NamespaceNode& ns) {
        namespaceStack.push_back(ns.name);

        for (auto& decl : ns.body) {
            if (auto fn = safe_get<FuncDefNode>(decl)) {
                if (fn->name_tok.has_value()) {
                    if (fn->name_tok.value().value == entrypointName && !this->is_main) { continue; }

                    if (fn->generics.empty()) { emitFuncDef(*fn); }
                }
            } else if (auto va = std::get_if<VarAssignNode*>(&decl)) {
                emitExpr(decl);
            } else if (auto nested = std::get_if<NamespaceNode*>(&decl)) {
                compileNamespaceFunctions(**nested);
            }
        }

        namespaceStack.pop_back();
    };

    if (root)
        for (auto& stmt : root->statements) {
            if (auto ns = std::get_if<NamespaceNode*>(&stmt)) {
                compileNamespaceFunctions(**ns);
            } else if (auto va = std::get_if<VarAssignNode*>(&stmt)) {
                emitExpr(stmt);
            } else if (std::holds_alternative<FuncDefNode*>(stmt)) {
                auto fnPtr = std::get<FuncDefNode*>(stmt);
                if (!fnPtr->name_tok.has_value()) continue;
                if (fnPtr->name_tok.value().value == entrypointName && !this->is_main) { continue; }
                if (fnPtr->generics.empty()) { emitFuncDef(*fnPtr); }
            }
        }
    for (auto& [className, info] : userTypes) {
        if (info.kind != UserTypeKind::Class || !info.generics.empty()) continue;
        for (auto& method : info.classMethods) {
            if (!method.is_static || !method.generics.empty()) continue;
            std::string mangledName = className + "::" + method.name_tok.value;
            if (functionDefs.count(mangledName)) { emitFuncDef(*functionDefs[mangledName]); }
        }
    }
    for (auto& [className, info] : userTypes) {
        if (info.kind != UserTypeKind::Class || !info.generics.empty()) continue;
        auto oldNamespaceStack = namespaceStack;
        namespaceStack.clear();
        if (!info.namespace_path.empty()) {
            size_t start = 0;
            size_t pos;
            while ((pos = info.namespace_path.find("::", start)) != std::string::npos) {
                namespaceStack.push_back(info.namespace_path.substr(start, pos - start));
                start = pos + 2;
            }
            namespaceStack.push_back(info.namespace_path.substr(start));
        }
        for (size_t methodIdx = 0; methodIdx < info.classMethods.size(); methodIdx++) {
            auto& method = info.classMethods[methodIdx];
            if (std::find(genericMethodIndices[className].begin(), genericMethodIndices[className].end(), methodIdx) !=
                genericMethodIndices[className].end()) {
                continue;
            }
            if (!method.generics.empty()) continue;
            llvm::Function* fn = nullptr;
            auto& overloads = classMethods[className][method.name_tok.value];
            bool isStatic = method.is_static;
            size_t expectedParamOffset = isStatic ? 0 : 1;
            for (auto* overload : overloads) {
                if (overload->arg_size() - expectedParamOffset == method.params.size()) {
                    bool matches = true;
                    for (size_t i = 0; i < method.params.size(); i++) {
                        auto& param = method.params[i];
                        llvm::Type* expectedType;
                        if (param.signature.has_value()) {
                            expectedType = llvm::PointerType::get(context, 0);
                        } else {
                            std::string resolvedType = resolveTypeName(param.type.value, false);
                            expectedType = llvmTypeFor(resolvedType);
                        }
                        llvm::Type* actualType = overload->getFunctionType()->getParamType(i + expectedParamOffset);
                        if (expectedType != actualType && param.type.value != "...") {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        fn = overload;
                        break;
                    }
                }
            }
            if (!fn || !fn->empty()) continue;
            llvm::Function* currentTarget = fn;
            std::string methodName = fn->getName().str();
            if (!method.modifiers.empty()) {
                std::string implName = "_impl_" + methodName;
                currentTarget = llvm::Function::Create(fn->getFunctionType(), llvm::Function::InternalLinkage, implName, module);
            }
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", currentTarget);
            builder->SetInsertPoint(entry);
            auto oldThis = currentThis;
            auto oldClassName = currentClassName;
            auto oldFunction = currentFunction;
            enterScope();
            if (!isStatic) {
                currentThis = currentTarget->getArg(0);
                varTypes["this"] = className + "*";
            } else {
                currentThis = nullptr;
            }
            currentClassName = className;
            currentFunction = currentTarget;
            for (size_t i = 0; i < method.params.size(); i++) {
                auto& param = method.params[i];
                llvm::Type* paramTy;
                std::string typeDescriptor;
                if (param.signature.has_value()) {
                    paramTy = llvm::PointerType::get(context, 0);
                    typeDescriptor = "fn";
                    lambdaTypes[param.name.value] = llvmFuncTypeFor(param.signature->return_types, param.signature->params);
                } else {
                    typeDescriptor = resolveTypeName(param.type.value, false);
                    paramTy = llvmTypeFor(typeDescriptor);
                }
                if (!paramTy) {
                    cg_error(param.name.pos, "could not resolve type '" + typeDescriptor + "' for parameter '" + param.name.value + "'", "QC-T999");
                    return errors;
                }
                llvm::AllocaInst* alloc = createEntryAlloca(param.name.value, paramTy);
                llvm::Value* argVal = currentTarget->getArg(i + expectedParamOffset);
                builder->CreateStore(argVal, alloc);
                locals[param.name.value] = alloc;
                varTypes[param.name.value] = typeDescriptor;
            }
            if (method.is_constructor && !info.baseClassName.empty() && method.parentConstructorCall) {
                if (auto varAccess = std::get_if<VarAccessNode*>(&method.parentConstructorCall->node_to_call)) {
                    std::string callName = (*varAccess)->var_name_tok.value;
                    if (callName == info.baseClassName) {
                        std::vector<llvm::Value*> parentArgs;
                        for (auto& argNode : method.parentConstructorCall->arg_nodes) {
                            llvm::Value* arg = emitExpr(argNode);
                            if (!arg) continue;
                            parentArgs.push_back(arg);
                        }
                        llvm::Function* parentCtor = findMethodOverload(info.baseClassName, info.baseClassName, parentArgs);
                        if (parentCtor) {
                            std::vector<llvm::Value*> allArgs = {currentThis};
                            allArgs.insert(allArgs.end(), parentArgs.begin(), parentArgs.end());
                            if (insideTry()) {
                                auto contBB = llvm::BasicBlock::Create(context, "invoke.cont." + std::to_string(invokeCounter++), currentFunction);
                                llvm::InvokeInst* invoke = builder->CreateInvoke(parentCtor, contBB, currentLandingPad(), allArgs);
                                builder->SetInsertPoint(contBB);
                            } else {
                                builder->CreateCall(parentCtor, allArgs);
                            }
                        } else {
                            cg_error(get_pos(method.parentConstructorCall), "parent class '" + info.baseClassName + "' has no matching constructor",
                                     "QC-S275");
                            addConstructorNotes(info.baseClassName, parentArgs, get_pos(method.parentConstructorCall));
                        }
                    }
                }
            }
            if (method.body) { emitStmt(method.body); }
            if (!builder->GetInsertBlock()->getTerminator()) {
                if (currentTarget->getReturnType()->isVoidTy()) {
                    builder->CreateRetVoid();
                } else {
                    builder->CreateRet(llvm::Constant::getNullValue(currentTarget->getReturnType()));
                }
            }
            if (!method.modifiers.empty()) {
                llvm::FunctionType* fTy = fn->getFunctionType();
                for (int i = (int)method.modifiers.size() - 1; i >= 0; --i) {
                    Token modTok = method.modifiers[i];
                    if (!modifiers.count(modTok.value)) {
                        cg_error(modTok.pos, "unknown modifier '" + modTok.value + "'", "QC-S112");
                        continue;
                    }
                    ModifierInfo& modInfo = modifiers[modTok.value];
                    bool isOutermost = (i == 0);
                    std::string layerName = isOutermost ? methodName : ("_mod_" + std::to_string(i) + "_" + methodName);
                    auto linkage = isOutermost ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
                    std::string proceedName = "_proceed_" + std::to_string(i) + "_" + methodName;
                    llvm::Function* proceedFunc = synthesizeProceed(proceedName, currentTarget, modInfo.onReturn,
                                                                    *funcDefFromClassMethod(method, className, "_"));
                    llvm::Function* layerFunc = isOutermost ? fn : module->getFunction(layerName);
                    if (!layerFunc) { layerFunc = llvm::Function::Create(fTy, linkage, layerName, module); }
                    auto savedDefers = defersStack;
                    defersStack.clear();
                    enterScope();
                    llvm::BasicBlock* savedInsertBlock = builder->GetInsertBlock();
                    auto* entryBB = llvm::BasicBlock::Create(context, "entry", layerFunc);
                    builder->SetInsertPoint(entryBB);
                    auto* prevFunction = currentFunction;
                    auto prevThis = currentThis;
                    currentFunction = layerFunc;
                    std::vector<llvm::Value*> forwardArgs;
                    if (!isStatic) {
                        currentThis = layerFunc->getArg(0);
                        varTypes["this"] = className + "*";
                        forwardArgs.push_back(currentThis);
                    } else {
                        currentThis = nullptr;
                    }
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
                        if (fTy->getReturnType()->isVoidTy()) {
                            builder->CreateCall(proceedFunc, forwardArgs);
                            builder->CreateRetVoid();
                        } else {
                            llvm::Value* retVal = builder->CreateCall(proceedFunc, forwardArgs);
                            builder->CreateRet(retVal);
                        }
                    }
                    if (!builder->GetInsertBlock()->getTerminator()) {
                        if (fTy->getReturnType()->isVoidTy()) {
                            builder->CreateRetVoid();
                        } else {
                            builder->CreateRet(llvm::ConstantAggregateZero::get(fTy->getReturnType()));
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
            currentThis = oldThis;
            currentClassName = oldClassName;
            currentFunction = oldFunction;
            exitScope();
        }
        namespaceStack = oldNamespaceStack;
    }
    currentFunction = nullptr;
    if (this->is_main) {
        if (!no_main) {
            llvm::Function* userEntry = module->getFunction(entrypointName);
            if (userEntry) {
                userEntry->setName("__user_entry");
                llvm::FunctionType* mainTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
                if (main_type != MainType::NA) {
                    mainTy = llvm::FunctionType::get(builder->getInt32Ty(), {builder->getInt32Ty(), builder->getPtrTy()}, false);
                }
                llvm::Function* realMain = llvm::Function::Create(mainTy, llvm::Function::ExternalLinkage, "main", module);
                llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", realMain);
                builder->SetInsertPoint(entry);
                currentFunction = realMain;
                std::vector<llvm::Value*> user_entry_args;
                for (llvm::Argument& arg : realMain->args()) { user_entry_args.push_back(&arg); }
                if (main_type == MainType::RT_ARRAY) {
                    llvm::Value* argc = realMain->getArg(0);
                    llvm::AllocaInst* argcSlot = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "argc.addr");
                    builder->CreateStore(argc, argcSlot);
                    user_entry_args = {realMain->getArg(1)};
                    arrayTypeStrings[functionDefs[entrypointName]->params.back().name.value] = "string";
                    runtimeArraySizes[functionDefs[entrypointName]->params.back().name.value] = argcSlot;
                }
                llvm::Value* result = builder->CreateCall(userEntry, user_entry_args, "entry_result");
                builder->CreateRet(result);
            } else {
                cg_error(Position(Position::INVALID_FILE_ID, 0, 0, 0), "entrypoint function '" + entrypointName + "' not defined", "QC-S276");
            }
        }
    }
    return errors;
}
#endif
