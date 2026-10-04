#include "nodes.h"
#include <stdexcept>
Position get_pos(AnyNode node) {
    return std::visit(
        [](auto& n) -> Position {
            if constexpr (requires { n.getPos(); }) {
                return n.getPos();
            } else if constexpr (requires { n->getPos(); }) {
                return n->getPos();
            } else {
                return Position(Position::INVALID_FILE_ID, 0, 0, 0);
            }
        },
        node);
}
std::string printAny(const AnyNode& node) {
    return std::visit(
        [](auto arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, NumberNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, StringNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, CharNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, BoolNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, QBoolNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, RefVarDeclNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, TypeValueNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, NullptrNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, BinOpNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, UnaryOpNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, StatementsNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ModifierNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, VarAccessNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, VarAssignNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, AssignExprNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, DeferNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, IfNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, TryCatchNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, std::monostate>) {
                return "";
            } else if constexpr (std::is_same_v<T, WhileNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ForNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ContinueNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, UnreachableNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, BreakNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, MatchNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, SwitchNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, FuncDefNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, CallNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ReturnNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ArrayDeclNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ArrayLiteralNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ArrayAccessNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, MethodCallNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, PropertyAccessNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, SpreadNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, ForeachNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, QIfNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, QSwitchNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, QInNode>) {
                return arg.print();
            } else if constexpr (std::is_same_v<T, ArrayAssignNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, FieldAssignNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, MapLiteralNode*>) {
                return arg->print();
            } else if constexpr (std::is_same_v<T, NamespaceNode*>) {
                std::string ret = arg->print() + "\n";
                for (auto& stmt : arg->body) { ret += printAny(stmt); }
                return ret;
            } else {
                return "<unknown>";
            }
        },
        node);
}
std::string VarAssignNode::print() const {
    return "(" + this->type_tok.print() + " " + this->var_name_tok.print() + " " + printAny(this->value_node) + ")";
}
std::string VarAccessNode::print() const {
    return "(" + this->var_name_tok.print() + ")";
}
NumberNode::NumberNode(Token tok) {
    this->tok = tok;
}
std::string CharNode::print() const {
    return this->tok.print();
}
std::string NumberNode::print() const {
    return this->tok.print();
}
std::string BinOpNode::print() const {
    return "(" + printAny(left_node) + " " + op_tok.print() + " " + printAny(right_node) + ")";
}
std::string UnaryOpNode::print() const {
    return std::string{"("} + this->op_tok.print() + ", " + printAny(this->node) + ")";
}
std::string StatementsNode::print() const {
    std::string res = "[";
    for (size_t i = 0; i < statements.size(); i++) {
        res += printAny(statements[i]);
        if (i < statements.size() - 1) { res += ", "; }
    }
    res += "]";
    return res;
}
std::string StringNode::print() const {
    return "(" + this->tok.print() + ")";
}
StringNode::StringNode(Token tok) {
    this->tok = tok;
}
std::string TypeValueNode::print() const {
    return "(" + this->tok.print() + ")";
}
TypeValueNode::TypeValueNode(Token tok) {
    this->tok = tok;
}
BoolNode::BoolNode(Token tok) {
    this->tok = tok;
}
std::string BoolNode::print() const {
    return "(" + this->tok.print() + ")";
}
QBoolNode::QBoolNode(Token tok) {
    this->tok = tok;
}
std::string QBoolNode::print() const {
    return "(" + this->tok.print() + ")";
}
std::string DeferNode::print() const {
    std::string res = "(defer ";
    res += this->block->print();
    return res + ")";
}
std::string IfNode::print() const {
    std::string res = std::string("(") + (this->is_comptime ? "comptime if " : "if ");
    if (init.has_value()) { res += "init=" + printAny(init.value()) + "; "; }
    res += printAny(this->condition) + " " + this->then_branch->print();
    for (auto& p : this->elif_branches) { res += " elif " + printAny(p.first) + " " + p.second->print(); }
    if (this->else_branch) { res += " else " + this->else_branch->print(); }
    res += ")";
    return res;
}
AnyNode ParseResult::reg_node(AnyNode res) {
    return res;
}
AnyNode ParseResult::reg(Prs res_variant) {
    if (std::holds_alternative<Error*>(res_variant)) {
        this->error = std::get<Error*>(res_variant);
        return std::monostate{};
    }
    return std::visit(
        [this](auto arg) -> AnyNode {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, Error*> || std::is_same_v<T, ParseResult>) {
                return AnyNode{std::monostate{}};
            } else if constexpr (std::is_same_v<T, UnaryOpNode>) {
                return AnyNode{new UnaryOpNode(arg)};
            } else if constexpr (std::is_constructible_v<AnyNode, T>) {
                return arg;
            } else {
                return AnyNode{std::monostate{}};
            }
        },
        res_variant);
}

Prs ParseResult::success(AnyNode node) {
    this->node = node;
    return std::visit(
        [](auto arg) -> Prs {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return Prs{std::monostate{}};
            } else if constexpr (std::is_same_v<T, NumberNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, StringNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, CharNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BoolNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, RefVarDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, TypeValueNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, NullptrNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BinOpNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, UnaryOpNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ModifierNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, VarAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, VarAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, AssignExprNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, StatementsNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, DeferNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, IfNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, TryCatchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, UnreachableNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BreakNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MatchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, SwitchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, WhileNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ForNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ContinueNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, FuncDefNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, CallNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiVarDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayLiteralNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MethodCallNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, PropertyAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, SpreadNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ForeachNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QBoolNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QIfNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QSwitchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QInNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, FieldAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MapLiteralNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, NamespaceNode*>) {
                return Prs{arg};
            } else {
                return Prs{std::monostate{}};
            }
        },
        this->node);
}

Prs ParseResult::to_prs() {
    if (this->error) { return Prs{this->error}; }
    return std::visit(
        [](auto arg) -> Prs {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return Prs{std::monostate{}};
            } else if constexpr (std::is_same_v<T, NumberNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, StringNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, CharNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BoolNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, RefVarDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BinOpNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, UnaryOpNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ModifierNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, VarAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, VarAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, AssignExprNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, StatementsNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, DeferNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, IfNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, TryCatchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, BreakNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MatchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, SwitchNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, WhileNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ForNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ContinueNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, FuncDefNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, CallNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiVarDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MultiReturnNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayDeclNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayLiteralNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MethodCallNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, PropertyAccessNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, SpreadNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ForeachNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QBoolNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QIfNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, QInNode>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, ArrayAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, FieldAssignNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, MapLiteralNode*>) {
                return Prs{arg};
            } else if constexpr (std::is_same_v<T, NamespaceNode*>) {
                return Prs{arg};
            } else {
                return Prs{std::monostate{}};
            }
        },
        this->node);
}
void ParseResult::failure(Error* error) {
    this->error = error;
}

