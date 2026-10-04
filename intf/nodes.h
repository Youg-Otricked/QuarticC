#ifndef NODE_H
#define NODE_H
#include <variant>
#include <string>
#include <list>
#include "token.h"
#include "errors.h"
class NumberNode;
class BinOpNode;
class UnaryOpNode;
class StatementsNode;
class VarAssignNode;
class VarAccessNode;
class StringNode;
class CharNode;
class AssignExprNode;
class BoolNode;
class IfNode;
class SwitchNode;
class BreakNode;
class UnreachableNode;
class WhileNode;
class ForNode;
class CallNode;
class ContinueNode;
class FuncDefNode;
class QInNode;
class ReturnNode;
class MultiReturnNode;
class MultiVarDeclNode;
class ArrayDeclNode;
class ArrayLiteralNode;
class ArrayAccessNode;
class MethodCallNode;
class PropertyAccessNode;
class SpreadNode;
class NamespaceNode;
class ForeachNode;
class QBoolNode;
class QIfNode;
class MatchNode;
class QSwitchNode;
class ArrayAssignNode;
class FieldAssignNode;
class MapLiteralNode;
class TryCatchNode;
class RefVarDeclNode;
class NullptrNode;
class TypeValueNode;
class DeferNode;
class ModifierNode;
using AnyNode = std::variant<std::monostate, NumberNode, StringNode, CharNode, BoolNode, QInNode, QBoolNode, RefVarDeclNode*, NullptrNode, BinOpNode*,
                             UnaryOpNode*, VarAccessNode*, VarAssignNode*, AssignExprNode*, IfNode*, QIfNode*, StatementsNode*, SwitchNode*,
                             QSwitchNode*, BreakNode*, UnreachableNode*, WhileNode*, ForNode*, ContinueNode*, CallNode*, FuncDefNode*, ReturnNode*,
                             MultiReturnNode*, MultiVarDeclNode*, ArrayDeclNode*, ArrayLiteralNode*, ArrayAccessNode*, MethodCallNode*,
                             PropertyAccessNode*, SpreadNode*, ForeachNode*, ArrayAssignNode*, FieldAssignNode*, MapLiteralNode*, NamespaceNode*,
                             TryCatchNode*, TypeValueNode, DeferNode*, ModifierNode*, MatchNode*>;
struct GenericType {
    std::string name;
    bool isVariadic = false;
    bool isNonType = false;
    std::string nonTypeKind;
    std::string constraint;
    std::string defaultValue;
    bool negated = false;
    std::vector<std::string> subconstraints;
};

class Parameter;
std::string printAny(const AnyNode& node);
Position get_pos(AnyNode node);
class QInNode {
  public:
    QInNode() {}

    std::string print() const { return "std::qin"; }
};
class CharNode {
  public:
    Token tok;
    CharNode(Token t) : tok(t) {}
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class NumberNode {
  public:
    Token tok;
    NumberNode(Token tok);
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class StringNode {
  public:
    Token tok;
    StringNode(Token tok);
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class BoolNode {
  public:
    Token tok;
    BoolNode(Token tok);
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class QBoolNode {
  public:
    Token tok;
    QBoolNode(Token tok);
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class NullptrNode {
  public:
    Position pos;
    NullptrNode(Position p) : pos(p) {}
    Position getPos() { return this->pos; }
    std::string print() const { return "nullptr"; }
};
class TypeValueNode {
  public:
    Token tok;
    TypeValueNode(Token tok);
    Position getPos() { return this->tok.pos; }
    std::string print() const;
};
class RefVarDeclNode {
  public:
    Token type_tok;
    Token var_name_tok;
    AnyNode target;
    Position pos;
    Position getPos() { return this->pos; }
    RefVarDeclNode(Token type, Token name, AnyNode target, Position p) : var_name_tok(name), target(target), pos(p) {
        type.value.erase(type.value.find_last_not_of('&') + 1);
        this->type_tok = type;
    }
    std::string print() const { return this->type_tok.value; }
};
class StatementsNode {
  public:
    std::vector<AnyNode> statements;
    bool is_block = false;
    bool is_scoped = false;
    Position getPos() { return statements.empty() ? Position(-1, 0, 0, 0) : get_pos(statements[0]); }
    StatementsNode(std::vector<AnyNode> stmts, bool is_block = false, bool is_scoped = false)
        : statements(stmts), is_block(is_block), is_scoped(is_scoped) {}
    std::string print() const;
};
class Parameter {
  public:
    Token type;
    Token name;
    std::optional<AnyNode> default_value;
    class FunctionSignature {
      public:
        std::vector<Token> return_types;
        std::vector<Parameter> params;
        std::string print() const {
            std::string res = "fn(";
            for (size_t i = 0; i < params.size(); i++) {
                res += params[i].toString();
                if (i + 1 != params.size()) res += ", ";
            }
            res += ") -> ";
            if (return_types.empty())
                res += "void";
            else
                for (size_t i = 0; i < return_types.size(); i++) {
                    res += return_types[i].value;
                    if (i + 1 != return_types.size()) res += ", ";
                }
            return res;
        }
    };
    std::string toString() const {
        std::string res = isVolatile ? "volatile " : "";
        return res + (signature.has_value() ? signature.value().print() : type.value + " " + name.value) +
               (default_value.has_value() ? (" = " + printAny(default_value.value())) : "");
    }
    std::optional<FunctionSignature> signature;
    bool isVolatile = false;
};
struct ParamTypeInfo {
    Token type;
    struct FunctionSignature {
        std::vector<Token> return_types;
        std::vector<ParamTypeInfo> params;
    };
    Token name;
    std::optional<FunctionSignature> signature;
    bool isVolatile = false;
};
struct StructField {
    std::string name;
    std::string type;
};
struct EnumEntry {
    std::string memberName;
    std::string value;
    std::vector<std::string> tags;
};
struct ClassField {
    std::string name;
    std::string type;
    std::string access;
    bool isStatic = false;
    AnyNode defaultValue = std::monostate{};
};
class ClassMethodInfo {
  public:
    Token name_tok;
    std::vector<Parameter> params;
    std::vector<Token> return_types;
    StatementsNode* body;
    bool is_constructor = false;
    CallNode* parentConstructorCall = nullptr;
    std::string access;
    bool is_final = false;
    bool is_volatile = false;
    bool is_static = false;
    std::vector<Token> modifiers;
    std::vector<GenericType> generics;
    std::string print() {
        std::string res = is_volatile ? "volatile " : "";
        if (is_final) res += " final ";
        res += access + " " + name_tok.value + "(";
        for (int i = 0; i < return_types.size(); i++) {
            res += return_types[i].value;
            if (i + 1 < return_types.size()) { res += ", "; }
        }
        for (int i = 0; i < params.size(); i++) {
            res += params[i].toString();
            if (i + 1 < return_types.size()) res += ", ";
        }
        return res;
    }
};
class ConceptInfo {
  public:
    class FunctionSignature {
      public:
        Token name;
        std::vector<Token> return_types;
        std::vector<Parameter> params;
        std::vector<GenericType> generics;
        std::string print() const {
            std::string res = this->name.value + "(";
            for (size_t i = 0; i < params.size(); i++) {
                res += params[i].toString();
                if (i + 1 != params.size()) res += ", ";
            }
            res += ") -> ";
            if (return_types.empty())
                res += "void";
            else
                for (size_t i = 0; i < return_types.size(); i++) {
                    res += return_types[i].value;
                    if (i + 1 != return_types.size()) res += ", ";
                }
            return res;
        }
    };
    struct Block {
        std::vector<FunctionSignature> signatures;
        std::vector<Block> subblocks;
        std::vector<Token> requiredConcepts;
        std::vector<AnyNode> nodes;
        std::vector<std::pair<std::string, std::string>> params;
        Token constraint;
    };
    struct DefaultBlock {
        std::vector<std::pair<Token, ClassMethodInfo>> definitions;
    };
    std::vector<std::pair<Block, std::optional<DefaultBlock>>> blocks;
};
struct ModifierInfo {
    std::vector<std::pair<Token, StatementsNode*>> handlers;
};
enum class UserTypeKind { Struct, Alias, Union, Enum, Class, Concept, Modifier };

struct UnionMember {
    std::string type;
};
class ConceptProvee {
  public:
    Token conceptName;
    Token proverName;
    std::vector<ClassMethodInfo> additionalProof;
    std::vector<std::string> namespacePath;
};

struct UserTypeInfo {
    Position pos;
    std::string enumType;
    UserTypeKind kind;
    ModifierInfo modifierInfo;
    std::vector<ConceptProvee> provees;
    std::vector<StructField> fields;
    std::string aliasTarget;
    ConceptInfo conceptInfo;
    std::vector<UnionMember> members;
    std::vector<EnumEntry> enumEntries;
    std::vector<ClassField> classFields;
    std::vector<ClassMethodInfo> classMethods;
    std::vector<std::string> friendClasses;
    std::vector<std::string> friendlyClasses;
    std::string baseClassName = "";
    std::string namespace_path;
    bool is_abstract_class = false;
    bool is_final_class = false;
    std::vector<GenericType> generics;
    std::string baseFile = "";
};
struct Aer {
    StatementsNode* statements;
    Error* error;
    std::unordered_map<std::string, UserTypeInfo> user_types;
};
struct Diagnostic {
    Error* error = nullptr;
    std::string level;
};
struct Mer {
    Aer ast;
    Ler tokens;
    std::string res;
    std::vector<Diagnostic> errors;
};
class BinOpNode {
  public:
    bool is_f;
    AnyNode left_node;
    Token op_tok;
    AnyNode right_node;
    Position getPos() { return op_tok.pos; }
    BinOpNode(AnyNode left, Token op, AnyNode right, bool is_f = false) : left_node(left), op_tok(op), right_node(right) { this->is_f = is_f; }
    std::string print() const;
};

class UnaryOpNode {
  public:
    Token op_tok;
    AnyNode node;
    bool is_postfix;
    Position getPos() { return op_tok.pos; }
    UnaryOpNode(Token op, AnyNode n, bool postfix = false) : op_tok(op), node(n), is_postfix(postfix) {}
    std::string print() const;
};

class AssignExprNode {
  public:
    AnyNode target;
    AnyNode value;
    Token op_tok;
    AssignExprNode(AnyNode t, Token op, AnyNode v) : target(t), op_tok(op), value(v) {}
    Position getPos() { return op_tok.pos; }
    std::string print() { return "(" + printAny(target) + " = " + printAny(value) + ")"; }
};

class VarAssignNode {
  public:
    bool is_const;
    Token type_tok;
    Token var_name_tok;
    AnyNode value_node;
    Position getPos() { return var_name_tok.pos; }
    VarAssignNode(bool is_const, Token type, Token name, AnyNode value, bool is_foreign = false)
        : is_const(is_const), is_foreign(is_foreign), type_tok(type), var_name_tok(name), value_node(value) {}
    bool is_foreign;
    std::string print() const;
};

class VarAccessNode {
  public:
    Token var_name_tok;
    Position getPos() { return var_name_tok.pos; }
    VarAccessNode(Token name) : var_name_tok(name) {}

    std::string print() const;
};
class ModifierNode {
  public:
    std::vector<Token> modifiers;
    AnyNode node;
    ModifierNode(std::vector<Token> modifiers, AnyNode node) {
        this->modifiers = modifiers;
        this->node = node;
    }
    Position getPos() { return modifiers.empty() ? get_pos(node) : modifiers[0].pos; }
    std::string print() const { return "modifier(" + printAny(node) + ")"; }
};
class DeferNode {
  public:
    StatementsNode* block;
    Position getPos() { return get_pos(block); }
    DeferNode(StatementsNode* block) : block(block) {}
    std::string print() const;
};

class IfNode {
  public:
    std::optional<AnyNode> init;
    AnyNode condition;
    StatementsNode* then_branch;
    std::vector<std::pair<AnyNode, StatementsNode*>> elif_branches;
    StatementsNode* else_branch;
    bool is_comptime;
    Position getPos() { return get_pos(condition); }
    IfNode(std::optional<AnyNode> init_node, AnyNode cond, StatementsNode* then_b, std::vector<std::pair<AnyNode, StatementsNode*>> elifs = {},
           StatementsNode* else_b = nullptr, bool is_comptime = false)
        : init(init_node), condition(cond), then_branch(then_b), elif_branches(elifs), else_branch(else_b), is_comptime(is_comptime) {}

    std::string print() const;
};
class QIfNode {
  public:
    std::optional<AnyNode> init;
    AnyNode condition;
    StatementsNode* then_branch;
    std::vector<std::pair<AnyNode, StatementsNode*>> qelif_branches;
    StatementsNode* qelse_branch;
    Position getPos() { return get_pos(condition); }
    QIfNode(std::optional<AnyNode> init_stmt, AnyNode cond, StatementsNode* then_b, std::vector<std::pair<AnyNode, StatementsNode*>> qelif_b,
            StatementsNode* qelse_b)
        : init(init_stmt), condition(cond), then_branch(then_b), qelif_branches(qelif_b), qelse_branch(qelse_b) {}

    std::string print() const { return "qif (...)"; }
};
struct CaseLabel {
    AnyNode expr;
};

class SwitchNode {
  public:
    AnyNode value;
    struct Section {
        std::vector<CaseLabel> cases;
        bool is_default = false;
        StatementsNode* body;
    };
    std::vector<Section> sections;
    Position getPos() { return get_pos(value); }
    std::string print() { return printAny(value); }
};
class MatchNode {
  public:
    bool is_enum;
    AnyNode value;
    struct Section {
        std::pair<std::string, std::vector<std::string>> enum_case;
        CaseLabel normal_case;
        bool is_default = false;
        StatementsNode* body;
    };
    std::vector<Section> sections;
    Position getPos() { return get_pos(value); }
    std::string print() { return printAny(value); }
};

class QSwitchNode {
  public:
    AnyNode value;
    StatementsNode* case_t;
    StatementsNode* case_f;
    StatementsNode* case_n;
    StatementsNode* case_b;
    Position getPos() { return get_pos(value); }
    QSwitchNode(AnyNode val, StatementsNode* t, StatementsNode* f, StatementsNode* n, StatementsNode* b)
        : value(val), case_t(t), case_f(f), case_n(n), case_b(b) {}

    std::string print() const { return "qswitch (...)"; }
};

class BreakNode {
  public:
    Token tok;
    BreakNode(Token t) : tok(t) {}
    std::string print() { return "(break)"; }
    Position getPos() { return tok.pos; }
};

class UnreachableNode {
  public:
    Token tok;
    UnreachableNode(Token t) : tok(t) {}
    std::string print() { return "(unreachable)"; }
    Position getPos() { return tok.pos; }
};
class WhileNode {
  public:
    AnyNode condition;
    StatementsNode* body;
    bool is_dowhile = false;
    Position getPos() { return get_pos(condition); }
    WhileNode(AnyNode cond, StatementsNode* b, bool isdowhile = false) : condition(cond), body(b), is_dowhile(isdowhile) {}

    std::string print() { return "(while " + printAny(condition) + " " + body->print() + ")"; }
};
class TryCatchNode {
  public:
    StatementsNode* try_body;
    struct CatchBody {
        std::string var_name;
        std::string var_type;
        StatementsNode* body;
    };
    std::vector<CatchBody> catch_bodys;
    Token tok;
    Position pos;
    Position getPos() { return tok.pos; }
    TryCatchNode(StatementsNode* try_b, std::vector<CatchBody> catch_b, Token t, Position p)
        : try_body(try_b), catch_bodys(catch_b), tok(t), pos(p) {}
    std::string print() { return "try {\n\t" + try_body->print() + "\n} catch (...) {    ...\n}"; }
};
class ForNode {
  public:
    std::optional<AnyNode> init;
    AnyNode condition;
    std::optional<AnyNode> update;
    StatementsNode* body;

    Position getPos() { return get_pos(condition); }
    ForNode(std::optional<AnyNode> i, AnyNode cond, std::optional<AnyNode> u, StatementsNode* b) : init(i), condition(cond), update(u), body(b) {}

    std::string print() {
        std::string res = "(for ";
        if (this->init.has_value()) { res += "init=" + printAny(this->init.value()) + "; "; }
        res += printAny(this->condition) + "; ";
        if (update.has_value()) { res += printAny(update.value()); }
        res += ")";
        return res;
    }
};

class ContinueNode {
  public:
    Token tok;
    ContinueNode(Token t) : tok(t) {}
    Position getPos() { return tok.pos; }
    std::string print() { return "(continue)"; }
};

class FuncDefNode {
  public:
    std::vector<Token> return_types;
    std::optional<Token> name_tok;
    std::list<Parameter> params;
    std::vector<Token> modifiers;
    StatementsNode* body;
    std::vector<GenericType> generics;
    std::string namespace_path;
    Position pos;
    bool is_extern = false;
    bool is_foreign = false;
    bool is_volatile = false;
    bool is_header = true;
    FuncDefNode(std::vector<Token> ret_types, std::optional<Token> name, std::list<Parameter> parameters, StatementsNode* func_body,
                std::string ns = "", bool is_ex = false, bool is_f = false, std::vector<GenericType> generics = {}, bool is_volatile = false,
                bool is_header = false, std::vector<Token> modifiers = {})
        : return_types(ret_types), name_tok(name), params(parameters), body(func_body), namespace_path(ns), modifiers(modifiers) {
        this->is_extern = is_ex;
        this->is_foreign = is_f;
        this->generics = generics;
        this->is_volatile = is_volatile;
        this->is_header = is_header;
    }
    Position getPos() const {
        if (name_tok.has_value()) return name_tok.value().pos;

        if (return_types.empty()) return get_pos(body);

        return return_types[0].pos;
    }
    std::string print() {
        std::string result = "";
        for (size_t i = 0; i < return_types.size(); i++) {
            result += return_types[i].value;
            if (i < return_types.size() - 1) result += ", ";
        }
        result += " " + (name_tok ? name_tok->value : "lambda") + "(";
        for (auto& param : params) { result += param.name.value; }
        result += "{" + body->print() + "}";
        return result;
    }

    bool is_multi_return() const { return return_types.size() > 1; }
};

class CallNode {
  public:
    AnyNode node_to_call;
    std::list<AnyNode> arg_nodes;
    CallNode(AnyNode node, std::list<AnyNode> args) : node_to_call(node), arg_nodes(args) {}
    Position getPos() { return get_pos(node_to_call); }
    std::string print() { return printAny(node_to_call) + "(args)"; }
};
class MultiReturnNode {
  public:
    std::vector<AnyNode> values;
    Position pos;
    Position getPos() { return pos; }
    MultiReturnNode(std::vector<AnyNode> vals, Position p) : values(vals), pos(p) {}

    std::string print() {
        std::string result = "return ";
        for (size_t i = 0; i < values.size(); i++) {
            result += printAny(values[i]);
            if (i < values.size() - 1) result += ", ";
        }
        return result + ";";
    }
};

class MultiVarDeclNode {
  public:
    bool is_const;
    std::vector<Token> type_toks;
    std::vector<Token> var_names;
    AnyNode value;
    Position getPos() { return var_names[0].pos; };
    MultiVarDeclNode(bool is_const, std::vector<Token> type_toks, std::vector<Token> var_names, AnyNode value)
        : is_const(is_const), type_toks(type_toks), var_names(var_names), value(value) {}
};
class ArrayDeclNode {
  public:
    bool is_const;
    Token type_tok;
    Token var_name_tok;
    AnyNode value;
    Position getPos() { return var_name_tok.pos; }
    int dimensions;
    std::vector<std::optional<int>> sizes;

    ArrayDeclNode(bool is_const, Token type_tok, Token var_name_tok, AnyNode value, int dims, std::vector<std::optional<int>> sizes)
        : is_const(is_const), type_tok(type_tok), var_name_tok(var_name_tok), value(value), dimensions(dims), sizes(sizes) {}
    std::string print() {
        std::string type_str = type_tok.value;
        std::string name_str = var_name_tok.value;
        for (int i = 0; i < dimensions; ++i) {
            name_str += "[";
            if (sizes[i].has_value()) name_str += std::to_string(*sizes[i]);
            name_str += "]";
        }

        std::string result = type_str + " " + name_str;
        result += " = " + printAny(value);
        return result;
    }
};

class ArrayLiteralNode {
  public:
    std::vector<AnyNode> elements;
    std::string type;
    Position pos;
    AnyNode length;
    Position getPos() { return pos; }
    ArrayLiteralNode(std::vector<AnyNode> elems, Position p, std::string struct_ty = "") : elements(elems), pos(p), type(struct_ty) {}

    std::string print() {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); i++) {
            result += printAny(elements[i]);
            if (i < elements.size() - 1) result += ", ";
        }
        if (elements.empty()) result += this->type;
        return result + "]";
    }
};
class MapLiteralNode {
  public:
    std::vector<std::pair<AnyNode, AnyNode>> pairs;
    Position pos;
    std::string struct_type;
    Position getPos() { return pos; }
    MapLiteralNode(std::vector<std::pair<AnyNode, AnyNode>> p, Position pos, std::string struct_ty = "")
        : pairs(p), pos(pos), struct_type(struct_ty) {}

    std::string print() const { return this->struct_type + "{}"; }
};
class ArrayAccessNode {
  public:
    AnyNode base;
    std::vector<AnyNode> indices;
    Position getPos() { return get_pos(base); }
    ArrayAccessNode(AnyNode base_node, std::vector<AnyNode> idxs) : base(base_node), indices(idxs) {}

    std::string print() {
        std::string s = printAny(base) + "[";
        for (size_t i = 0; i < indices.size(); ++i) {
            s += printAny(indices[i]);
            if (i != indices.size() - 1) s += ", ";
        }
        s += "]";
        return s;
    }
};
class MethodCallNode {
  public:
    AnyNode base;
    Token method_name;
    std::vector<AnyNode> args;
    Position getPos() { return method_name.pos; }
    MethodCallNode(AnyNode base_node, Token method, std::vector<AnyNode> arguments) : base(base_node), method_name(method), args(arguments) {}

    std::string print() { return printAny(base) + "." + method_name.value + "(...)"; }
};
class ReturnNode {
  public:
    AnyNode value;
    Position pos;
    Position getPos() { return pos; }
    ReturnNode(AnyNode val, Position p) : value(val), pos(p) {}

    std::string print() { return "return " + printAny(value); }
};
class PropertyAccessNode {
  public:
    AnyNode* base;
    Token property_name;
    Token base_name_tok;
    PropertyAccessNode(AnyNode base_node, Token base_name, Token prop)
        : base(new AnyNode(base_node)), base_name_tok(base_name), property_name(prop) {}
    Position getPos() { return property_name.pos; }
    std::string print() { return printAny(*base) + "." + property_name.value; }
};
class SpreadNode {
  public:
    AnyNode expr;
    Position getPos() { return get_pos(expr); }
    SpreadNode(AnyNode expression) : expr(expression) {}

    std::string print() { return "@" + printAny(expr); }
};
class ForeachNode {
  public:
    Token elem_type;
    Token elem_name;
    AnyNode collection;
    AnyNode body;
    Position getPos() { return elem_name.pos; }
    ForeachNode(Token type, Token name, AnyNode coll, AnyNode body_stmt) : elem_type(type), elem_name(name), collection(coll), body(body_stmt) {}

    std::string print() { return "foreach (" + elem_type.value + " " + elem_name.value + " in ...)"; }
};
class FieldAssignNode {
  public:
    AnyNode base;
    Token field_name;
    AnyNode value;
    Position getPos() { return field_name.pos; }
    FieldAssignNode(AnyNode b, Token f, AnyNode v) : base(b), field_name(f), value(v) {}
    std::string print() const { return printAny(base) + "." + field_name.value + " = " + printAny(value); }
};
class ArrayAssignNode {
  public:
    AnyNode array_access;
    AnyNode value;
    Position getPos() { return get_pos(array_access); }
    ArrayAssignNode(AnyNode access, AnyNode val) : array_access(access), value(val) {}

    std::string print() const { return "array_assign"; }
};
class NamespaceNode {
  public:
    std::string name;
    Position pos;
    std::vector<AnyNode> body;
    Position getPos() { return pos; }
    NamespaceNode(std::string name, std::vector<AnyNode> body, Position pos) : name(name), body(body) { this->pos = pos; }
    std::string print() { return "namespace " + name; }
};
class ParseResult;
using Prs = std::variant<std::monostate, ParseResult, NumberNode, StringNode, CharNode, BoolNode, BinOpNode*, Error*, UnaryOpNode*, VarAccessNode*,
                         VarAssignNode*, AssignExprNode*, StatementsNode*, IfNode*, BreakNode*, UnreachableNode*, SwitchNode*, MatchNode*, WhileNode*, ForNode*,
                         ContinueNode*, CallNode*, FuncDefNode*, ReturnNode*, MultiReturnNode*, MultiVarDeclNode*, ArrayDeclNode*, ArrayLiteralNode*,
                         ArrayAccessNode*, MethodCallNode*, PropertyAccessNode*, SpreadNode*, ForeachNode*, QBoolNode, QInNode, QIfNode*,
                         QSwitchNode*, ArrayAssignNode*, FieldAssignNode*, MapLiteralNode*, NamespaceNode*, TryCatchNode*, RefVarDeclNode*,
                         NullptrNode, TypeValueNode, DeferNode*, ModifierNode*>;
class ParseResult {
  public:
    AnyNode node;
    Error* error = nullptr;
    ParseResult() = default;
    AnyNode reg_node(AnyNode res);
    AnyNode reg(Prs res);
    Prs success(AnyNode node);
    void failure(Error*);
    Prs to_prs();
};
class InterpEer {
  public:
    Error* err;
    Position pos;
    InterpEer() {}
    void fail(Error* err, Position pos) {
        this->err = err;
        this->pos = pos;
    }
};
#endif
