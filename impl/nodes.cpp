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
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "";
            } else if constexpr (std::is_same_v<T, NamespaceNode*>) {
                std::string ret = arg->print() + "\n";
                for (auto& stmt : arg->body) { ret += printAny(stmt); }
                return ret;
            } else if constexpr (std::is_pointer_v<T>) {
                return arg->print();
            } else if constexpr (requires { arg.print(); }) {
                return arg.print();
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
    return std::visit([](auto arg) -> Prs { return arg; }, this->node);
}

Prs ParseResult::to_prs() {
    if (this->error) { return Prs{this->error}; }
    return std::visit([](auto arg) -> Prs { return arg; }, this->node);
}
void ParseResult::failure(Error* error) {
    this->error = error;
}
