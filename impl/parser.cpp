#include "parser.h"
#include "lexer.h"
#include "shared_globals.h"
#include <algorithm>
AnyNode Parser::default_value_for_type(const Token& type_tok, const Position& pos) {
    std::string type = type_tok.value;
    if (type == "short int") return AnyNode{NumberNode(Token(TokenType::SHORT_INT, "0", pos))};
    if (type == "int") return AnyNode{NumberNode(Token(TokenType::INT, "0", pos))};
    if (type == "long int") return AnyNode{NumberNode(Token(TokenType::LONG_INT, "0", pos))};

    if (type == "float") return AnyNode{NumberNode(Token(TokenType::FLOAT, "0.0f", pos))};
    if (type == "double") return AnyNode{NumberNode(Token(TokenType::DOUBLE, "0.0", pos))};
    if (type == "long double") return AnyNode{NumberNode(Token(TokenType::LONG_DOUBLE, "0.0", pos))};
    if (type == "addr_t") return AnyNode{NumberNode(Token(TokenType::ADDR_T, "0", pos))};
    if (type == "byte") return AnyNode{NumberNode(Token(TokenType::BYTE, "0", pos))};
    if (type == "nibble") return AnyNode{NumberNode(Token(TokenType::NIBBLE, "0", pos))};

    if (type == "string") return AnyNode{StringNode(Token(TokenType::STRING, "", pos))};
    if (type == "char") return AnyNode{CharNode(Token(TokenType::CHAR, std::string(1, '\0'), pos))};
    if (type == "bool") return AnyNode{BoolNode(Token(TokenType::BOOL, "false", pos))};
    if (type == "qbool") return AnyNode{QBoolNode(Token(TokenType::QBOOL, "none", pos))};
    return AnyNode{std::monostate{}};
}
Parser::Parser(std::vector<Token> tokens, std::unordered_map<std::string, UserTypeInfo> user_types) {
    this->user_types = user_types;
    this->tokens = tokens;
    if (this->tokens.size() > index) { this->current_tok = this->tokens[index]; }
}
Token Parser::advance() {
    Position last_pos = this->current_tok.pos;

    if (index + 1 < tokens.size()) {
        ++index;
        this->current_tok = tokens[index];
    } else {
        this->current_tok = Token(TokenType::EOFT, "", last_pos);
        index = tokens.size();
    }
    return this->current_tok;
}
std::string Parser::qualify_name(const std::string& name) {
    if (namespaceStack.empty()) return name;
    std::string q;
    for (auto& ns : namespaceStack) {
        if (!q.empty()) q += "::";
        q += ns;
    }
    q += "::";
    q += name;
    return q;
}
void Parser::parseGenerics(std::vector<GenericType>& genericsM, ParseResult& res) {
    if (this->current_tok.type == TokenType::LESS) {
        this->advance();
        while (true) {
            GenericType curr;
            if (this->current_tok.type != TokenType::IDENTIFIER) {
                if (this->current_tok.type == TokenType::KEYWORD &&
                    std::unordered_set<std::string>({"int", "double", "float", "byte", "nibble", "addr_t", "string", "char", "bool", "qbool"})
                        .contains(this->current_tok.value)) {
                    curr.isNonType = true;
                    curr.nonTypeKind = this->current_tok.value;
                } else if (this->current_tok.value == "long" || this->current_tok.value == "short") {
                    std::string prev = this->current_tok.value;
                    this->advance();
                    if (this->current_tok.value != "int" && this->current_tok.value != "double") {
                        res.failure(new InvalidSyntaxError("QC-S087: Expected 'int' or 'double' after '" + prev + "'", this->current_tok.pos));
                        return;
                    }
                    curr.isNonType = true;
                    curr.nonTypeKind = prev + " " + this->current_tok.value;
                } else {
                    res.failure(new InvalidSyntaxError("QC-G003: Expected generic typename to be a identifier ([_a-zA-Z][0-9a-zA-Z_]*)",
                                                       this->current_tok.pos));
                    return;
                }
                this->advance();
            }
            curr.name = this->current_tok.value;
            this->advance();
            if (this->current_tok.type == TokenType::LPAREN && !curr.isNonType) {
                this->advance();
                if (this->current_tok.type != TokenType::COLON) {
                    if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.value == "proves") {
                        curr.constraint = (std::unordered_set<std::string>({"usertype", "primitive", "numeric", "pointer"})
                                                   .contains(this->current_tok.value)
                                               ? this->current_tok.value
                                               : parseTypeString());
                    } else {
                        res.failure(new InvalidSyntaxError(
                            "QC-C007: Expected : or a concept: or a usertype:, primitive:, or callable: before generic constraint list",
                            this->current_tok.pos));
                        return;
                    }
                    if (this->current_tok.value == "usertype" || this->current_tok.value == "primitive" || this->current_tok.value == "numeric" ||
                        this->current_tok.value == "pointer")
                        this->advance();
                } else {
                    curr.constraint = "";
                }
                this->advance();
                if (this->current_tok.type == TokenType::NOT) {
                    curr.negated = true;
                    this->advance();
                }
                while (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                    curr.subconstraints.push_back(this->current_tok.value);
                    this->advance();
                    if (this->current_tok.type == TokenType::PIPE) { this->advance(); }
                }
                if (this->current_tok.type != TokenType::RPAREN) {
                    res.failure(new InvalidSyntaxError("QC-C008: Expected ) after generic type constraint list.", this->current_tok.pos));
                    return;
                }
                this->advance();
            }
            if (this->current_tok.type == TokenType::EQ) {
                this->advance();
                curr.defaultValue = this->current_tok.value;
                this->advance();
            }
            if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::MORE) {
                res.failure(new InvalidSyntaxError("QC-G004: Expected > or , after generic type.", this->current_tok.pos));
                return;
            }
            genericsM.push_back(curr);
            if (this->current_tok.type == TokenType::MORE) {
                this->advance();
                break;
            }
            this->advance();
        }
    }
}
void Parser::fn(std::vector<ConceptInfo::Block>& blockList, ParseResult& res, std::vector<ConceptInfo::Block>& blocks,
                std::vector<std::pair<int, ConceptInfo::DefaultBlock>>& defaultBlocks) {
    ConceptInfo::Block block;
    bool is_at_least = false;
    Token is_at_least_tok;
    if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "at_least") {
        is_at_least = true;
        is_at_least_tok = this->current_tok;
        this->advance();
    }
    std::string num = "";
    if (this->current_tok.type == TokenType::INT) {
        num = this->current_tok.value;
        this->advance();
    }
    if (this->current_tok.type == TokenType::KEYWORD && (this->current_tok.value == "all_of" || this->current_tok.value == "_of")) {
        if (is_at_least) block.constraint = is_at_least_tok;
        std::vector<ConceptInfo::Block> subblocks;
        block.constraint.pos = this->current_tok.pos;
        block.constraint.value += ((this->current_tok.value == "_of") ? (is_at_least ? " " : "") + num + "_of" : "all_of");
        this->advance();
        std::vector<std::pair<std::string, std::string>> params;
        if (this->current_tok.type == TokenType::LPAREN) {
            this->advance();
            while (this->current_tok.type != TokenType::RPAREN && this->current_tok.type != TokenType::EOFT) {
                std::string type = parseTypeString();
                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-C002: Expected parameter name in concept block", this->current_tok.pos));
                    return;
                }
                params.push_back(std::make_pair(type, this->current_tok.value));
                this->advance();
                if (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                } else if (this->current_tok.type != TokenType::RPAREN) {
                    res.failure(new InvalidSyntaxError("QC-C003: Expected ',' or ')' after parameter", this->current_tok.pos));
                    return;
                }
            }
            if (this->current_tok.type != TokenType::RPAREN) {
                res.failure(new InvalidSyntaxError("QC-C004: Expected ')' closing parameter list", this->current_tok.pos));
                return;
            }
            this->advance();
        }
        block.params = params;
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after concept name", this->current_tok.pos));
            return;
        }
        this->advance();
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type == TokenType::KEYWORD &&
                    std::unordered_set<std::string>({"at_least", "default", "all_of"}).contains(this->current_tok.value) ||
                this->current_tok.type == TokenType::INT) {
                fn(subblocks, res, blocks, defaultBlocks);
                if (res.error) return;
                continue;
            }
            std::vector<Token> modifiers;
            if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
                user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
                modifiers.push_back(this->current_tok);
                this->advance();
            }
            if (!is_known_type(this->current_tok.value) && !is_primitive_type(this->current_tok.value)) {
                auto node = res.reg(this->text_unops());
                block.nodes.push_back(modifiers.empty() ? node : new ModifierNode(modifiers, node));
                if (res.error) return;
                if (this->current_tok.type != TokenType::SEMICOLON) {
                    res.failure(new InvalidSyntaxError("QC-C001: Expected ; after concept expression", this->current_tok.pos));
                    return;
                }
                this->advance();
                continue;
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "proves") {
                this->advance();
                block.requiredConcepts.push_back(Token(TokenType::IDENTIFIER, parseTypeString(), this->current_tok.pos));
                this->advance();
                continue;
            }
            std::vector<Token> type_list;
            auto parse_one_type_into = [&](Token& out_tok) -> bool {
                std::string field_type = parseTypeString();
                out_tok = Token(TokenType::KEYWORD, field_type, this->current_tok.pos);
                return true;
            };
            {
                Token t;
                if (!parse_one_type_into(t)) return;
                type_list.push_back(t);
            }
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                Token t;
                if (!parse_one_type_into(t)) return;
                type_list.push_back(t);
            }
            Token name_tok;
            if (this->current_tok.type == TokenType::IDENTIFIER && this->current_tok.value != "operator") {
                name_tok = this->current_tok;
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "operator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::MUL:
                case TokenType::DIV:
                case TokenType::EQ_TO:
                case TokenType::NOT_EQ:
                case TokenType::EQ:
                case TokenType::NOT:
                case TokenType::AND:
                case TokenType::OR:
                case TokenType::MORE:
                case TokenType::LESS:
                case TokenType::MORE_EQ:
                case TokenType::LESS_EQ:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::XOR:
                case TokenType::QNOT:
                case TokenType::QAND:
                case TokenType::QOR:
                case TokenType::QXOR:
                case TokenType::INCREMENT:
                case TokenType::DECREMENT:
                case TokenType::BITWISE_NOT:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT:
                case TokenType::BITWISE_XOR:
                case TokenType::PIPE:
                case TokenType::AMPERSAND:
                case TokenType::COLLAPSE_OR:
                case TokenType::PLUS_EQ:
                case TokenType::MINUS_EQ:
                case TokenType::MUL_EQ:
                case TokenType::DIV_EQ:
                case TokenType::MOD_EQ:
                case TokenType::BIT_X_EQ:
                case TokenType::BIT_A_EQ:
                case TokenType::BIT_O_EQ:
                case TokenType::LSH_EQ:
                case TokenType::RSH_EQ:
                case TokenType::LRSH_EQ:
                case TokenType::RROT_EQ:
                case TokenType::LROT_EQ:
                case TokenType::COLLAPSE_AND: break;
                case TokenType::LPAREN:
                    this->advance();
                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S094: expected closing paren in operator()", op_tok.pos));
                        return;
                    }
                    break;
                case TokenType::LBRACKET:
                    this->advance();
                    if (this->current_tok.type == TokenType::RBRACKET) {
                        long_ops[0] = this->current_tok;
                        if (this->peek().type == TokenType::EQ) {
                            this->advance();
                            long_ops[1] = this->current_tok;
                            break;
                        }
                        long_ops[1] = Token(TokenType::EOFT, "N/A", op_tok.pos);
                        break;
                    } else {
                        res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos));
                        return;
                    }
                default: res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos)); return;
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::PLUS: op_name = "operator+"; break;
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::MUL: op_name = "operator*"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::EQ_TO: op_name = "operator=="; break;
                case TokenType::NOT_EQ: op_name = "operator!="; break;
                case TokenType::EQ: op_name = "operator="; break;
                case TokenType::NOT: op_name = "operator!"; break;
                case TokenType::AND: op_name = "operator&&"; break;
                case TokenType::OR: op_name = "operator||"; break;
                case TokenType::MORE: op_name = "operator>"; break;
                case TokenType::LESS: op_name = "operator<"; break;
                case TokenType::PLUS_EQ: op_name = "operator+="; break;
                case TokenType::MINUS_EQ: op_name = "operator-="; break;
                case TokenType::MUL_EQ: op_name = "operator*="; break;
                case TokenType::DIV_EQ: op_name = "operator/="; break;
                case TokenType::MOD_EQ: op_name = "operator%="; break;
                case TokenType::BIT_X_EQ: op_name = "operator$="; break;
                case TokenType::BIT_A_EQ: op_name = "operator&="; break;
                case TokenType::BIT_O_EQ: op_name = "operator|="; break;
                case TokenType::LSH_EQ: op_name = "operator<<="; break;
                case TokenType::RSH_EQ: op_name = "operator|>="; break;
                case TokenType::LRSH_EQ: op_name = "operator:>="; break;
                case TokenType::RROT_EQ: op_name = "operator|>>="; break;
                case TokenType::LROT_EQ: op_name = "operator<<<="; break;
                case TokenType::MORE_EQ: op_name = "operator>="; break;
                case TokenType::LESS_EQ: op_name = "operator<="; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::XOR: op_name = "operator^"; break;
                case TokenType::QNOT: op_name = "operator!!"; break;
                case TokenType::QAND: op_name = "operator&&&"; break;
                case TokenType::QOR: op_name = "operator|||"; break;
                case TokenType::QXOR: op_name = "operator^^"; break;
                case TokenType::COLLAPSE_OR: op_name = "operator|&|"; break;
                case TokenType::COLLAPSE_AND: op_name = "operator&|&"; break;
                case TokenType::LBRACKET: op_name = ((long_ops[1].type == TokenType::EQ) ? "operator[]=" : "operator[]"); break;
                case TokenType::LPAREN: op_name = "operator()"; break;
                case TokenType::INCREMENT: op_name = "operator++"; break;
                case TokenType::DECREMENT: op_name = "operator--"; break;
                case TokenType::BITWISE_NOT: op_name = "operator~"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                case TokenType::BITWISE_XOR: op_name = "operator$"; break;
                case TokenType::PIPE: op_name = "operator|"; break;
                case TokenType::AMPERSAND: op_name = "operator&"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "roperator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::MINUS:
                case TokenType::DIV:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT: break;
                default: res.failure(new InvalidSyntaxError("QC-S096: Unsupported operator in roperator method", op_tok.pos)); return;
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else {
                res.failure(new InvalidSyntaxError("QC-T007: Expected method name after type(s)", this->current_tok.pos));
                return;
            }
            std::vector<GenericType> genericsM;
            parseGenerics(genericsM, res);
            if (res.error) return;
            if (this->current_tok.type == TokenType::LPAREN) {
                auto m_pr = this->func_def_multi(type_list, std::make_optional(name_tok), genericsM, true, false, modifiers);
                if (std::holds_alternative<Error*>(m_pr)) return;
                auto fn = std::get<FuncDefNode*>(m_pr);
                block.signatures.push_back(
                    ConceptInfo::FunctionSignature(name_tok, type_list, std::vector<Parameter>(fn->params.begin(), fn->params.end()), genericsM));
                continue;
            }
        }
        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '}' after concept block", this->current_tok.pos));
            return;
        }
        this->advance();
        block.subblocks = subblocks;
        blockList.push_back(block);
    }
    if (this->current_tok.type == TokenType::KEYWORD && (this->current_tok.value == "default")) {
        ConceptInfo::DefaultBlock defBlock;
        this->advance();
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after concept block name", this->current_tok.pos));
            return;
        }
        this->advance();
        std::string modifier = "else";
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "class") {
                modifier = "class";
                this->advance();
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "else") {
                modifier = "else";
                this->advance();
                this->advance();
            }
            std::vector<Token> modifiers;
            if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
                user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
                modifiers.push_back(this->current_tok);
                this->advance();
            }
            if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-T008: Expected method type in default block", this->current_tok.pos));
                return;
            }
            std::vector<Token> type_list;
            auto parse_one_type_into = [&](Token& out_tok) -> bool {
                std::string field_type = parseTypeString();
                out_tok = Token(TokenType::KEYWORD, field_type, this->current_tok.pos);
                return true;
            };
            {
                Token t;
                if (!parse_one_type_into(t)) return;
                type_list.push_back(t);
            }
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                Token t;
                if (!parse_one_type_into(t)) return;
                type_list.push_back(t);
            }
            Token name_tok;
            if (this->current_tok.type == TokenType::IDENTIFIER && this->current_tok.value != "operator") {
                name_tok = this->current_tok;
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "operator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::MUL:
                case TokenType::DIV:
                case TokenType::EQ_TO:
                case TokenType::NOT_EQ:
                case TokenType::EQ:
                case TokenType::NOT:
                case TokenType::AND:
                case TokenType::OR:
                case TokenType::MORE:
                case TokenType::LESS:
                case TokenType::MORE_EQ:
                case TokenType::LESS_EQ:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::XOR:
                case TokenType::QNOT:
                case TokenType::QAND:
                case TokenType::QOR:
                case TokenType::QXOR:
                case TokenType::INCREMENT:
                case TokenType::DECREMENT:
                case TokenType::BITWISE_NOT:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT:
                case TokenType::BITWISE_XOR:
                case TokenType::PIPE:
                case TokenType::AMPERSAND:
                case TokenType::COLLAPSE_OR:
                case TokenType::PLUS_EQ:
                case TokenType::MINUS_EQ:
                case TokenType::MUL_EQ:
                case TokenType::DIV_EQ:
                case TokenType::MOD_EQ:
                case TokenType::BIT_X_EQ:
                case TokenType::BIT_A_EQ:
                case TokenType::BIT_O_EQ:
                case TokenType::LSH_EQ:
                case TokenType::RSH_EQ:
                case TokenType::LRSH_EQ:
                case TokenType::RROT_EQ:
                case TokenType::LROT_EQ:
                case TokenType::COLLAPSE_AND: break;
                case TokenType::LPAREN:
                    this->advance();
                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S094: expected closing paren in operator()", op_tok.pos));
                        return;
                    }
                    break;
                case TokenType::LBRACKET:
                    this->advance();
                    if (this->current_tok.type == TokenType::RBRACKET) {
                        long_ops[0] = this->current_tok;
                        if (this->peek().type == TokenType::EQ) {
                            this->advance();
                            long_ops[1] = this->current_tok;
                            break;
                        }
                        long_ops[1] = Token(TokenType::EOFT, "N/A", op_tok.pos);
                        break;
                    } else {
                        res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos));
                        return;
                    }
                default: res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos)); return;
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::PLUS: op_name = "operator+"; break;
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::MUL: op_name = "operator*"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::EQ_TO: op_name = "operator=="; break;
                case TokenType::NOT_EQ: op_name = "operator!="; break;
                case TokenType::EQ: op_name = "operator="; break;
                case TokenType::NOT: op_name = "operator!"; break;
                case TokenType::AND: op_name = "operator&&"; break;
                case TokenType::OR: op_name = "operator||"; break;
                case TokenType::MORE: op_name = "operator>"; break;
                case TokenType::LESS: op_name = "operator<"; break;
                case TokenType::PLUS_EQ: op_name = "operator+="; break;
                case TokenType::MINUS_EQ: op_name = "operator-="; break;
                case TokenType::MUL_EQ: op_name = "operator*="; break;
                case TokenType::DIV_EQ: op_name = "operator/="; break;
                case TokenType::MOD_EQ: op_name = "operator%="; break;
                case TokenType::BIT_X_EQ: op_name = "operator$="; break;
                case TokenType::BIT_A_EQ: op_name = "operator&="; break;
                case TokenType::BIT_O_EQ: op_name = "operator|="; break;
                case TokenType::LSH_EQ: op_name = "operator<<="; break;
                case TokenType::RSH_EQ: op_name = "operator|>="; break;
                case TokenType::LRSH_EQ: op_name = "operator:>="; break;
                case TokenType::RROT_EQ: op_name = "operator|>>="; break;
                case TokenType::LROT_EQ: op_name = "operator<<<="; break;
                case TokenType::MORE_EQ: op_name = "operator>="; break;
                case TokenType::LESS_EQ: op_name = "operator<="; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::XOR: op_name = "operator^"; break;
                case TokenType::QNOT: op_name = "operator!!"; break;
                case TokenType::QAND: op_name = "operator&&&"; break;
                case TokenType::QOR: op_name = "operator|||"; break;
                case TokenType::QXOR: op_name = "operator^^"; break;
                case TokenType::COLLAPSE_OR: op_name = "operator|&|"; break;
                case TokenType::COLLAPSE_AND: op_name = "operator&|&"; break;
                case TokenType::LBRACKET: op_name = ((long_ops[1].type == TokenType::EQ) ? "operator[]=" : "operator[]"); break;
                case TokenType::LPAREN: op_name = "operator()"; break;
                case TokenType::INCREMENT: op_name = "operator++"; break;
                case TokenType::DECREMENT: op_name = "operator--"; break;
                case TokenType::BITWISE_NOT: op_name = "operator~"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                case TokenType::BITWISE_XOR: op_name = "operator$"; break;
                case TokenType::PIPE: op_name = "operator|"; break;
                case TokenType::AMPERSAND: op_name = "operator&"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "roperator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::MINUS:
                case TokenType::DIV:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT: break;
                default: res.failure(new InvalidSyntaxError("QC-S096: Unsupported operator in roperator method", op_tok.pos)); return;
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else {
                res.failure(new InvalidSyntaxError("QC-T007: Expected method name after type(s)", this->current_tok.pos));
                return;
            }
            std::vector<GenericType> genericsM;
            parseGenerics(genericsM, res);
            if (res.error) return;
            if (this->current_tok.type == TokenType::LPAREN) {
                ClassMethodInfo mi;
                mi.name_tok = name_tok;
                auto m_pr = this->func_def_multi(type_list, std::make_optional(name_tok), genericsM, true, false, modifiers);
                if (std::holds_alternative<Error*>(m_pr)) return;
                auto fn = std::get<FuncDefNode*>(m_pr);
                mi.params.clear();
                mi.modifiers = fn->modifiers;
                mi.params.reserve(fn->params.size());
                for (auto it = fn->params.begin(); it != fn->params.end(); ++it) { mi.params.push_back(*it); }
                mi.return_types = fn->return_types;
                mi.body = fn->body;
                mi.is_constructor = false;
                mi.generics = genericsM;
                defBlock.definitions.push_back(std::make_pair(Token(TokenType::IDENTIFIER, modifier, this->current_tok.pos), mi));
                continue;
            }
        }
        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '}' after concept block", this->current_tok.pos));
            return;
        }
        this->advance();
        defaultBlocks.push_back(std::make_pair(blocks.size() - 1, defBlock));
    }
}

Prs Parser::qif_expr() {
    ParseResult res;
    this->advance();

    if (this->current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S001: Expected '(' after 'qif'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    AnyNode condition = res.reg(this->text_unops());
    if (res.error) { return res.to_prs(); }

    if (this->current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S002: Expected ')' after qif condition", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (this->current_tok.type != TokenType::LBRACE) {
        res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after qif condition", this->current_tok.pos));
        return res.to_prs();
    }

    this->advance();

    std::vector<AnyNode> then_stmts;
    while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
        auto stmt = res.reg(this->statement());
        if (res.error) { return res.to_prs(); }
        then_stmts.push_back(stmt);
    }

    if (this->current_tok.type != TokenType::RBRACE) {
        res.failure(new InvalidSyntaxError("QC-S004: Expected '}'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    auto then_branch = new StatementsNode(then_stmts, true);
    if (res.error) { return res.to_prs(); }

    std::vector<std::pair<AnyNode, StatementsNode*>> qelif_branches;

    while (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "qelif") {
        this->advance();
        if (this->current_tok.type != TokenType::LPAREN) {
            res.failure(new InvalidSyntaxError("QC-S005: Expected '(' after 'qelif'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        AnyNode qelif_cond = res.reg(this->text_unops());
        if (res.error) return res.to_prs();

        if (this->current_tok.type != TokenType::RPAREN) {
            res.failure(new InvalidSyntaxError("QC-S006: Expected ')' after qelif condition", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S007: Expected '{' after qelif condition", this->current_tok.pos));
            return res.to_prs();
        }

        this->advance();

        std::vector<AnyNode> qelif_stmts;
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            auto stmt = res.reg(this->statement());
            if (res.error) return res.to_prs();
            qelif_stmts.push_back(stmt);
        }

        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S004: Expected '}'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        auto qelif_body = new StatementsNode(qelif_stmts, true);
        if (res.error) return res.to_prs();

        qelif_branches.emplace_back(qelif_cond, qelif_body);
    }

    StatementsNode* qelse_branch = nullptr;
    if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "qelse") {
        this->advance();
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S008: Expected '{' after 'qelse'", this->current_tok.pos));
            return res.to_prs();
        }

        this->advance();

        std::vector<AnyNode> qelse_stmts;
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            auto stmt = res.reg(this->statement());
            if (res.error) return res.to_prs();
            qelse_stmts.push_back(stmt);
        }

        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S004: Expected '}'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        qelse_branch = new StatementsNode(qelse_stmts, true);
        if (res.error) return res.to_prs();
    }
    return res.success(new QIfNode(std::nullopt, condition, then_branch, qelif_branches, qelse_branch));
}
Prs Parser::defer_expr() {
    ParseResult res;
    if (!(this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "defer")) {
        res.failure(new InvalidSyntaxError("QC-S009: Expected 'defer'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    StatementsNode* deferBlock;
    if (!parse_block_into(deferBlock, res)) return res.to_prs();
    auto defernode = new DeferNode(deferBlock);
    return res.success(defernode);
}

Prs Parser::if_expr(bool is_comptime) {
    auto has_semicolon_before_closing_paren = [this]() -> bool {
        size_t idx = index;
        int depth = 0;
        for (size_t i = idx; i < this->tokens.size(); ++i) {
            const Token& t = this->tokens[i];
            if (t.type == TokenType::LPAREN) {
                ++depth;
            } else if (t.type == TokenType::RPAREN) {
                if (depth == 0) { return false; }
                --depth;
            } else if (t.type == TokenType::SEMICOLON && depth == 0) {
                return true;
            }
        }
        return false;
    };
    ParseResult res;
    if (!(this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "if")) {
        res.failure(new InvalidSyntaxError("QC-S009: Expected 'if'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (this->current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S010: Expected '(' after 'if'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    std::optional<AnyNode> init_node = std::nullopt;

    if (has_semicolon_before_closing_paren()) {
        if (is_known_type(this->current_tok.value)) {
            bool is_const = false;
            Token tok = this->current_tok;
            if (tok.value == "const") {
                is_const = true;
                this->advance();
                tok = this->current_tok;
                if (tok.type != TokenType::KEYWORD) {
                    res.failure(new InvalidSyntaxError("QC-S011: Expected type after 'const' in if-init", this->current_tok.pos));
                    return res.to_prs();
                }
            }
            Token type_tok = tok;
            this->advance();
            if (this->current_tok.type == TokenType::AMPERSAND) {
                this->advance();
                type_tok.value += "&";
            }

            while (this->current_tok.type == TokenType::MUL) {
                this->advance();
                type_tok.value += "*";
            }
            if (this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-S012: Expected identifier in if-init", this->current_tok.pos));
                return res.to_prs();
            }
            Token var_name = this->current_tok;
            this->advance();

            AnyNode value;
            if (this->current_tok.type == TokenType::EQ) {
                this->advance();
                value = res.reg(this->qout_expr());
                if (res.error) return res.to_prs();
            } else {
                if (is_const) {
                    res.failure(new InvalidSyntaxError("QC-S013: const variables must be initialized in "
                                                       "if-init",
                                                       var_name.pos));
                    return res.to_prs();
                }
                value = default_value_for_type(type_tok, var_name.pos);
            }

            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new InvalidSyntaxError("QC-S014: Expected ';' after if-init declaration", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();

            init_node = AnyNode{new VarAssignNode(is_const, type_tok, var_name, value, this->in_foreign)};
        } else {
            AnyNode expr_init = res.reg(this->assignment_expr());
            if (res.error) return res.to_prs();

            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new InvalidSyntaxError("QC-S015: Expected ';' after if-init expression", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            init_node = expr_init;
        }
    }

    AnyNode condition = res.reg(this->text_unops());
    if (res.error) return res.to_prs();

    if (this->current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S016: Expected ')' after condition", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    StatementsNode* then_branch;
    if (!parse_block_into(then_branch, res)) return res.to_prs();

    std::vector<std::pair<AnyNode, StatementsNode*>> elifs;
    StatementsNode* else_branch = nullptr;

    while (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "else") {
        this->advance();
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "if") {
            this->advance();
            if (this->current_tok.type != TokenType::LPAREN) {
                res.failure(new InvalidSyntaxError("QC-S017: Expected '(' after 'else if'", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            AnyNode elif_cond = res.reg(this->text_unops());
            if (res.error) return res.to_prs();
            if (this->current_tok.type != TokenType::RPAREN) {
                res.failure(new InvalidSyntaxError("QC-S018: Expected ')' after 'else if' condition", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            StatementsNode* elif_block;
            if (!parse_block_into(elif_block, res)) return res.to_prs();
            elifs.emplace_back(elif_cond, elif_block);
            continue;
        } else {
            if (!parse_block_into(else_branch, res)) return res.to_prs();
            break;
        }
    }

    auto ifnode = new IfNode(init_node, condition, then_branch, elifs, else_branch, is_comptime);
    return res.success(ifnode);
}
Prs Parser::try_catch_expr() {
    ParseResult res;
    if (!(this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "try")) {
        res.failure(new InvalidSyntaxError("QC-S030: Expected 'try'", this->current_tok.pos));
        return res.to_prs();
    }
    Token try_tok = this->current_tok;
    this->advance();
    StatementsNode* try_body;
    if (!parse_block_into(try_body, res)) return res.to_prs();
    std::vector<TryCatchNode::CatchBody> catch_bodys;
    while (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "catch") {
        this->advance();
        if (this->current_tok.type != TokenType::LPAREN) {
            res.failure(new InvalidSyntaxError("QC-S032: Expected '(' after 'catch'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER && this->current_tok.value != "...") {
            res.failure(new InvalidSyntaxError("QC-S033: Expected type in catch declaration", this->current_tok.pos));
            return res.to_prs();
        }
        Token type_tok = this->current_tok;
        this->advance();
        if (this->current_tok.type == TokenType::AMPERSAND) {
            this->advance();
            type_tok.value += "&";
        }

        while (this->current_tok.type == TokenType::MUL) {
            this->advance();
            type_tok.value += "*";
        }
        std::string catch_type = type_tok.value;
        if (this->current_tok.type != TokenType::IDENTIFIER && catch_type != "...") {
            res.failure(new InvalidSyntaxError("QC-S034: Expected variable name in catch declaration", this->current_tok.pos));
            return res.to_prs();
        }
        std::string catch_var = "";
        if (catch_type != "...") {
            Token var_tok = this->current_tok;
            catch_var = var_tok.value;
            this->advance();
        }
        if (this->current_tok.type != TokenType::RPAREN) {
            res.failure(new InvalidSyntaxError("QC-S035: Expected ')' after catch variable", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        StatementsNode* catch_body;
        if (!parse_block_into(catch_body, res)) return res.to_prs();
        catch_bodys.push_back({catch_var, catch_type, catch_body});
    }
    if (catch_bodys.size() == 0) {
        res.failure(new InvalidSyntaxError("QC-S031: Expected 'catch' after try block", this->current_tok.pos));
        return res.to_prs();
    }
    auto try_catch_node = new TryCatchNode(try_body, catch_bodys, try_tok, try_tok.pos);
    return res.success(try_catch_node);
}
Prs Parser::match_stmt() {
    ParseResult res;
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "match")) {
        res.failure(new InvalidSyntaxError("QC-S019: Expected 'match'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S020: Expected '(' after 'match'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    AnyNode value = res.reg(this->text_unops());
    if (res.error) return res.to_prs();
    if (current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S021: Expected ')' after match expression", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (current_tok.type != TokenType::LBRACE) {
        res.failure(new InvalidSyntaxError("QC-S022: Expected '{' after match(...)", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    std::vector<MatchNode::Section> sections;
    bool is_enum = false;
    while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
        MatchNode::Section section;
        bool saw_label = true;
        if (current_tok.value == "default") {
            this->advance();
            section.is_default = true;
        } else {
            if (is_known_type(this->current_tok.value)) {
                is_enum = true;
                std::pair<std::string, std::vector<std::string>> data;
                std::string type = this->parseTypeString();
                if (this->current_tok.type != TokenType::DOT) {
                    res.failure(new InvalidSyntaxError("QC-S301: Expected . after enum name", current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                data.first = type + "." + this->current_tok.value;
                this->advance();
                if (this->current_tok.type == TokenType::LPAREN) {
                    this->advance();
                    while (this->current_tok.type == TokenType::IDENTIFIER) {
                        data.second.push_back(this->current_tok.value);
                        this->advance();
                        if (this->current_tok.type == TokenType::COMMA) this->advance();
                    } 
                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S302: Expected ) after enum tags", current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();
                }
                section.enum_case = data;
            } else {
                AnyNode case_expr = res.reg(this->text_unops());
                if (res.error) return res.to_prs();
                section.normal_case = CaseLabel{case_expr};
            }
        }
        std::vector<AnyNode> stmts;
        if (this->current_tok.type != TokenType::BIG_ARROW) {
            res.failure(new InvalidSyntaxError("QC-S300: Expected '=>' after match case", current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        section.body = new StatementsNode({});
        parse_block_into(section.body, res);
        if (res.error) return res.to_prs();
        sections.push_back(section);
    }
    if (current_tok.type != TokenType::RBRACE) {
        res.failure(new InvalidSyntaxError("QC-S025: Expected '}' after match body", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    auto mch = new MatchNode();
    mch->value = value;
    mch->sections = sections;
    mch->is_enum = is_enum;
    return res.success(mch);
}
Prs Parser::switch_stmt() {
    ParseResult res;
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "switch")) {
        res.failure(new InvalidSyntaxError("QC-S019: Expected 'switch'", current_tok.pos));
        return res.to_prs();
    }
    advance();

    if (current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S020: Expected '(' after 'switch'", current_tok.pos));
        return res.to_prs();
    }
    advance();

    AnyNode value = res.reg(this->text_unops());
    if (res.error) return res.to_prs();

    if (current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S021: Expected ')' after switch expression", current_tok.pos));
        return res.to_prs();
    }
    advance();

    if (current_tok.type != TokenType::LBRACE) {
        res.failure(new InvalidSyntaxError("QC-S022: Expected '{' after switch(...)", current_tok.pos));
        return res.to_prs();
    }
    advance();

    std::vector<SwitchNode::Section> sections;

    while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
        SwitchNode::Section section;
        bool saw_label = false;
        while (this->current_tok.type == TokenType::KEYWORD && (this->current_tok.value == "case" || this->current_tok.value == "default")) {

            saw_label = true;

            if (current_tok.value == "default") {
                this->advance();
                if (current_tok.type != TokenType::COLON && current_tok.type != TokenType::SEMICOLON) {}
                section.is_default = true;
                this->advance();
                break;
            } else {
                advance();
                AnyNode case_expr = res.reg(this->text_unops());
                if (res.error) return res.to_prs();

                if (current_tok.type != TokenType::COLON) {
                    res.failure(new InvalidSyntaxError("QC-S023: Expected ':' after case label", current_tok.pos));
                    return res.to_prs();
                }
                advance();

                section.cases.push_back(CaseLabel{case_expr});
            }
        }

        if (!saw_label) {
            res.failure(new InvalidSyntaxError("QC-S024: Expected 'case' or 'default' inside switch", current_tok.pos));
            return res.to_prs();
        }
        std::vector<AnyNode> stmts;
        while (current_tok.type != TokenType::RBRACE &&
               !(current_tok.type == TokenType::KEYWORD && (current_tok.value == "case" || current_tok.value == "default"))) {

            Prs st = this->statement();
            if (std::holds_alternative<Error*>(st)) {
                res.failure(std::get<Error*>(st));
                return res.to_prs();
            }
            AnyNode any_stmt = std::visit(
                [](auto arg) -> AnyNode {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_constructible_v<AnyNode, T>) { return AnyNode(arg); }
                    return std::monostate{};
                },
                st);
            stmts.push_back(any_stmt);
        }

        section.body = new StatementsNode(stmts, true);
        sections.push_back(section);
    }

    if (current_tok.type != TokenType::RBRACE) {
        res.failure(new InvalidSyntaxError("QC-S025: Expected '}' after switch body", current_tok.pos));
        return res.to_prs();
    }
    advance();

    auto sw = new SwitchNode();
    sw->value = value;
    sw->sections = sections;
    return res.success(sw);
}
Prs Parser::qswitch_stmt() {
    ParseResult res;
    this->advance();

    if (this->current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S026: Expected '(' after 'qswitch'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    AnyNode value = res.reg(this->text_unops());
    if (res.error) return res.to_prs();

    if (this->current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S027: Expected ')' after qswitch value", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (this->current_tok.type != TokenType::LBRACE) {
        res.failure(new InvalidSyntaxError("QC-S028: Expected '{' after qswitch", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    StatementsNode* case_t = nullptr;
    StatementsNode* case_f = nullptr;
    StatementsNode* case_n = nullptr;
    StatementsNode* case_b = nullptr;

    while (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "case") {
        this->advance();

        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S029: Expected case label (t, f, n, or b)", this->current_tok.pos));
            return res.to_prs();
        }

        std::string case_label = this->current_tok.value;
        this->advance();

        if (this->current_tok.type != TokenType::COLON) {
            res.failure(new InvalidSyntaxError("QC-S023: Expected ':' after case label", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        std::vector<AnyNode> case_stmts;
        while (this->current_tok.type != TokenType::KEYWORD || (this->current_tok.value != "case" && this->current_tok.value != "break")) {

            if (this->current_tok.type == TokenType::RBRACE) break;
            if (this->current_tok.type == TokenType::EOFT) break;

            auto stmt = res.reg(this->statement());
            if (res.error) return res.to_prs();
            case_stmts.push_back(stmt);
        }

        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "break") {
            this->advance();
            if (this->current_tok.type == TokenType::SEMICOLON) { this->advance(); }
        }

        auto case_body = new StatementsNode(case_stmts, true);

        if (case_label == "t") {
            case_t = case_body;
        } else if (case_label == "f") {
            case_f = case_body;
        } else if (case_label == "n") {
            case_n = case_body;
        } else if (case_label == "b") {
            case_b = case_body;
        } else {
            res.failure(new InvalidSyntaxError("QC-S030: Invalid case label (must be t, f, n, or b)", this->current_tok.pos));
            return res.to_prs();
        }
    }

    if (this->current_tok.type != TokenType::RBRACE) {
        res.failure(new InvalidSyntaxError("QC-S031: Expected '}' after qswitch", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    return res.success(new QSwitchNode(value, case_t, case_f, case_n, case_b));
}
Prs Parser::loop_stmt() {
    ParseResult res;
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "loop")) {
        res.failure(new InvalidSyntaxError("QC-S032: Expected 'loop'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    Position& cond_pos = this->current_tok.pos;
    StatementsNode* body;
    if (!this->parse_block_into(body, res)) return res.to_prs();
    auto wn = new WhileNode(BoolNode(Token(TokenType::BOOL, "true", cond_pos)), body);
    return res.success(wn);
}
Prs Parser::dowhile_stmt() {
    ParseResult res;
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "do")) {
        res.failure(new InvalidSyntaxError("QC-S032: Expected 'do'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    StatementsNode* body;
    if (!this->parse_block_into(body, res)) return res.to_prs();
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "while")) {
        res.failure(new InvalidSyntaxError("QC-S032: Expected 'while'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S033: Expected '(' after 'while'", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    AnyNode cond = res.reg(this->text_unops());
    if (res.error) return res.to_prs();
    if (current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S034: Expected ')' after while condition", current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (current_tok.type != TokenType::SEMICOLON) {
        res.failure(new MissingSemicolonError(this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    auto wn = new WhileNode(cond, body, true);
    return res.success(wn);
}

Prs Parser::while_stmt() {
    ParseResult res;
    if (!(current_tok.type == TokenType::KEYWORD && current_tok.value == "while")) {
        res.failure(new InvalidSyntaxError("QC-S032: Expected 'while'", current_tok.pos));
        return res.to_prs();
    }
    advance();

    if (current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S033: Expected '(' after 'while'", current_tok.pos));
        return res.to_prs();
    }
    advance();

    AnyNode cond = res.reg(this->text_unops());
    if (res.error) return res.to_prs();

    if (current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S034: Expected ')' after while condition", current_tok.pos));
        return res.to_prs();
    }
    advance();

    StatementsNode* body;
    if (!this->parse_block_into(body, res)) return res.to_prs();

    auto wn = new WhileNode(cond, body);
    return res.success(wn);
}
Prs Parser::for_stmt() {
    ParseResult res;
    Token type_tok;
    if (!(this->current_tok.type == TokenType::KEYWORD && current_tok.value == "for")) {
        res.failure(new InvalidSyntaxError("QC-S035: Expected 'for'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (this->current_tok.type != TokenType::LPAREN) {
        res.failure(new InvalidSyntaxError("QC-S036: Expected '(' after 'for'", current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    std::optional<AnyNode> init;
    std::optional<AnyNode> update;
    AnyNode condition;

    if (this->current_tok.type != TokenType::SEMICOLON) {
        if (this->current_tok.type == TokenType::KEYWORD &&
            (this->current_tok.value == "const" || this->current_tok.value == "int" || this->current_tok.value == "float" ||
             this->current_tok.value == "double" || this->current_tok.value == "bool" || this->current_tok.value == "qbool" ||
             this->current_tok.value == "string" || this->current_tok.value == "char" || this->current_tok.value == "addr_t" ||
             this->current_tok.value == "byte" || this->current_tok.value == "nibble" || this->current_tok.value == "atomic")) {

            bool is_const = false;
            Token tok = current_tok;
            if (tok.value == "atomic") { this->advance(); }
            if (tok.value == "const") {
                is_const = true;
                advance();
                tok = current_tok;
                if (tok.type != TokenType::KEYWORD) {
                    res.failure(new InvalidSyntaxError("QC-S011:  Expected type after 'const' in for-init", current_tok.pos));
                    return res.to_prs();
                }
            }

            type_tok = tok;
            advance();
            if (this->current_tok.type == TokenType::AMPERSAND) {
                this->advance();
                type_tok.value += "&";
            }

            while (this->current_tok.type == TokenType::MUL) {
                this->advance();
                type_tok.value += "*";
            }
            if (current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-S038: Expected identifier in for-init", current_tok.pos));
                return res.to_prs();
            }

            Token var_name = current_tok;
            advance();

            AnyNode value;
            if (current_tok.type == TokenType::EQ) {
                advance();
                value = res.reg(this->qout_expr());
                if (res.error) return res.to_prs();
            } else {
                if (is_const) {
                    res.failure(new InvalidSyntaxError("QC-S039: const variables must be initialized in "
                                                       "for-init",
                                                       var_name.pos));
                    return res.to_prs();
                }
                value = default_value_for_type(type_tok, var_name.pos);
            }

            init = AnyNode{new VarAssignNode(is_const, type_tok, var_name, value, this->in_foreign)};
        } else {
            AnyNode expr_init = res.reg(this->assignment_expr());
            if (res.error) return res.to_prs();
            init = expr_init;
        }
    }

    if (current_tok.type != TokenType::SEMICOLON) {
        res.failure(new InvalidSyntaxError("QC-S040: Expected ';' after for-init", current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (current_tok.type == TokenType::SEMICOLON) {
        condition = AnyNode{BoolNode(Token(TokenType::BOOL, "true", current_tok.pos))};
    } else {
        condition = res.reg(this->text_unops());
        if (res.error) return res.to_prs();
    }

    if (current_tok.type != TokenType::SEMICOLON) {
        res.failure(new InvalidSyntaxError("QC-S041: Expected ';' after for condition", current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (current_tok.type != TokenType::RPAREN) {
        AnyNode upd_expr = res.reg(this->assignment_expr());
        if (res.error) return res.to_prs();
        update = upd_expr;
    }

    if (current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S042: Expected ')' after for header", current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    auto parse_block = [&](StatementsNode*& out_block) -> bool {
        if (this->current_tok.type == TokenType::LBRACE) {
            this->advance();
            std::vector<AnyNode> stmts;
            while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
                Prs st = this->statement();
                if (std::holds_alternative<Error*>(st)) {
                    res.failure(std::get<Error*>(st));
                    return false;
                }
                AnyNode any_stmt = std::visit(
                    [](auto arg) -> AnyNode {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_constructible_v<AnyNode, T>) { return AnyNode(arg); }
                        return std::monostate{};
                    },
                    st);
                stmts.push_back(any_stmt);
            }
            if (this->current_tok.type != TokenType::RBRACE) {
                res.failure(new InvalidSyntaxError("QC-S043: Expected '}' after for body", this->current_tok.pos));
                return false;
            }
            this->advance();
            out_block = new StatementsNode(stmts, true);
            return true;
        } else {
            Prs st = this->statement();
            if (std::holds_alternative<Error*>(st)) {
                res.failure(std::get<Error*>(st));
                return false;
            }
            AnyNode any_stmt = std::visit(
                [](auto arg) -> AnyNode {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_constructible_v<AnyNode, T>) { return AnyNode(arg); }
                    return std::monostate{};
                },
                st);
            std::vector<AnyNode> stmts;
            stmts.push_back(any_stmt);
            out_block = new StatementsNode(stmts, true);
            return true;
        }
    };

    StatementsNode* body;
    if (!parse_block(body)) return res.to_prs();

    auto fn = new ForNode(init, condition, update, body);
    return res.success(fn);
}
Prs Parser::call(AnyNode node_to_call) {
    ParseResult res;
    this->advance();

    std::list<AnyNode> args;

    if (this->current_tok.type != TokenType::RPAREN) {

        auto parse_arg = [&]() -> AnyNode {
            if (this->current_tok.type == TokenType::AT) {
                this->advance();
                AnyNode expr = res.reg(this->text_unops());
                if (res.error) return AnyNode{};
                return new SpreadNode(expr);
            } else {
                return res.reg(this->text_unops());
            }
        };

        AnyNode arg = parse_arg();
        if (res.error) return res.to_prs();
        args.push_back(arg);

        while (this->current_tok.type == TokenType::COMMA) {
            this->advance();
            arg = parse_arg();
            if (res.error) return res.to_prs();
            args.push_back(arg);
        }
    }

    if (this->current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S044: Expected ')' after function arguments", this->current_tok.pos));
        return res.to_prs();
    }

    this->advance();

    return res.success(new CallNode(node_to_call, args));
}
Prs Parser::qout_expr() {
    ParseResult res;
    AnyNode left = res.reg(this->text_unops());
    if (res.error) return res.to_prs();

    return res.success(left);
}
Prs Parser::array_literal() {
    ParseResult res;
    Position start_pos = this->current_tok.pos;

    this->advance();

    std::vector<AnyNode> elements;
    std::string type = "";
    AnyNode length = std::monostate{};
    if (this->current_tok.type != TokenType::RBRACKET) {
        if (this->current_tok.type == TokenType::KEYWORD || is_known_type(this->current_tok.value)) {
            type = parseTypeString();
            if (this->current_tok.type != TokenType::COMMA) {
                res.failure(new InvalidSyntaxError("QC-S045: Expected ',' after element type in empty array literal.", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            length = res.reg(this->atom());
            if (res.error) return res.to_prs();
        } else {
            if (this->current_tok.type == TokenType::AT) {
                this->advance();
                AnyNode spread_expr = res.reg(this->text_unops());
                if (res.error) return res.to_prs();

                elements.push_back(new SpreadNode(spread_expr));
            } else {
                AnyNode elem = res.reg(this->text_unops());
                if (res.error) return res.to_prs();
                elements.push_back(elem);
            }

            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();

                if (this->current_tok.type == TokenType::AT) {
                    this->advance();
                    AnyNode spread_expr = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();

                    elements.push_back(new SpreadNode(spread_expr));
                } else {
                    AnyNode elem = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();
                    elements.push_back(elem);
                }
            }
        }
    }

    if (this->current_tok.type != TokenType::RBRACKET) {
        res.failure(new InvalidSyntaxError("QC-S045: Expected ']' in array literal", this->current_tok.pos));
        return res.to_prs();
    }

    this->advance();
    ArrayLiteralNode* result = new ArrayLiteralNode(elements, start_pos);
    result->type = type;
    result->length = length;
    return res.success(result);
}
Prs Parser::atom() {
    ParseResult res;
    std::vector<Token> modifiers;
    if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
        user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
        modifiers.push_back(this->current_tok);
        this->advance();
    }
    Token tok = this->current_tok;
    if (tok.type == TokenType::TYPEOF) {
        this->advance();
        AnyNode n = res.reg(this->text_unops());
        if (res.error) return res.to_prs();
        return res.success(new UnaryOpNode(tok, n));
    }
    if (tok.type == TokenType::LBRACE) {
        ParseResult res2;
        Position start_pos = this->current_tok.pos;
        this->advance();

        if (this->current_tok.type == TokenType::RBRACE) {
            this->advance();
            std::vector<std::pair<AnyNode, AnyNode>> pairs;
            return res2.success(new MapLiteralNode(pairs, start_pos));
        }

        auto first_key_expr = res2.reg(this->text_unops());
        if (res2.error) return res2.to_prs();

        if (this->current_tok.type == TokenType::COLON) {
            this->advance();

            AnyNode first_val_expr = res2.reg(this->text_unops());
            if (res2.error) return res2.to_prs();

            std::vector<std::pair<AnyNode, AnyNode>> pairs;
            pairs.emplace_back(first_key_expr, first_val_expr);

            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();

                AnyNode key_expr = res2.reg(this->text_unops());
                if (res2.error) return res2.to_prs();

                if (this->current_tok.type != TokenType::COLON) {
                    res2.failure(new InvalidSyntaxError("QC-S046: Expected ':' in map literal", this->current_tok.pos));
                    return res2.to_prs();
                }
                this->advance();

                AnyNode val_expr = res2.reg(this->text_unops());
                if (res2.error) return res2.to_prs();

                pairs.emplace_back(key_expr, val_expr);
            }

            if (this->current_tok.type != TokenType::RBRACE) {
                res2.failure(new InvalidSyntaxError("QC-S047: Expected '}' at end of map literal", this->current_tok.pos));
                return res2.to_prs();
            }
            this->advance();

            return res2.success(new MapLiteralNode(pairs, start_pos));
        } else {
            std::vector<AnyNode> elements;
            elements.push_back(first_key_expr);

            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                AnyNode e2 = res2.reg(this->text_unops());
                if (res2.error) return res2.to_prs();
                elements.push_back(e2);
            }

            if (this->current_tok.type != TokenType::RBRACE) {
                res2.failure(new InvalidSyntaxError("QC-S048: Expected '}' in initializer list", this->current_tok.pos));
                return res2.to_prs();
            }
            this->advance();

            return res2.success(new ArrayLiteralNode(elements, start_pos));
        }
    }
    if (tok.type == TokenType::LBRACKET) return this->array_literal();
    if (current_tok.value == "qin") {
        this->advance();

        return this->qin_expr();
    }

    if (tok.type == TokenType::INT || tok.type == TokenType::FLOAT || tok.type == TokenType::DOUBLE || tok.type == TokenType::ADDR_T ||
        tok.type == TokenType::BYTE || tok.type == TokenType::LONG_INT || tok.type == TokenType::SHORT_INT || tok.type == TokenType::LONG_DOUBLE ||
        tok.type == TokenType::NIBBLE) {
        this->advance();
        return res.success(NumberNode(tok));
    } else if (tok.type == TokenType::STRING) {
        this->advance();
        return res.success(StringNode(tok));
    } else if (tok.type == TokenType::CHAR) {
        this->advance();
        return res.success(CharNode(tok));
    } else if (tok.type == TokenType::BOOL) {
        this->advance();
        return res.success(BoolNode(tok));
    } else if (tok.type == TokenType::QBOOL) {
        this->advance();
        return res.success(QBoolNode(tok));
    } else if (tok.type == TokenType::AT) {
        Token op = tok;
        this->advance();

        AnyNode value = res.reg(this->atom());
        if (res.error) return res.to_prs();

        return res.success(new SpreadNode(value));
    } else if (tok.type == TokenType::KEYWORD && tok.value == "nullptr") {
        this->advance();
        return res.success(NullptrNode(tok.pos));
    } else if (tok.type == TokenType::IDENTIFIER) {
        std::string name = tok.value;
        Position pos = tok.pos;
        this->advance();
        bool generic_failed = false;
        while (true) {
            if (this->current_tok.type == TokenType::SCOPE) {
                this->advance();
                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-N001: Expected identifier or namespace name after '::'", this->current_tok.pos));
                    return res.to_prs();
                }
                name += "::" + this->current_tok.value;
                pos = this->current_tok.pos;
                this->advance();
            }
            if (this->current_tok.type == TokenType::LESS) {
                size_t oldId = this->index;
                std::string oldName = name;
                this->advance();
                name += "<";
                int depth = 1;
                if (this->current_tok.type == TokenType::MORE) {
                    name = oldName;
                    this->index = oldId;
                    this->current_tok = this->tokens[this->index];
                    depth = 0;
                    generic_failed = true;
                }
                bool next_comma = false;
                bool just_incremented = true;
                while (depth > 0) {
                    if (next_comma) {
                        if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                            this->current_tok.type != TokenType::MORE) {
                            this->index = oldId;
                            name = oldName;
                            generic_failed = true;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                    } else {
                        if (this->current_tok.type == TokenType::COMMA) {
                            this->index = oldId;
                            generic_failed = true;
                            name = oldName;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                    }
                    if (!(std::unordered_set<TokenType>(
                              {TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING, TokenType::INT, TokenType::DOUBLE,
                               TokenType::FLOAT, TokenType::CHAR, TokenType::ADDR_T, TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT,
                               TokenType::SHORT_INT, TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE, TokenType::BYTE, TokenType::NIBBLE})
                              .contains(this->current_tok.type))) {
                        this->index = oldId;
                        name = oldName;
                        generic_failed = true;
                        this->current_tok = this->tokens[this->index];
                        break;
                    }
                    if (this->current_tok.type == TokenType::EOFT) {
                        res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", pos));
                        return res.to_prs();
                    }
                    if (this->current_tok.type == TokenType::LESS) {
                        if (just_incremented) {
                            this->index = oldId;
                            name = oldName;
                            generic_failed = true;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                        just_incremented = true;
                        depth++;
                        if (depth > 128) {
                            res.failure(
                                new InvalidSyntaxError("QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                                           name.substr(0, 120) + "..." +
                                                           "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                                           "compiler is not a Matryoshka doll. It has feelings too.",
                                                       pos));
                            return res.to_prs();
                        }
                    } else if (this->current_tok.type == TokenType::MORE) {
                        depth--;
                        if (depth == 0) {
                            this->advance();
                            break;
                        }
                    } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                        just_incremented = false;
                        next_comma = true;
                        name += this->parseNoGenericString();
                        continue;
                    } else { // need to copy like 50 times
                        just_incremented = false;
                    }
                    if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                    name += this->current_tok.value;
                    this->advance();
                }
                if (this->index != oldId && name != oldName) name += ">";
            }
            if (generic_failed || this->current_tok.type != TokenType::SCOPE && this->current_tok.type != TokenType::LESS) { break; }
        }
        if (this->current_tok.type == TokenType::LESS) {
            size_t oldId = this->index;
            std::string oldName = name;
            this->advance();
            name += "<";
            int depth = 1;
            if (this->current_tok.type == TokenType::MORE) {
                name = oldName;
                this->index = oldId;
                this->current_tok = this->tokens[this->index];
                depth = 0;
            }
            bool next_comma = false;
            bool just_incremented = true;
            while (depth > 0) {
                if (next_comma) {
                    if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                        this->current_tok.type != TokenType::MORE) {
                        this->index = oldId;
                        name = oldName;
                        this->current_tok = this->tokens[this->index];
                        break;
                    }
                } else {
                    if (this->current_tok.type == TokenType::COMMA) {
                        this->index = oldId;
                        name = oldName;
                        this->current_tok = this->tokens[this->index];
                        break;
                    }
                }
                if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING, TokenType::INT,
                                                     TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR, TokenType::ADDR_T, TokenType::BOOL,
                                                     TokenType::QBOOL, TokenType::LONG_INT, TokenType::SHORT_INT, TokenType::LONG_DOUBLE,
                                                     TokenType::LESS, TokenType::MORE, TokenType::BYTE, TokenType::NIBBLE})
                          .contains(this->current_tok.type))) {
                    this->index = oldId;
                    name = oldName;
                    this->current_tok = this->tokens[this->index];
                    break;
                }
                if (this->current_tok.type == TokenType::EOFT) {
                    res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", pos));
                    return res.to_prs();
                }
                if (this->current_tok.type == TokenType::LESS) {
                    if (just_incremented) {
                        this->index = oldId;
                        name = oldName;
                        this->current_tok = this->tokens[this->index];
                        break;
                    }
                    just_incremented = true;
                    depth++;
                    if (depth > 128) {
                        res.failure(new InvalidSyntaxError("QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                                               name.substr(0, 120) + "..." +
                                                               "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                                               "compiler is not a Matryoshka doll. It has feelings too.",
                                                           pos));
                        return res.to_prs();
                    }
                } else if (this->current_tok.type == TokenType::MORE) {
                    depth--;
                    if (depth == 0) {
                        this->advance();
                        break;
                    }
                } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                    just_incremented = false;
                    next_comma = true;
                    name += this->parseNoGenericString();
                    continue;
                } else {
                    just_incremented = false;
                }
                if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                name += this->current_tok.value;
                this->advance();
            }
            if (this->index != oldId && name != oldName) name += ">";
        }
        if (this->current_tok.type == TokenType::LBRACE) {
            Position start_pos = this->current_tok.pos;
            this->advance();
            if (this->current_tok.type == TokenType::RBRACE) {
                this->advance();
                std::vector<std::pair<AnyNode, AnyNode>> pairs;
                return res.success(new MapLiteralNode(pairs, start_pos, name));
            }
            auto first_key_expr = res.reg(this->text_unops());
            if (res.error) return res.to_prs();
            if (this->current_tok.type == TokenType::COLON) {
                this->advance();
                AnyNode first_val_expr = res.reg(this->text_unops());
                if (res.error) return res.to_prs();
                std::vector<std::pair<AnyNode, AnyNode>> pairs;
                pairs.emplace_back(first_key_expr, first_val_expr);
                while (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                    AnyNode key_expr = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();

                    if (this->current_tok.type != TokenType::COLON) {
                        res.failure(new InvalidSyntaxError("QC-S046: Expected ':' in map literal", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();
                    AnyNode val_expr = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();
                    pairs.emplace_back(key_expr, val_expr);
                }
                if (this->current_tok.type != TokenType::RBRACE) {
                    res.failure(new InvalidSyntaxError("QC-S047: Expected '}' at end of map literal", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                return res.success(new MapLiteralNode(pairs, start_pos, name));
            } else {
                std::vector<AnyNode> elements;
                elements.push_back(first_key_expr);
                while (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                    AnyNode e2 = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();
                    elements.push_back(e2);
                }

                if (this->current_tok.type != TokenType::RBRACE) {
                    res.failure(new InvalidSyntaxError("QC-S048: Expected '}' in initializer list", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();

                return res.success(new ArrayLiteralNode(elements, start_pos, name));
            }
        }
        Token ident(TokenType::IDENTIFIER, name, pos);
        AnyNode base = new VarAccessNode(ident);
        if (this->current_tok.type == TokenType::LPAREN) {
            base = res.reg(this->call(base));
            if (res.error) return res.to_prs();
        }

        if (this->current_tok.type == TokenType::LBRACKET) {
            std::vector<AnyNode> indices;

            while (this->current_tok.type == TokenType::LBRACKET) {
                this->advance();
                AnyNode index = res.reg(this->text_unops());
                if (res.error) return res.to_prs();

                if (this->current_tok.type != TokenType::RBRACKET) {
                    res.failure(new InvalidSyntaxError("QC-S049: Expected ']'", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();

                indices.push_back(index);
            }

            base = new ArrayAccessNode(base, indices);
        }
        while (this->current_tok.type == TokenType::DOT || this->current_tok.type == TokenType::ARROW) {

            if (this->current_tok.type == TokenType::ARROW) {
                this->advance();

                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-S037: Expected property or method name after '->'", this->current_tok.pos));
                    return res.to_prs();
                }

                Token property_name = this->current_tok;
                this->advance();
                if (this->current_tok.type == TokenType::LESS) {
                    size_t oldId = this->index;
                    std::string oldName = property_name.value;
                    this->advance();
                    property_name.value += "<";
                    int depth = 1;
                    if (this->current_tok.type == TokenType::MORE) {
                        property_name.value = oldName;
                        this->index = oldId;
                        this->current_tok = this->tokens[this->index];
                        depth = 0;
                    }
                    bool next_comma = false;
                    bool just_incremented = true;
                    while (depth > 0) {
                        if (next_comma) {
                            if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                                this->current_tok.type != TokenType::MORE) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        } else {
                            if (this->current_tok.type == TokenType::COMMA) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        }
                        if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING,
                                                             TokenType::INT, TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR, TokenType::ADDR_T,
                                                             TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT, TokenType::SHORT_INT,
                                                             TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE, TokenType::BYTE,
                                                             TokenType::NIBBLE})
                                  .contains(this->current_tok.type))) {
                            this->index = oldId;
                            property_name.value = oldName;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                        if (this->current_tok.type == TokenType::EOFT) {
                            res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", this->current_tok.pos));
                            return res.to_prs();
                        }
                        if (this->current_tok.type == TokenType::LESS) {
                            if (just_incremented) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                            just_incremented = true;
                            depth++;
                            if (depth > 128) {
                                res.failure(
                                    new InvalidSyntaxError("QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                                               property_name.value.substr(0, 120) + "..." +
                                                               "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                                               "compiler is not a Matryoshka doll. It has feelings too.",
                                                           this->current_tok.pos));
                                return res.to_prs();
                            }
                        } else if (this->current_tok.type == TokenType::MORE) {
                            depth--;
                            if (depth == 0) {
                                this->advance();
                                break;
                            }
                        } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                            just_incremented = false;
                            next_comma = true;
                            property_name.value += this->parseNoGenericString();
                            continue;
                        } else {
                            just_incremented = false;
                        }
                        if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                        property_name.value += this->current_tok.value;
                        this->advance();
                    }
                    if (this->index != oldId && property_name.value != oldName) property_name.value += ">";
                }
                Token base_name_tok;
                if (auto var = std::get_if<VarAccessNode*>(&base)) {
                    base_name_tok = (*var)->var_name_tok;
                } else {
                    base_name_tok = Token(TokenType::IDENTIFIER, "", property_name.pos);
                }

                auto deref = new UnaryOpNode(Token(TokenType::MUL, "*", property_name.pos), base);

                if (this->current_tok.type == TokenType::LPAREN) {
                    this->advance();

                    std::vector<AnyNode> args;
                    if (this->current_tok.type != TokenType::RPAREN) {
                        while (true) {
                            if (this->current_tok.type == TokenType::AT) {
                                this->advance();
                                AnyNode expr = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(new SpreadNode(expr));
                            } else {
                                AnyNode arg = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(arg);
                            }
                            if (this->current_tok.type != TokenType::COMMA) break;
                            this->advance();
                        }
                    }

                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S052: Expected ')' after method arguments", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();

                    base = new MethodCallNode(deref, property_name, args);
                } else {
                    base = new PropertyAccessNode(deref, base_name_tok, property_name);
                }
            } else if (this->current_tok.type == TokenType::DOT) {
                this->advance();

                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-S053: Expected property or method name after '.'", this->current_tok.pos));
                    return res.to_prs();
                }

                Token property_name = this->current_tok;
                this->advance();
                if (this->current_tok.type == TokenType::LESS) {
                    size_t oldId = this->index;
                    std::string oldName = property_name.value;
                    this->advance();
                    property_name.value += "<";
                    int depth = 1;
                    if (this->current_tok.type == TokenType::MORE) {
                        property_name.value = oldName;
                        this->index = oldId;
                        this->current_tok = this->tokens[this->index];
                        depth = 0;
                    }
                    bool next_comma = false;
                    bool just_incremented = true;
                    while (depth > 0) {
                        if (next_comma) {
                            if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                                this->current_tok.type != TokenType::MORE) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        } else {
                            if (this->current_tok.type == TokenType::COMMA) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        }
                        if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING,
                                                             TokenType::INT, TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR, TokenType::ADDR_T,
                                                             TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT, TokenType::SHORT_INT,
                                                             TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE, TokenType::BYTE,
                                                             TokenType::NIBBLE})
                                  .contains(this->current_tok.type))) {
                            this->index = oldId;
                            property_name.value = oldName;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                        if (this->current_tok.type == TokenType::EOFT) {
                            res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", this->current_tok.pos));
                            return res.to_prs();
                        }
                        if (this->current_tok.type == TokenType::LESS) {
                            if (just_incremented) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                            just_incremented = true;
                            depth++;
                            if (depth > 128) {
                                res.failure(
                                    new InvalidSyntaxError("QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                                               property_name.value.substr(0, 120) + "..." +
                                                               "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                                               "compiler is not a Matryoshka doll. It has feelings too.",
                                                           this->current_tok.pos));
                                return res.to_prs();
                            }
                        } else if (this->current_tok.type == TokenType::MORE) {
                            depth--;
                            if (depth == 0) {
                                this->advance();
                                break;
                            }
                        } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                            just_incremented = false;
                            next_comma = true;
                            property_name.value += this->parseNoGenericString();
                            continue;
                        } else {
                            just_incremented = false;
                        }
                        if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                        property_name.value += this->current_tok.value;
                        this->advance();
                    }
                    if (this->index != oldId && property_name.value != oldName) property_name.value += ">";
                }
                if (this->current_tok.type == TokenType::LPAREN) {
                    this->advance();
                    std::vector<AnyNode> args;

                    if (this->current_tok.type != TokenType::RPAREN) {
                        while (true) {
                            if (this->current_tok.type == TokenType::AT) {
                                this->advance();
                                AnyNode expr = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(new SpreadNode(expr));
                            } else {
                                AnyNode arg = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(arg);
                            }
                            if (this->current_tok.type != TokenType::COMMA) break;
                            this->advance();
                        }
                    }

                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S044: Expected ')' after function arguments", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();

                    base = new MethodCallNode(base, property_name, args);
                } else {
                    base = new PropertyAccessNode(base, ident, property_name);
                }

                while (this->current_tok.type == TokenType::LBRACKET) {
                    std::vector<AnyNode> indices;
                    while (this->current_tok.type == TokenType::LBRACKET) {
                        this->advance();
                        AnyNode index = res.reg(this->text_unops());
                        if (res.error) return res.to_prs();
                        if (this->current_tok.type != TokenType::RBRACKET) {
                            res.failure(new InvalidSyntaxError("QC-S049: Expected ']'", this->current_tok.pos));
                            return res.to_prs();
                        }
                        this->advance();
                        indices.push_back(index);
                    }
                    base = new ArrayAccessNode(base, indices);
                }
            }
        }
        if (this->current_tok.type == TokenType::INCREMENT || this->current_tok.type == TokenType::DECREMENT) {
            Token op = this->current_tok;
            this->advance();
            AnyNode value_node = new UnaryOpNode(op, base, true);
            return res.success(value_node);
        }
        while (true) {
            if (this->current_tok.type == TokenType::LPAREN) {
                base = res.reg(this->call(base));
            } else if (this->current_tok.type == TokenType::LBRACKET) {
                std::vector<AnyNode> indices;
                while (this->current_tok.type == TokenType::LBRACKET) {
                    this->advance();
                    AnyNode index = res.reg(this->text_unops());
                    if (res.error) return res.to_prs();
                    if (this->current_tok.type != TokenType::RBRACKET) {
                        res.failure(new InvalidSyntaxError("QC-S049: Expected ']'", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();
                    indices.push_back(index);
                }
                base = new ArrayAccessNode(base, indices);
            } else if (this->current_tok.type == TokenType::DOT || this->current_tok.type == TokenType::ARROW) {
                if (this->current_tok.type == TokenType::ARROW) {
                    this->advance();

                    if (this->current_tok.type != TokenType::IDENTIFIER) {
                        res.failure(new InvalidSyntaxError("QC-S037: Expected property or method name after '->'", this->current_tok.pos));
                        return res.to_prs();
                    }

                    Token property_name = this->current_tok;
                    this->advance();
                    if (this->current_tok.type == TokenType::LESS) {
                        size_t oldId = this->index;
                        std::string oldName = property_name.value;
                        this->advance();
                        property_name.value += "<";
                        int depth = 1;
                        if (this->current_tok.type == TokenType::MORE) {
                            property_name.value = oldName;
                            this->index = oldId;
                            this->current_tok = this->tokens[this->index];
                            depth = 0;
                        }
                        bool next_comma = false;
                        bool just_incremented = true;
                        while (depth > 0) {
                            if (next_comma) {
                                if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                                    this->current_tok.type != TokenType::MORE) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                            } else {
                                if (this->current_tok.type == TokenType::COMMA) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                            }
                            if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING,
                                                                 TokenType::INT, TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR,
                                                                 TokenType::ADDR_T, TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT,
                                                                 TokenType::SHORT_INT, TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE,
                                                                 TokenType::BYTE, TokenType::NIBBLE})
                                      .contains(this->current_tok.type))) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                            if (this->current_tok.type == TokenType::EOFT) {
                                res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", this->current_tok.pos));
                                return res.to_prs();
                            }
                            if (this->current_tok.type == TokenType::LESS) {
                                if (just_incremented) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                                just_incremented = true;
                                depth++;
                                if (depth > 128) {
                                    res.failure(new InvalidSyntaxError(
                                        "QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                            property_name.value.substr(0, 120) + "..." +
                                            "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                            "compiler is not a Matryoshka doll. It has feelings too.",
                                        this->current_tok.pos));
                                    return res.to_prs();
                                }
                            } else if (this->current_tok.type == TokenType::MORE) {
                                depth--;
                                if (depth == 0) {
                                    this->advance();
                                    break;
                                }
                            } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                                just_incremented = false;
                                next_comma = true;
                                property_name.value += this->parseNoGenericString();
                                continue;
                            } else {
                                just_incremented = false;
                            }
                            if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                            property_name.value += this->current_tok.value;
                            this->advance();
                        }
                        if (this->index != oldId && property_name.value != oldName) property_name.value += ">";
                    }
                    Token base_name_tok;
                    if (auto var = std::get_if<VarAccessNode*>(&base)) {
                        base_name_tok = (*var)->var_name_tok;
                    } else {
                        base_name_tok = Token(TokenType::IDENTIFIER, "", property_name.pos);
                    }

                    auto deref = new UnaryOpNode(Token(TokenType::MUL, "*", property_name.pos), base);

                    if (this->current_tok.type == TokenType::LPAREN) {
                        this->advance();

                        std::vector<AnyNode> args;
                        if (this->current_tok.type != TokenType::RPAREN) {
                            while (true) {
                                if (this->current_tok.type == TokenType::AT) {
                                    this->advance();
                                    AnyNode expr = res.reg(this->text_unops());
                                    if (res.error) return res.to_prs();
                                    args.push_back(new SpreadNode(expr));
                                } else {
                                    AnyNode arg = res.reg(this->text_unops());
                                    if (res.error) return res.to_prs();
                                    args.push_back(arg);
                                }
                                if (this->current_tok.type != TokenType::COMMA) break;
                                this->advance();
                            }
                        }

                        if (this->current_tok.type != TokenType::RPAREN) {
                            res.failure(new InvalidSyntaxError("QC-S052: Expected ')' after method arguments", this->current_tok.pos));
                            return res.to_prs();
                        }
                        this->advance();

                        base = new MethodCallNode(deref, property_name, args);
                    } else {
                        base = new PropertyAccessNode(deref, base_name_tok, property_name);
                    }
                } else if (this->current_tok.type == TokenType::DOT) {
                    this->advance();

                    if (this->current_tok.type != TokenType::IDENTIFIER) {
                        res.failure(new InvalidSyntaxError("QC-S053: Expected property or method name after '.'", this->current_tok.pos));
                        return res.to_prs();
                    }

                    Token property_name = this->current_tok;
                    this->advance();
                    if (this->current_tok.type == TokenType::LESS) {
                        size_t oldId = this->index;
                        std::string oldName = property_name.value;
                        this->advance();
                        property_name.value += "<";
                        int depth = 1;
                        if (this->current_tok.type == TokenType::MORE) {
                            property_name.value = oldName;
                            this->index = oldId;
                            this->current_tok = this->tokens[this->index];
                            depth = 0;
                        }
                        bool next_comma = false;
                        bool just_incremented = true;
                        while (depth > 0) {
                            if (next_comma) {
                                if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                                    this->current_tok.type != TokenType::MORE) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                            } else {
                                if (this->current_tok.type == TokenType::COMMA) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                            }
                            if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING,
                                                                 TokenType::INT, TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR,
                                                                 TokenType::ADDR_T, TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT,
                                                                 TokenType::SHORT_INT, TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE,
                                                                 TokenType::BYTE, TokenType::NIBBLE})
                                      .contains(this->current_tok.type))) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                            if (this->current_tok.type == TokenType::EOFT) {
                                res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", this->current_tok.pos));
                                return res.to_prs();
                            }
                            if (this->current_tok.type == TokenType::LESS) {
                                if (just_incremented) {
                                    this->index = oldId;
                                    property_name.value = oldName;
                                    this->current_tok = this->tokens[this->index];
                                    break;
                                }
                                just_incremented = true;
                                depth++;
                                if (depth > 128) {
                                    res.failure(new InvalidSyntaxError(
                                        "QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                            property_name.value.substr(0, 120) + "..." +
                                            "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                            "compiler is not a Matryoshka doll. It has feelings too.",
                                        this->current_tok.pos));
                                    return res.to_prs();
                                }
                            } else if (this->current_tok.type == TokenType::MORE) {
                                depth--;
                                if (depth == 0) {
                                    this->advance();
                                    break;
                                }
                            } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                                just_incremented = false;
                                next_comma = true;
                                property_name.value += this->parseNoGenericString();
                                continue;
                            } else {
                                just_incremented = false;
                            }
                            if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                            property_name.value += this->current_tok.value;
                            this->advance();
                        }
                        if (this->index != oldId && property_name.value != oldName) property_name.value += ">";
                    }
                    if (this->current_tok.type == TokenType::LPAREN) {
                        this->advance();
                        std::vector<AnyNode> args;

                        if (this->current_tok.type != TokenType::RPAREN) {
                            while (true) {
                                if (this->current_tok.type == TokenType::AT) {
                                    this->advance();
                                    AnyNode expr = res.reg(this->text_unops());
                                    if (res.error) return res.to_prs();
                                    args.push_back(new SpreadNode(expr));
                                } else {
                                    AnyNode arg = res.reg(this->text_unops());
                                    if (res.error) return res.to_prs();
                                    args.push_back(arg);
                                }
                                if (this->current_tok.type != TokenType::COMMA) break;
                                this->advance();
                            }
                        }

                        if (this->current_tok.type != TokenType::RPAREN) {
                            res.failure(new InvalidSyntaxError("QC-S044: Expected ')' after function arguments", this->current_tok.pos));
                            return res.to_prs();
                        }
                        this->advance();

                        base = new MethodCallNode(base, property_name, args);
                    } else {
                        base = new PropertyAccessNode(base, ident, property_name);
                    }

                    while (this->current_tok.type == TokenType::LBRACKET) {
                        std::vector<AnyNode> indices;
                        while (this->current_tok.type == TokenType::LBRACKET) {
                            this->advance();
                            AnyNode index = res.reg(this->text_unops());
                            if (res.error) return res.to_prs();
                            if (this->current_tok.type != TokenType::RBRACKET) {
                                res.failure(new InvalidSyntaxError("QC-S049: Expected ']'", this->current_tok.pos));
                                return res.to_prs();
                            }
                            this->advance();
                            indices.push_back(index);
                        }
                        base = new ArrayAccessNode(base, indices);
                    }
                }
            } else if (this->current_tok.type == TokenType::INCREMENT || this->current_tok.type == TokenType::DECREMENT) {
                Token op = this->current_tok;
                this->advance();
                AnyNode value_node = new UnaryOpNode(op, base, true);
                return res.success(value_node);
            } else {
                break;
            }
            if (res.error) return res.to_prs();
        }
        if (is_known_type(name) && std::holds_alternative<VarAccessNode*>(base)) {
            AnyNode node = TypeValueNode(Token(TokenType::KEYWORD, name, tok.pos));
            return res.success(modifiers.empty() ? node : new ModifierNode(modifiers, node));
        }
        return res.success(modifiers.empty() ? base : new ModifierNode(modifiers, base));
    } else if (tok.type == TokenType::LPAREN) {
        this->advance();
        AnyNode any_expr = res.reg(this->text_unops());
        if (res.error) return res.to_prs();

        if (this->current_tok.type == TokenType::RPAREN) {
            this->advance();
            AnyNode base = any_expr;

            while (this->current_tok.type == TokenType::DOT) {
                this->advance();

                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-S053: Expected property or method name after '.'", this->current_tok.pos));
                    return res.to_prs();
                }

                Token property_name = this->current_tok;
                this->advance();
                if (this->current_tok.type == TokenType::LESS) {
                    size_t oldId = this->index;
                    std::string oldName = property_name.value;
                    this->advance();
                    property_name.value += "<";
                    int depth = 1;
                    if (this->current_tok.type == TokenType::MORE) {
                        property_name.value = oldName;
                        this->index = oldId;
                        this->current_tok = this->tokens[this->index];
                        depth = 0;
                    }
                    bool next_comma = false;
                    bool just_incremented = true;
                    while (depth > 0) {
                        if (next_comma) {
                            if (this->current_tok.type != TokenType::COMMA && this->current_tok.type != TokenType::LESS &&
                                this->current_tok.type != TokenType::MORE) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        } else {
                            if (this->current_tok.type == TokenType::COMMA) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                        }
                        if (!(std::unordered_set<TokenType>({TokenType::COMMA, TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::STRING,
                                                             TokenType::INT, TokenType::DOUBLE, TokenType::FLOAT, TokenType::CHAR, TokenType::ADDR_T,
                                                             TokenType::BOOL, TokenType::QBOOL, TokenType::LONG_INT, TokenType::SHORT_INT,
                                                             TokenType::LONG_DOUBLE, TokenType::LESS, TokenType::MORE, TokenType::BYTE,
                                                             TokenType::NIBBLE})
                                  .contains(this->current_tok.type))) {
                            this->index = oldId;
                            property_name.value = oldName;
                            this->current_tok = this->tokens[this->index];
                            break;
                        }
                        if (this->current_tok.type == TokenType::EOFT) {
                            res.failure(new InvalidSyntaxError("QC-G001: Unterminated generic argument list", this->current_tok.pos));
                            return res.to_prs();
                        }
                        if (this->current_tok.type == TokenType::LESS) {
                            if (just_incremented) {
                                this->index = oldId;
                                property_name.value = oldName;
                                this->current_tok = this->tokens[this->index];
                                break;
                            }
                            just_incremented = true;
                            depth++;
                            if (depth > 128) {
                                res.failure(
                                    new InvalidSyntaxError("QC-G002: Generic nesting exceeds maximum depth of 128.\n\nNote: While expanding:\n    " +
                                                               property_name.value.substr(0, 120) + "..." +
                                                               "\n\nNote: We opened the box and there was another box. And another. Please stop. The "
                                                               "compiler is not a Matryoshka doll. It has feelings too.",
                                                           this->current_tok.pos));
                                return res.to_prs();
                            }
                        } else if (this->current_tok.type == TokenType::MORE) {
                            depth--;
                            if (depth == 0) {
                                this->advance();
                                break;
                            }
                        } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                            just_incremented = false;
                            next_comma = true;
                            property_name.value += this->parseNoGenericString();
                            continue;
                        } else {
                            just_incremented = false;
                        }
                        if (this->current_tok.type != TokenType::MORE) next_comma = !next_comma;
                        property_name.value += this->current_tok.value;
                        this->advance();
                    }
                    if (this->index != oldId && property_name.value != oldName) property_name.value += ">";
                }
                Token base_name_tok;
                if (auto unary = std::get_if<UnaryOpNode*>(&base)) {
                    if ((*unary)->op_tok.type == TokenType::MUL) {
                        if (auto var = std::get_if<VarAccessNode*>(&(*unary)->node)) {
                            base_name_tok = (*var)->var_name_tok;
                        } else {
                            base_name_tok = Token(TokenType::IDENTIFIER, "", property_name.pos);
                        }
                    } else {
                        base_name_tok = Token(TokenType::IDENTIFIER, "", property_name.pos);
                    }
                } else if (auto var = std::get_if<VarAccessNode*>(&base)) {
                    base_name_tok = (*var)->var_name_tok;
                } else {
                    base_name_tok = Token(TokenType::IDENTIFIER, "", property_name.pos);
                }

                if (this->current_tok.type == TokenType::LPAREN) {
                    this->advance();
                    std::vector<AnyNode> args;
                    if (this->current_tok.type != TokenType::RPAREN) {
                        while (true) {
                            if (this->current_tok.type == TokenType::AT) {
                                this->advance();
                                AnyNode expr = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(new SpreadNode(expr));
                            } else {
                                AnyNode arg = res.reg(this->text_unops());
                                if (res.error) return res.to_prs();
                                args.push_back(arg);
                            }
                            if (this->current_tok.type != TokenType::COMMA) break;
                            this->advance();
                        }
                    }

                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S052: Expected ')' after method arguments", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();

                    base = new MethodCallNode(base, property_name, args);
                } else {
                    base = new PropertyAccessNode(base, base_name_tok, property_name);
                }
            }

            return res.success(modifiers.empty() ? base : new ModifierNode(modifiers, base));
        } else {
            res.failure(new InvalidSyntaxError("QC-S050: Expected ')'", this->current_tok.pos));
            return res.to_prs();
        }
    } else if (tok.type == TokenType::FSTRING) {
        this->advance();

        std::vector<std::string> parts;
        std::vector<std::string> exprs;
        std::string current = "";
        bool in_expr = false;

        for (char c : tok.value) {
            if (c == '\x01') {
                if (in_expr)
                    exprs.push_back(current);
                else
                    parts.push_back(current);
                current = "";
                in_expr = !in_expr;
            } else {
                current += c;
            }
        }

        if (!current.empty()) {
            if (in_expr)
                exprs.push_back(current);
            else
                parts.push_back(current);
        }

        if (parts.empty()) parts.push_back("");

        AnyNode result = StringNode(Token(TokenType::STRING, parts[0], tok.pos));

        for (size_t i = 0; i < exprs.size(); ++i) {
            Lexer expr_lexer(exprs[i], "<fstring>");
            auto expr_tokens = expr_lexer.make_tokens();
            Parser expr_parser(expr_tokens.Tkns);
            AnyNode expr_node = res.reg(expr_parser.logical_or());
            if (res.error) return res.to_prs();

            result = new BinOpNode(result, Token(TokenType::PLUS, "+", tok.pos), expr_node, true);

            if (i + 1 < parts.size()) {
                result = new BinOpNode(result, Token(TokenType::PLUS, "+", tok.pos), StringNode(Token(TokenType::STRING, parts[i + 1], tok.pos)),
                                       true);
            }
        }

        return res.success(result);
    } else if (tok.type == TokenType::KEYWORD && tok.value == "fn") {
        this->advance();

        if (this->current_tok.type != TokenType::LPAREN) {
            res.failure(new InvalidSyntaxError("QC-S051: Expected '(' after 'fn'", this->current_tok.pos));
            return res.to_prs();
        }

        auto saved_index = this->index;

        int paren_depth = 1;
        this->advance();

        while (paren_depth > 0 && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type == TokenType::LPAREN) paren_depth++;
            if (this->current_tok.type == TokenType::RPAREN) paren_depth--;
            this->advance();
        }

        std::vector<Token> return_types;

        if (this->current_tok.type == TokenType::ARROW) {
            this->advance();
            return_types.push_back(Token(TokenType::KEYWORD, parseTypeString(), this->current_tok.pos));
            this->advance();

            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                return_types.push_back(Token(TokenType::KEYWORD, parseTypeString(), this->current_tok.pos));
                this->advance();
            }
        }
        this->index = saved_index;
        this->current_tok = this->tokens[this->index];

        auto fn_pr = this->func_def_multi(return_types, std::nullopt, {}, false);

        if (std::holds_alternative<Error*>(fn_pr)) return fn_pr;

        AnyNode fn_node;
        if (std::holds_alternative<FuncDefNode*>(fn_pr))
            fn_node = std::get<FuncDefNode*>(fn_pr);
        else
            fn_node = std::get<FuncDefNode*>(fn_pr);

        if (this->current_tok.type == TokenType::LPAREN) return this->call(fn_node);

        return res.success(fn_node);
    } else if (tok.type == TokenType::KEYWORD) {
        return res.success(TypeValueNode(Token(TokenType::KEYWORD, this->parseTypeString(), tok.pos)));
    }
    return res.success(std::monostate{});
}

Prs Parser::power() {
    ParseResult res;
    AnyNode left = res.reg(this->atom());
    if (res.error) return res.to_prs();

    if (this->current_tok.type == TokenType::POWER) {
        Token op_tok = this->current_tok;
        this->advance();

        AnyNode right = res.reg(this->factor());
        if (res.error) return res.to_prs();
        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::factor() {
    ParseResult res;
    Token tok = this->current_tok;

    if (tok.type == TokenType::PLUS || tok.type == TokenType::MINUS || tok.type == TokenType::BITWISE_NOT || tok.type == TokenType::NOT ||
        tok.type == TokenType::QNOT || tok.type == TokenType::AMPERSAND || tok.type == TokenType::MUL) {

        this->advance();
        AnyNode factor_node = res.reg(this->text_unops());
        if (res.error) return res.to_prs();
        return res.success(new UnaryOpNode(tok, factor_node));
    }
    if (this->current_tok.type == TokenType::INCREMENT || this->current_tok.type == TokenType::DECREMENT) {
        Token op = this->current_tok;
        this->advance();
        AnyNode operand = res.reg(this->factor());
        if (res.error) return res.to_prs();
        return res.success(new UnaryOpNode(op, operand, false));
    }
    return this->power();
}
Prs Parser::term() {
    return this->bin_op([this]() { return this->factor(); }, {TokenType::DIV, TokenType::MUL, TokenType::MOD});
}
Prs Parser::bitwise() {
    return this->bin_op([this]() { return this->term(); },
                        {TokenType::RSHIFT, TokenType::LSHIFT, TokenType::R_ROT, TokenType::L_ROT, TokenType::LOGICAL_RSHIFT});
}
Prs Parser::logical_and() {
    ParseResult res;
    AnyNode left = res.reg(this->comparison());
    if (res.error) return res.to_prs();

    while (this->current_tok.type == TokenType::AND || this->current_tok.type == TokenType::AMPERSAND) {
        Token op_tok = this->current_tok;
        this->advance();
        AnyNode right = res.reg(this->comparison());
        if (res.error) return res.to_prs();

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}

Prs Parser::logical_or() {
    ParseResult res;
    AnyNode left = res.reg(this->logical_and());
    if (res.error) return res.to_prs();
    while (this->current_tok.type == TokenType::OR || this->current_tok.type == TokenType::XOR || this->current_tok.type == TokenType::PIPE ||
           this->current_tok.type == TokenType::BITWISE_XOR) {
        Token op_tok = this->current_tok;
        this->advance();
        AnyNode right = res.reg(this->logical_and());
        if (res.error) return res.to_prs();

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::ternary() {
    ParseResult res;
    AnyNode cond = res.reg(this->logical_or());
    if (res.error) return res.to_prs();
    if (this->current_tok.type == TokenType::QUESTION) {
        Token question_mark = this->current_tok;
        this->advance();
        AnyNode left = res.reg(this->ternary());
        if (res.error) return res.to_prs();
        if (this->current_tok.type != TokenType::COLON) {
            res.failure(new InvalidSyntaxError("QC-S054: Expected : after if-true value in ternary expression", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        AnyNode right = res.reg(this->ternary());
        if (res.error) return res.to_prs();
        question_mark.value = "`ternary";
        cond = new CallNode(new VarAccessNode(question_mark), {cond, left, right});
    }
    return res.success(cond);
}
Prs Parser::text_unops() {
    ParseResult res;
    if (this->current_tok.type == TokenType::SIZEOF || this->current_tok.type == TokenType::THROW) {
        Token tok = this->current_tok;
        this->advance();
        AnyNode factor_node = res.reg(this->ternary());
        if (res.error) return res.to_prs();
        return res.success(new UnaryOpNode(tok, factor_node));
    }
    AnyNode n = res.reg(this->ternary());
    if (res.error) return res.to_prs();
    return res.success(n);
}
Prs Parser::qin_expr() {
    ParseResult res;

    AnyNode left = QInNode{};

    while (current_tok.type == TokenType::RSHIFT) {
        Token op_tok = current_tok;
        advance();

        AnyNode right = res.reg(this->bitwise());
        if (!std::holds_alternative<VarAccessNode*>(right)) {
            res.failure(new InvalidSyntaxError("QC-S055: Right-hand side of >> must be a variable", op_tok.pos));
            return res.to_prs();
        }

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::comparison() {
    ParseResult res;
    AnyNode left = res.reg(this->expr());
    if (res.error) return res.to_prs();
    if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "proved_by" ||
        (this->current_tok.type == TokenType::AS && this->current_tok.value == "as")) {
        Token op_tok = this->current_tok;
        this->advance();
        AnyNode right = res.reg(this->expr());
        if (res.error) return res.to_prs();
        left = new BinOpNode(left, op_tok, right);
        return res.success(left);
    }
    while (this->current_tok.type == TokenType::EQ_TO || this->current_tok.type == TokenType::NOT_EQ || this->current_tok.type == TokenType::LESS ||
           this->current_tok.type == TokenType::LESS_EQ || this->current_tok.type == TokenType::MORE ||
           this->current_tok.type == TokenType::MORE_EQ || this->current_tok.type == TokenType::QEQEQ || this->current_tok.type == TokenType::QNEQ ||
           this->current_tok.type == TokenType::QAND || this->current_tok.type == TokenType::QOR || this->current_tok.type == TokenType::QXOR ||
           this->current_tok.type == TokenType::COLLAPSE_AND || this->current_tok.type == TokenType::COLLAPSE_OR) {

        Token op_tok = this->current_tok;
        this->advance();
        AnyNode right = res.reg(this->expr());
        if (res.error) return res.to_prs();

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::expr() {
    ParseResult res;
    AnyNode left = res.reg(this->bitwise());
    if (res.error) return res.to_prs();

    while (current_tok.type != TokenType::SEMICOLON && (current_tok.type == TokenType::PLUS || current_tok.type == TokenType::MINUS)) {
        Token op_tok = current_tok;
        advance();

        AnyNode right = res.reg(this->bitwise());
        if (res.error) return res.to_prs();

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::bin_op(std::function<Prs()> func, std::initializer_list<TokenType> ops) {
    ParseResult res;
    AnyNode left = res.reg(func());
    if (res.error) return res.to_prs();

    while (std::find(ops.begin(), ops.end(), this->current_tok.type) != ops.end()) {
        Token op_tok = this->current_tok;
        this->advance();

        AnyNode right = res.reg(func());
        if (res.error) return res.to_prs();

        left = new BinOpNode(left, op_tok, right);
    }

    return res.success(left);
}
Prs Parser::return_stmt() {
    ParseResult res;
    Position start_pos = this->current_tok.pos;

    this->advance();

    if (this->current_tok.type == TokenType::SEMICOLON) {
        this->advance();
        return res.success(new ReturnNode(std::monostate{}, start_pos));
    }

    std::vector<AnyNode> values;
    values.push_back(res.reg(this->text_unops()));
    if (res.error) { return res.to_prs(); }

    while (this->current_tok.type == TokenType::COMMA) {
        this->advance();
        values.push_back(res.reg(this->text_unops()));
        if (res.error) return res.to_prs();
    }

    if (this->current_tok.type != TokenType::SEMICOLON) {
        res.failure(new MissingSemicolonError(this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();

    if (values.size() == 1) { return res.success(new ReturnNode(values[0], start_pos)); }

    return res.success(new MultiReturnNode(values, start_pos));
}
Prs Parser::assignment_expr() {
    ParseResult res;

    AnyNode left = res.reg(this->text_unops());

    if (res.error) return res.to_prs();

    if (this->current_tok.type == TokenType::EQ || this->current_tok.type == TokenType::PLUS_EQ || this->current_tok.type == TokenType::MINUS_EQ ||
        this->current_tok.type == TokenType::MUL_EQ || this->current_tok.type == TokenType::DIV_EQ || this->current_tok.type == TokenType::MOD_EQ ||
        this->current_tok.type == TokenType::RSH_EQ || this->current_tok.type == TokenType::LSH_EQ || this->current_tok.type == TokenType::LRSH_EQ ||
        this->current_tok.type == TokenType::RROT_EQ || this->current_tok.type == TokenType::LROT_EQ ||
        this->current_tok.type == TokenType::BIT_X_EQ || this->current_tok.type == TokenType::BIT_O_EQ ||
        this->current_tok.type == TokenType::BIT_A_EQ) {
        bool is_var = std::holds_alternative<VarAccessNode*>(left);
        bool is_array_access = std::holds_alternative<ArrayAccessNode*>(left);
        bool is_prop = std::holds_alternative<PropertyAccessNode*>(left);
        bool is_deref = std::holds_alternative<UnaryOpNode*>(left) && std::get<UnaryOpNode*>(left)->op_tok.type == TokenType::MUL;
        Token op_tok = this->current_tok;
        this->advance();
        AnyNode right;
        if (op_tok.type == TokenType::EQ) {
            size_t next_i = index + 1;
            if (this->current_tok.type == TokenType::IDENTIFIER && next_i < tokens.size() && tokens[next_i].type == TokenType::EQ) {
                right = res.reg(this->assignment_expr());
            } else {
                right = res.reg(this->text_unops());
            }
        } else {
            right = res.reg(this->text_unops());
        }

        if (res.error) return res.to_prs();
        if (is_prop) {
            auto& prop = std::get<PropertyAccessNode*>(left);
            Token field = prop->property_name;

            TokenType binop_type;
            switch (op_tok.type) {
            case TokenType::EQ: binop_type = TokenType::EQ; break;
            case TokenType::PLUS_EQ: binop_type = TokenType::PLUS; break;
            case TokenType::MINUS_EQ: binop_type = TokenType::MINUS; break;
            case TokenType::MUL_EQ: binop_type = TokenType::MUL; break;
            case TokenType::DIV_EQ: binop_type = TokenType::DIV; break;
            case TokenType::MOD_EQ: binop_type = TokenType::MOD; break;
            case TokenType::RSH_EQ: binop_type = TokenType::RSHIFT; break;
            case TokenType::LSH_EQ: binop_type = TokenType::LSHIFT; break;
            case TokenType::LRSH_EQ: binop_type = TokenType::LOGICAL_RSHIFT; break;
            case TokenType::RROT_EQ: binop_type = TokenType::R_ROT; break;
            case TokenType::LROT_EQ: binop_type = TokenType::L_ROT; break;
            case TokenType::BIT_X_EQ: binop_type = TokenType::BITWISE_XOR; break;
            case TokenType::BIT_O_EQ: binop_type = TokenType::PIPE; break;
            case TokenType::BIT_A_EQ: binop_type = TokenType::AMPERSAND; break;
            default: res.failure(new InvalidSyntaxError("QC-S057: Unsupported op for struct fields", op_tok.pos)); return res.to_prs();
            }
            AnyNode lhsBase = *(prop->base);
            if (binop_type == TokenType::EQ) { return res.success(new FieldAssignNode(lhsBase, field, right)); }
            AnyNode rhsBase = *(prop->base);
            auto readProp = new PropertyAccessNode(rhsBase, prop->base_name_tok, field);
            AnyNode readPropNode = readProp;
            Token bin_tok(binop_type, get_token_name(binop_type), op_tok.pos);
            AnyNode binExpr = AnyNode{new BinOpNode(readPropNode, bin_tok, right)};
            return res.success(new FieldAssignNode(lhsBase, field, binExpr));
        }
        if (is_array_access) {
            if (op_tok.type != TokenType::EQ) {
                res.failure(new InvalidSyntaxError("QC-S058: Compound assignment (+=, -=, etc.) not supported "
                                                   "for "
                                                   "array access",
                                                   op_tok.pos));
                return res.to_prs();
            }

            return res.success(new ArrayAssignNode(left, right));
        }

        return res.success(new AssignExprNode(left, op_tok, right));
    }

    return res.success(left);
}
static bool is_statement_node(const Prs& st) {
    return std::visit(
        [](auto arg) -> bool {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, Error*>) { return true; }

            if constexpr (std::is_same_v<T, ParseResult> || std::is_same_v<T, std::monostate>) { return false; }

            return true;
        },
        st);
}
AnyNode to_any_node(Prs prs) {
    return std::visit(
        [](auto arg) -> AnyNode {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, Error*>) {
                return std::monostate{};
            } else if constexpr (std::is_same_v<T, ParseResult>) {
                return arg.node;
            } else if constexpr (std::is_same_v<T, BinOpNode*> || std::is_same_v<T, UnaryOpNode*> || std::is_same_v<T, VarAccessNode*> ||
                                 std::is_same_v<T, VarAssignNode*> || std::is_same_v<T, AssignExprNode*> || std::is_same_v<T, IfNode*> ||
                                 std::is_same_v<T, StatementsNode*> || std::is_same_v<T, SwitchNode*> || std::is_same_v<T, BreakNode*> ||
                                 std::is_same_v<T, WhileNode*> || std::is_same_v<T, ForNode*> || std::is_same_v<T, ContinueNode*> ||
                                 std::is_same_v<T, CallNode*> || std::is_same_v<T, FuncDefNode*> || std::is_same_v<T, ReturnNode*> ||
                                 std::is_same_v<T, MultiReturnNode*> || std::is_same_v<T, MultiVarDeclNode*> || std::is_same_v<T, ArrayDeclNode*> ||
                                 std::is_same_v<T, ArrayLiteralNode*> || std::is_same_v<T, ArrayAccessNode*> || std::is_same_v<T, QIfNode*> ||
                                 std::is_same_v<T, QSwitchNode*> || std::is_same_v<T, FieldAssignNode*> || std::is_same_v<T, MapLiteralNode*> ||
                                 std::is_same_v<T, NamespaceNode*> || std::is_same_v<T, TryCatchNode*> || std::is_same_v<T, UnreachableNode*>) {
                return arg;
            } else {
                return arg;
            }
        },
        prs);
}
Parameter Parser::parse_parameter(bool type_only = false) {
    Parameter p;
    if (this->current_tok.value == "fn" && this->current_tok.type == TokenType::KEYWORD) {
        p.type = this->current_tok;
        this->advance();

        if (this->current_tok.type != TokenType::LPAREN) { throw new InvalidSyntaxError("QC-S056: Expected '(' after 'fn'", this->current_tok.pos); }
        this->advance();

        Parameter::FunctionSignature sig;
        if (this->current_tok.type != TokenType::RPAREN) {
            while (true) {
                sig.params.push_back(this->parse_parameter());
                if (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                } else {
                    break;
                }
            }
        }

        if (this->current_tok.type != TokenType::RPAREN) { throw new InvalidSyntaxError("QC-S059: Expected ')'", this->current_tok.pos); }
        this->advance();
        if (this->current_tok.type == TokenType::ARROW) {
            this->advance();
            if (this->current_tok.type == TokenType::LPAREN) {
                this->advance();
                while (true) {
                    sig.return_types.push_back(this->parse_parameter(true).type);
                    if (this->current_tok.type != TokenType::COMMA) break;
                    this->advance();
                }
                if (this->current_tok.type != TokenType::RPAREN) { throw new InvalidSyntaxError("QC-S059: Expected ')'", this->current_tok.pos); }
                this->advance();
            } else {
                auto temp_p = this->parse_parameter(true);
                sig.return_types.push_back(temp_p.type);
            }
        }
        p.signature = sig;
    } else {
        p.type = this->current_tok;
        if (!(p.type.type == TokenType::VARADIC)) {
            p.type.value = "";
            while (this->current_tok.type == TokenType::KEYWORD) {
                if (std::unordered_set<std::string>({"out", "inout", "volatile"}).contains(this->current_tok.value)) {
                    if (this->current_tok.value == "volatile") {
                        p.isVolatile = true;
                    } else {
                        p.type.value += this->current_tok.value + " ";
                    }
                    this->advance();
                } else {
                    break;
                }
            }
            p.type.value += parseTypeString();
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "restrict") {
                p.type.value += "restrict";
                this->advance();
            }
        } else {
            this->advance();
            p.name = Token(TokenType::IDENTIFIER, "<varadic>", this->current_tok.pos);
        }
    }
    if (!type_only) {
        if (this->current_tok.type == TokenType::IDENTIFIER) {
            p.name = this->current_tok;
            this->advance();
        }
        if (this->current_tok.type == TokenType::EQ) {
            this->advance();
            Prs val = this->text_unops();
            p.default_value = to_any_node(val);
        }
    }
    return p;
}
Prs Parser::func_def_multi(std::vector<Token> return_types, std::optional<Token> func_name, std::vector<GenericType> generics, bool keep,
                           bool is_volatile, std::vector<Token> modifiers) {
    ParseResult res;
    std::vector<GenericType> old_generics = this->current_generics;
    if (keep) {
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
    } else {
        this->current_generics = generics;
    }
    this->advance();
    std::vector<Parameter> params;
    if (this->current_tok.type != TokenType::RPAREN) {
        while (true) {
            params.push_back(this->parse_parameter());
            if (params.back().type.type == TokenType::VARADIC) { break; }
            if (this->current_tok.type == TokenType::COMMA) {
                this->advance();
            } else {
                break;
            }
        }
    }
    if (this->current_tok.type != TokenType::RPAREN) {
        res.failure(new InvalidSyntaxError("QC-S064: Expected ')' after parameters", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (this->current_tok.type == TokenType::ARROW) {
        this->advance();
        while (true) {
            if (this->current_tok.type == TokenType::SCOPE) {
                this->advance();
                if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) { this->advance(); }
            } else if (this->current_tok.type == TokenType::LESS) {
                int depth = 1;
                this->advance();
                while (depth > 0 && this->current_tok.type != TokenType::EOFT) {
                    if (this->current_tok.type == TokenType::LESS)
                        depth++;
                    else if (this->current_tok.type == TokenType::MORE)
                        depth--;
                    this->advance();
                }
            } else {
                break;
            }
        }
        if (this->current_tok.type == TokenType::KEYWORD || is_known_type(this->current_tok.value) > 0) {
            this->parse_parameter(true);
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                this->parse_parameter(true);
                if (this->current_tok.type == TokenType::KEYWORD || is_known_type(this->current_tok.value) > 0) {
                    this->advance();
                    this->parse_parameter(true);
                }
            }
        }
    }
    if (this->current_tok.type != TokenType::LBRACE) {
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            std::list<Parameter> params_list((params.begin()), (params.end()));
            std::vector<AnyNode> body;
            body.emplace_back(std::monostate{});
            return res.success(new FuncDefNode(return_types, func_name, params_list, new StatementsNode(body), currentNamespace, this->in_extern,
                                               this->in_foreign, generics, is_volatile, true, modifiers));
        }
        res.failure(new InvalidSyntaxError("QC-S003: Expected '{' to start function body", this->current_tok.pos));
        return res.to_prs();
    }
    this->advance();
    if (this->in_foreign) {
        res.failure(new InvalidSyntaxError("QC-S003: Expected no function body for a foreign function.", this->current_tok.pos));
        return res.to_prs();
    }
    std::vector<AnyNode> body_stmts;
    while (true) {
        if (this->current_tok.type == TokenType::RBRACE) break;

        if (this->current_tok.type == TokenType::EOFT) {
            res.failure(new InvalidSyntaxError("QC-S060: Unexpected end of file in function body", this->current_tok.pos));
            return res.to_prs();
        }
        Prs st = this->statement();

        if (std::holds_alternative<Error*>(st)) {
            res.failure(std::get<Error*>(st));
            return res.to_prs();
        }

        if (!is_statement_node(st)) { continue; }

        AnyNode node = to_any_node(st);

        if (std::holds_alternative<std::monostate>(node)) { continue; }

        body_stmts.push_back(node);
    }

    if (this->current_tok.type != TokenType::RBRACE) {
        res.failure(new InvalidSyntaxError("QC-S062: Expected '}' to end function body", this->current_tok.pos));
        return res.to_prs();
    }
    auto body = new StatementsNode(body_stmts, true);
    this->advance();
    std::list<Parameter> params_list((params.begin()), (params.end()));
    this->current_generics = old_generics;
    return res.success(new FuncDefNode(return_types, func_name, params_list, body, currentNamespace, this->in_extern, this->in_foreign, generics,
                                       is_volatile, false, modifiers));
}
Prs Parser::statement() {
    ParseResult res;
    Token tok = this->current_tok;
    if (tok.type == TokenType::KEYWORD && tok.value == "extern") {
        this->advance();
        if (this->current_tok.type == TokenType::COLON) {
            this->advance();
            tok = this->current_tok;
            this->in_extern = true;
        } else {
            res.failure(new InvalidSyntaxError("QC-S063: Expected ':' after 'extern'", this->current_tok.pos));
            return res.to_prs();
        }
    }
    if (tok.type == TokenType::COLON && peek().value == "extern") {
        this->advance();
        this->advance();
        tok = this->current_tok;
        this->in_extern = false;
        return res.success(std::monostate{});
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "foreign") {
        this->advance();
        if (this->current_tok.type == TokenType::COLON) {
            this->advance();
            tok = this->current_tok;
            this->in_foreign = true;
        } else {
            res.failure(new InvalidSyntaxError("QC-S066: Expected ':' after 'foreign'", this->current_tok.pos));
            return res.to_prs();
        }
    }
    if (tok.type == TokenType::COLON && peek().value == "foreign") {
        this->advance();
        this->advance();
        tok = this->current_tok;
        this->in_foreign = false;
        return res.success(std::monostate{});
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "fn") {
        this->advance();

        if (this->current_tok.type != TokenType::LPAREN) {
            res.failure(new InvalidSyntaxError("QC-S056: Expected '(' after 'fn'", this->current_tok.pos));
            return res.to_prs();
        }

        auto saved_index = this->index;

        int paren_depth = 1;
        this->advance();

        while (paren_depth > 0 && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type == TokenType::LPAREN) paren_depth++;
            if (this->current_tok.type == TokenType::RPAREN) paren_depth--;
            this->advance();
        }

        std::vector<Token> return_types;

        if (this->current_tok.type == TokenType::ARROW) {
            this->advance();

            if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER &&
                std::none_of(this->current_generics.begin(), this->current_generics.end(),
                             [this](GenericType t) { return t.name == this->current_tok.value; })) {
                res.failure(new InvalidSyntaxError("QC-T001: Expected return type after '->'", this->current_tok.pos));
                return res.to_prs();
            }
            Token saved_tok = this->current_tok;
            std::string type_str = parseTypeString();
            saved_tok.value = type_str;
            return_types.push_back(saved_tok);
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER &&
                    std::none_of(this->current_generics.begin(), this->current_generics.end(),
                                 [this](GenericType t) { return t.name == this->current_tok.value; })) {
                    res.failure(new InvalidSyntaxError("QC-T002: Expected return type after ','", this->current_tok.pos));
                    return res.to_prs();
                }
                Token saved_tok = this->current_tok;
                std::string type_str = parseTypeString();
                saved_tok.value = type_str;
                return_types.push_back(saved_tok);
            }
        } else {
            return_types.push_back(Token(TokenType::KEYWORD, "void", tok.pos));
        }

        this->index = saved_index;
        this->current_tok = this->tokens[this->index];

        auto fn_pr = this->func_def_multi(return_types, std::nullopt, {}, false);

        if (std::holds_alternative<Error*>(fn_pr)) return fn_pr;

        AnyNode fn_node;
        if (std::holds_alternative<FuncDefNode*>(fn_pr))
            fn_node = std::get<FuncDefNode*>(fn_pr);
        else
            fn_node = std::get<FuncDefNode*>(fn_pr);

        if (this->current_tok.type == TokenType::LPAREN) return this->call(fn_node);
        if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
        return res.success(fn_node);
    }
    bool is_comptime = false;
    if (tok.type == TokenType::KEYWORD && tok.value == "comptime") {
        is_comptime = true;
        this->advance();
        tok = this->current_tok;
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "defer") { return this->defer_expr(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "if") { return this->if_expr(is_comptime); }
    if (tok.type == TokenType::KEYWORD && tok.value == "try") { return this->try_catch_expr(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "qif") { return this->qif_expr(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "switch") { return this->switch_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "match") { return this->match_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "qswitch") { return this->qswitch_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "while") { return this->while_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "for") { return this->for_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "do") { return this->dowhile_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "loop") { return this->loop_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "foreach") {
        this->advance();

        if (this->current_tok.type != TokenType::LPAREN) {
            res.failure(new InvalidSyntaxError("QC-S067: Expected '(' after 'foreach'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        Token elem_type = this->current_tok;
        elem_type.value = parseTypeString();
        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S068: Expected variable name in foreach", this->current_tok.pos));
            return res.to_prs();
        }
        Token elem_name = this->current_tok;
        this->advance();

        if (this->current_tok.type != TokenType::KEYWORD || this->current_tok.value != "in") {
            res.failure(new InvalidSyntaxError("QC-S069: Expected 'in' in foreach", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        AnyNode collection = res.reg(this->text_unops());
        if (res.error) { return res.to_prs(); }
        if (this->current_tok.type != TokenType::RPAREN) {
            res.failure(new InvalidSyntaxError("QC-S016: Expected ')' after foreach", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' to start foreach body", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        std::vector<AnyNode> stmts;
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            Prs st = this->statement();
            if (std::holds_alternative<Error*>(st)) {
                res.failure(std::get<Error*>(st));
                return res.to_prs();
            }
            AnyNode any_stmt = this->prs_to_anynode(st);
            stmts.push_back(any_stmt);
        }

        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S070: Expected '}' to end foreach body", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        auto body_block = new StatementsNode(stmts, true);

        return res.success(new ForeachNode(elem_type, elem_name, collection, body_block));
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "continue") {
        this->advance();
        if (current_tok.type != TokenType::SEMICOLON) {
            res.failure(new MissingSemicolonError(current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        return res.success(new ContinueNode(tok));
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "return") { return this->return_stmt(); }
    if (tok.type == TokenType::KEYWORD && tok.value == "break") {
        this->advance();
        if (current_tok.type != TokenType::SEMICOLON) {
            res.failure(new MissingSemicolonError(current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        return res.success(new BreakNode(tok));
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "unreachable") {
        this->advance();
        if (current_tok.type != TokenType::SEMICOLON) {
            res.failure(new MissingSemicolonError(current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        return res.success(new UnreachableNode(tok));
    }
    bool is_abstract_class = false;
    bool is_final_class = false;
    if (tok.type == TokenType::KEYWORD && tok.value == "abstract") {
        this->advance();
        if (this->current_tok.type != TokenType::KEYWORD || this->current_tok.value != "class") {
            res.failure(new InvalidSyntaxError("QC-S078: Expected 'class' after 'abstract'", this->current_tok.pos));
            return res.to_prs();
        }
        is_abstract_class = true;
        tok = this->current_tok;
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "final") {
        this->advance();
        if (this->current_tok.type != TokenType::KEYWORD || this->current_tok.value != "class") {
            res.failure(new InvalidSyntaxError("QC-S079: Expected 'class' after 'final'", this->current_tok.pos));
            return res.to_prs();
        }
        is_final_class = true;
        tok = this->current_tok;
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "class") {
        this->advance();
        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S086: Expected class name", this->current_tok.pos));
            return res.to_prs();
        }
        Token class_name = this->current_tok;
        this->advance();
        std::vector<GenericType> generics;
        parseGenerics(generics, res);
        if (res.error) return res.to_prs();
        auto saved_generics = this->current_generics;
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
        std::string baseName = "";
        if (this->current_tok.type == TokenType::COLON) {
            this->advance();
            if (this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-S088: Expected base class name after ':'", this->current_tok.pos));
                return res.to_prs();
            }
            baseName = this->current_tok.value;
            this->advance();
            if (this->current_tok.type == TokenType::LESS) {
                baseName += "<";
                this->advance();
                int angleDepth = 1;
                while (angleDepth > 0) {
                    if (this->current_tok.type == TokenType::LESS) {
                        angleDepth++;
                        baseName += "<";
                        this->advance();
                    } else if (this->current_tok.type == TokenType::MORE) {
                        angleDepth--;
                        baseName += ">";
                        if (angleDepth == 0) {
                            this->advance();
                            break;
                        }
                        this->advance();
                    } else if (this->current_tok.type == TokenType::COMMA) {
                        baseName += ",";
                        this->advance();
                    } else if (this->current_tok.type == TokenType::IDENTIFIER || this->current_tok.type == TokenType::KEYWORD) {
                        baseName += parseTypeString();
                    } else {
                        res.failure(
                            new InvalidSyntaxError("QC-G005: Unexpected token inside base class generic argument list", this->current_tok.pos));
                        return res.to_prs();
                    }
                }
            }
        }
        if (!baseName.empty()) {
            auto* base_ptr = find_type(base_type_name(baseName));
            if (base_ptr && base_ptr->kind == UserTypeKind::Class && base_ptr->is_final_class) {
                res.failure(new InvalidSyntaxError("QC-S089: Cannot inherit from final class '" + base_type_name(baseName) + "'", class_name.pos));
                return res.to_prs();
            }
        }
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            UserTypeInfo tempInfo;
            tempInfo.generics = generics;
            tempInfo.baseClassName = baseName;
            tempInfo.is_final_class = is_final_class;
            tempInfo.kind = UserTypeKind::Class;
            tempInfo.pos = class_name.pos;
            tempInfo.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
            std::string full_key = currentNamespace.empty() ? class_name.value : currentNamespace + "::" + class_name.value;
            user_types[full_key] = tempInfo;
            this->current_generics = saved_generics;
            return res.success(std::monostate{});
        }
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after class name", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        UserTypeInfo dummy;
        UserTypeInfo info;
        ClassField vp;
        vp.name = "__vptr";
        vp.type = (currentNamespace.empty() ? class_name.value : currentNamespace + "::" + class_name.value) + "*";
        vp.access = "public";
        info.classFields.push_back(vp);
        dummy.baseClassName = baseName;
        dummy.is_final_class = is_final_class;
        dummy.kind = UserTypeKind::Class;
        info.baseClassName = baseName;
        info.is_final_class = is_final_class;
        info.kind = UserTypeKind::Class;
        info.pos = class_name.pos;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        dummy.is_abstract_class = is_abstract_class;
        info.is_abstract_class = is_abstract_class;
        std::string full_key = currentNamespace.empty() ? class_name.value : currentNamespace + "::" + class_name.value;
        dummy.generics = generics;
        info.generics = generics;
        dummy.namespace_path = currentNamespace;
        info.namespace_path = currentNamespace;
        user_types[base_type_name(full_key)] = dummy;

        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {

            std::string access = "public";
            bool is_final_method = false;
            bool is_volatile_method = false;
            bool is_static = false;
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "final") {
                is_final_method = true;
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD &&
                (this->current_tok.value == "public" || this->current_tok.value == "private" || this->current_tok.value == "protected")) {
                access = this->current_tok.value;
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "static") {
                is_static = true;
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "final") {
                is_final_method = true;
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "volatile") {
                is_volatile_method = true;
                this->advance();
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "friend") {
                this->advance();
                info.friendClasses.push_back(parseTypeString());
                if (this->current_tok.type != TokenType::SEMICOLON) {
                    res.failure(new InvalidSyntaxError("QC-MS01: expected ; after friend class name", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                continue;
            }
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "friendly") {
                this->advance();
                info.friendlyClasses.push_back(parseTypeString());
                if (this->current_tok.type != TokenType::SEMICOLON) {
                    res.failure(new InvalidSyntaxError("QC-MS02: expected ; after friendly class name", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                continue;
            }
            std::vector<Token> modifiers;
            if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
                user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
                modifiers.push_back(this->current_tok);
                this->advance();
            }
            if (this->current_tok.type == TokenType::IDENTIFIER && this->current_tok.value == class_name.value) {

                if (is_abstract_class) {
                    res.failure(
                        new InvalidSyntaxError("QC-S090: Cannot make constructor on abstract class '" + class_name.value + "'", class_name.pos));
                    return res.to_prs();
                }

                Token next_tok;
                if (index + 1 < tokens.size()) {
                    next_tok = tokens[index + 1];
                } else {
                    next_tok = Token(TokenType::EOFT, "", this->current_tok.pos);
                }
                std::vector<GenericType> genericsM;
                Token ctor_name = this->current_tok;
                size_t oldPos = this->index;
                parseGenerics(genericsM, res);
                if (res.error) return res.to_prs();
                next_tok = peek();
                if (next_tok.type == TokenType::LPAREN) {
                    this->advance();
                    if (this->current_tok.type != TokenType::LPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S091: Expected '(' after constructor name", this->current_tok.pos));
                        return res.to_prs();
                    }

                    ParseResult res;
                    std::vector<GenericType> old_generics = this->current_generics;
                    this->current_generics.insert(this->current_generics.end(), genericsM.begin(), genericsM.end());
                    this->advance();
                    std::vector<Parameter> params;
                    if (this->current_tok.type != TokenType::RPAREN) {
                        while (true) {
                            params.push_back(this->parse_parameter());
                            if (params.back().type.type == TokenType::VARADIC) { break; }
                            if (this->current_tok.type == TokenType::COMMA) {
                                this->advance();
                            } else {
                                break;
                            }
                        }
                    }
                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S064: Expected ')' after parameters", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();
                    ClassMethodInfo mi;
                    if (this->current_tok.type == TokenType::COLON) {
                        this->advance();
                        Token tok = this->current_tok;
                        tok.value = this->parseTypeString();
                        AnyNode node = res.reg(this->call(new VarAccessNode(tok)));
                        if (res.error) return res.to_prs();
                        mi.parentConstructorCall = *std::get_if<CallNode*>(&node);
                    }
                    if (this->current_tok.type != TokenType::LBRACE) {
                        res.failure(new InvalidSyntaxError("QC-S003: Expected '{' to start constructor body", this->current_tok.pos));
                        return res.to_prs();
                    }
                    this->advance();
                    std::vector<AnyNode> body_stmts;
                    while (true) {
                        if (this->current_tok.type == TokenType::RBRACE) break;
                        if (this->current_tok.type == TokenType::EOFT) {
                            res.failure(new InvalidSyntaxError("QC-S092: Unexpected end of file in constructor body", this->current_tok.pos));
                            return res.to_prs();
                        }
                        Prs st = this->statement();
                        if (std::holds_alternative<Error*>(st)) {
                            res.failure(std::get<Error*>(st));
                            return res.to_prs();
                        }
                        if (!is_statement_node(st)) { continue; }
                        AnyNode node = to_any_node(st);
                        if (std::holds_alternative<std::monostate>(node)) { continue; }
                        body_stmts.push_back(node);
                    }
                    if (this->current_tok.type != TokenType::RBRACE) {
                        res.failure(new InvalidSyntaxError("QC-S093: Expected '}' to end constructor body", this->current_tok.pos));
                        return res.to_prs();
                    }
                    auto body = new StatementsNode(body_stmts, true);
                    this->advance();
                    this->current_generics = old_generics;
                    mi.name_tok = ctor_name;
                    mi.params.clear();
                    mi.params = params;
                    mi.return_types = {};
                    mi.body = body;
                    mi.is_constructor = true;
                    mi.access = access;
                    mi.modifiers = modifiers;
                    mi.is_volatile = is_volatile_method;
                    mi.generics = genericsM;
                    info.classMethods.push_back(mi);
                    continue;
                } else {
                    this->index = oldPos;
                    this->current_tok = peek(0);
                }
            }
            if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-T004: Expected type or constructor in class body", this->current_tok.pos));
                return res.to_prs();
            }

            std::vector<Token> type_list;

            auto parse_one_type_into = [&](Token& out_tok) -> bool {
                std::string field_type = parseTypeString();
                out_tok = Token(TokenType::KEYWORD, field_type, this->current_tok.pos);
                return true;
            };
            {
                Token t;
                if (!parse_one_type_into(t)) return res.to_prs();
                type_list.push_back(t);
            }
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                Token t;
                if (!parse_one_type_into(t)) return res.to_prs();
                type_list.push_back(t);
            }
            Token name_tok;
            if (this->current_tok.type == TokenType::IDENTIFIER && this->current_tok.value != "operator") {
                name_tok = this->current_tok;
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "operator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::MUL:
                case TokenType::DIV:
                case TokenType::EQ_TO:
                case TokenType::NOT_EQ:
                case TokenType::EQ:
                case TokenType::NOT:
                case TokenType::AND:
                case TokenType::OR:
                case TokenType::MORE:
                case TokenType::LESS:
                case TokenType::MORE_EQ:
                case TokenType::LESS_EQ:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::XOR:
                case TokenType::QNOT:
                case TokenType::QAND:
                case TokenType::QOR:
                case TokenType::QXOR:
                case TokenType::INCREMENT:
                case TokenType::DECREMENT:
                case TokenType::BITWISE_NOT:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT:
                case TokenType::BITWISE_XOR:
                case TokenType::PIPE:
                case TokenType::AMPERSAND:
                case TokenType::COLLAPSE_OR:
                case TokenType::PLUS_EQ:
                case TokenType::MINUS_EQ:
                case TokenType::MUL_EQ:
                case TokenType::DIV_EQ:
                case TokenType::MOD_EQ:
                case TokenType::BIT_X_EQ:
                case TokenType::BIT_A_EQ:
                case TokenType::BIT_O_EQ:
                case TokenType::LSH_EQ:
                case TokenType::RSH_EQ:
                case TokenType::LRSH_EQ:
                case TokenType::RROT_EQ:
                case TokenType::LROT_EQ:
                case TokenType::COLLAPSE_AND: break;
                case TokenType::LPAREN:
                    this->advance();
                    if (this->current_tok.type != TokenType::RPAREN) {
                        res.failure(new InvalidSyntaxError("QC-S094: expected closing paren in operator()", op_tok.pos));
                        return res.to_prs();
                    }
                    break;
                case TokenType::LBRACKET:
                    this->advance();
                    if (this->current_tok.type == TokenType::RBRACKET) {
                        long_ops[0] = this->current_tok;
                        if (this->peek().type == TokenType::EQ) {
                            this->advance();
                            long_ops[1] = this->current_tok;
                            break;
                        }
                        long_ops[1] = Token(TokenType::EOFT, "N/A", op_tok.pos);
                        break;
                    } else {
                        res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos));
                        return res.to_prs();
                    }
                default: res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos)); return res.to_prs();
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::PLUS: op_name = "operator+"; break;
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::MUL: op_name = "operator*"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::EQ_TO: op_name = "operator=="; break;
                case TokenType::NOT_EQ: op_name = "operator!="; break;
                case TokenType::EQ: op_name = "operator="; break;
                case TokenType::NOT: op_name = "operator!"; break;
                case TokenType::AND: op_name = "operator&&"; break;
                case TokenType::OR: op_name = "operator||"; break;
                case TokenType::MORE: op_name = "operator>"; break;
                case TokenType::LESS: op_name = "operator<"; break;
                case TokenType::PLUS_EQ: op_name = "operator+="; break;
                case TokenType::MINUS_EQ: op_name = "operator-="; break;
                case TokenType::MUL_EQ: op_name = "operator*="; break;
                case TokenType::DIV_EQ: op_name = "operator/="; break;
                case TokenType::MOD_EQ: op_name = "operator%="; break;
                case TokenType::BIT_X_EQ: op_name = "operator$="; break;
                case TokenType::BIT_A_EQ: op_name = "operator&="; break;
                case TokenType::BIT_O_EQ: op_name = "operator|="; break;
                case TokenType::LSH_EQ: op_name = "operator<<="; break;
                case TokenType::RSH_EQ: op_name = "operator|>="; break;
                case TokenType::LRSH_EQ: op_name = "operator:>="; break;
                case TokenType::RROT_EQ: op_name = "operator|>>="; break;
                case TokenType::LROT_EQ: op_name = "operator<<<="; break;
                case TokenType::MORE_EQ: op_name = "operator>="; break;
                case TokenType::LESS_EQ: op_name = "operator<="; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::XOR: op_name = "operator^"; break;
                case TokenType::QNOT: op_name = "operator!!"; break;
                case TokenType::QAND: op_name = "operator&&&"; break;
                case TokenType::QOR: op_name = "operator|||"; break;
                case TokenType::QXOR: op_name = "operator^^"; break;
                case TokenType::COLLAPSE_OR: op_name = "operator|&|"; break;
                case TokenType::COLLAPSE_AND: op_name = "operator&|&"; break;
                case TokenType::LBRACKET: op_name = ((long_ops[1].type == TokenType::EQ) ? "operator[]=" : "operator[]"); break;
                case TokenType::LPAREN: op_name = "operator()"; break;
                case TokenType::INCREMENT: op_name = "operator++"; break;
                case TokenType::DECREMENT: op_name = "operator--"; break;
                case TokenType::BITWISE_NOT: op_name = "operator~"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                case TokenType::BITWISE_XOR: op_name = "operator$"; break;
                case TokenType::PIPE: op_name = "operator|"; break;
                case TokenType::AMPERSAND: op_name = "operator&"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "roperator") {
                this->advance();
                Token op_tok = this->current_tok;
                Token long_ops[2] = {};
                switch (op_tok.type) {
                case TokenType::MINUS:
                case TokenType::DIV:
                case TokenType::POWER:
                case TokenType::MOD:
                case TokenType::RSHIFT:
                case TokenType::LOGICAL_RSHIFT:
                case TokenType::R_ROT:
                case TokenType::LSHIFT:
                case TokenType::L_ROT: break;
                default: res.failure(new InvalidSyntaxError("QC-S096: Unsupported operator in roperator method", op_tok.pos)); return res.to_prs();
                }
                std::string op_name;
                switch (op_tok.type) {
                case TokenType::MINUS: op_name = "operator-"; break;
                case TokenType::DIV: op_name = "operator/"; break;
                case TokenType::POWER: op_name = "operator#^"; break;
                case TokenType::MOD: op_name = "operator%"; break;
                case TokenType::RSHIFT: op_name = "operator|>"; break;
                case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                case TokenType::R_ROT: op_name = "operator|>>"; break;
                case TokenType::LSHIFT: op_name = "operator<<"; break;
                case TokenType::L_ROT: op_name = "operator<<<"; break;
                default: break;
                }
                name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                this->advance();
            } else {
                res.failure(new InvalidSyntaxError("QC-T005: Expected method or field name after type(s)", this->current_tok.pos));
                return res.to_prs();
            }
            std::vector<GenericType> genericsM;
            parseGenerics(genericsM, res);
            if (res.error) return res.to_prs();
            if (this->current_tok.type == TokenType::LPAREN) {
                ClassMethodInfo mi;
                mi.name_tok = name_tok;
                if (!info.baseClassName.empty()) {
                    auto* base_ptr = find_type(base_type_name(baseName));
                    if (base_ptr && base_ptr->kind == UserTypeKind::Class && base_ptr->is_final_class) {
                        auto& baseInfo = *base_ptr;
                        for (auto& bm : baseInfo.classMethods) {
                            if (bm.name_tok.value == mi.name_tok.value && bm.is_final) {
                                res.failure(new InvalidSyntaxError("QC-S097: Cannot override final method '" + mi.name_tok.value +
                                                                       "' from base class '" + info.baseClassName + "'",
                                                                   mi.name_tok.pos));
                                return res.to_prs();
                            }
                        }
                    }
                }

                auto m_pr = this->func_def_multi(type_list, std::make_optional(name_tok), genericsM, true, false, modifiers);
                if (std::holds_alternative<Error*>(m_pr)) return m_pr;

                auto fn = std::get<FuncDefNode*>(m_pr);

                mi.params.clear();
                mi.params.reserve(fn->params.size());
                for (auto it = fn->params.begin(); it != fn->params.end(); ++it) { mi.params.push_back(*it); }
                mi.return_types = fn->return_types;
                mi.body = fn->body;
                mi.is_constructor = false;
                mi.is_final = is_final_method;
                mi.access = access;
                mi.is_volatile = is_volatile_method;
                mi.generics = genericsM;
                mi.is_static = is_static;
                info.classMethods.push_back(mi);
                continue;
            }
            if (type_list.size() != 1) {
                res.failure(new InvalidSyntaxError("QC-T006: Class fields cannot have multiple types", name_tok.pos));
                return res.to_prs();
            }
            std::string field_type = type_list[0].value;
            int array_dims = 0;
            while (this->current_tok.type == TokenType::LBRACKET) {
                this->advance();
                field_type += "[";
                if (this->current_tok.type != TokenType::RBRACKET) {
                    field_type += this->current_tok.value;
                    this->advance();
                }
                if (this->current_tok.type != TokenType::RBRACKET) {
                    res.failure(new InvalidSyntaxError("QC-S061: Expected ']' after '[' in array", this->current_tok.pos));
                    return res.to_prs();
                }
                field_type += "]";
                this->advance();
            }
            AnyNode default_value = std::monostate{};
            if (this->current_tok.type == TokenType::EQ) {
                this->advance();
                default_value = res.reg(this->text_unops());
                if (res.error) return res.to_prs();
            }
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new InvalidSyntaxError("QC-MS03: Expected ';' after field declaration", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();

            ClassField cf;
            cf.name = name_tok.value;
            cf.type = field_type;
            cf.access = access;
            cf.isStatic = is_static;
            cf.defaultValue = default_value;
            if (std::ranges::any_of(info.classFields, [&](const ClassField& thing) { return thing.name == name_tok.value; })) {
                res.failure(new InvalidSyntaxError("QC-CS01: Duplicate field in class", this->current_tok.pos));
                return res.to_prs();
            }
            info.classFields.push_back(cf);
        }

        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S098: Expected '}' at end of class", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        full_key = currentNamespace.empty() ? class_name.value : currentNamespace + "::" + class_name.value;
        user_types[base_type_name(full_key)] = info;
        this->current_generics = saved_generics;
        return res.success(std::monostate{});
    }

    if (tok.type == TokenType::KEYWORD && tok.value == "namespace") {
        ParseResult res;
        this->advance();

        if (current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-N001: Expected namespace name", current_tok.pos));
            return res.to_prs();
        }

        Token nsName = current_tok;
        this->advance();

        if (current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-N004: Expected '{' after namespace name", current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        namespaceStack.push_back(nsName.value);

        if (currentNamespace.empty()) {
            currentNamespace = nsName.value;
        } else {
            currentNamespace += "::" + nsName.value;
        }

        std::vector<AnyNode> body;

        while (current_tok.type != TokenType::RBRACE && current_tok.type != TokenType::EOFT) {

            Prs st = statement();
            if (std::holds_alternative<Error*>(st)) {
                namespaceStack.pop_back();
                if (namespaceStack.empty()) {
                    currentNamespace = "";
                } else {
                    currentNamespace = "";
                    for (size_t i = 0; i < namespaceStack.size(); ++i) {
                        if (i > 0) currentNamespace += "::";
                        currentNamespace += namespaceStack[i];
                    }
                }
                return res.to_prs();
            }

            body.push_back(prs_to_anynode(st));
        }

        if (current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-N005: Expected '}' at end of namespace", current_tok.pos));
            return res.to_prs();
        }

        this->advance();

        namespaceStack.pop_back();

        if (namespaceStack.empty()) {
            currentNamespace = "";
        } else {
            currentNamespace = "";
            for (size_t i = 0; i < namespaceStack.size(); ++i) {
                if (i > 0) currentNamespace += "::";
                currentNamespace += namespaceStack[i];
            }
        }

        return res.success(new NamespaceNode(nsName.value, body, nsName.pos));
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "struct") {
        this->advance();

        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S065: Expected struct name", this->current_tok.pos));
            return res.to_prs();
        }
        Token struct_name = this->current_tok;
        this->advance();
        std::vector<GenericType> generics;
        parseGenerics(generics, res);
        if (res.error) return res.to_prs();
        auto saved_generics = this->current_generics;
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            UserTypeInfo tempInfo;
            tempInfo.kind = UserTypeKind::Struct;
            tempInfo.pos = struct_name.pos;
            tempInfo.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
            std::string full_key = currentNamespace.empty() ? struct_name.value : currentNamespace + "::" + struct_name.value;
            user_types[full_key] = tempInfo;
            return res.success(std::monostate{});
        }
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after struct name", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        std::vector<StructField> fields;

        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {

            std::string field_type;
            bool is_user_type = false;
            if (this->current_tok.type == TokenType::IDENTIFIER) {
                field_type = parseTypeString();
            } else if (this->current_tok.type == TokenType::KEYWORD) {
                field_type = parseTypeString();
            } else {
                res.failure(new InvalidSyntaxError("QC-S071: Expected field type in struct", this->current_tok.pos));
                return res.to_prs();
            }

            if (this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-S072: Expected field name in struct", this->current_tok.pos));
                return res.to_prs();
            }
            Token field_name = this->current_tok;
            this->advance();

            int array_dims = 0;
            while (this->current_tok.type == TokenType::LBRACKET) {
                this->advance();
                if (this->current_tok.type == TokenType::INT) { this->advance(); }
                if (this->current_tok.type != TokenType::RBRACKET) {
                    res.failure(new InvalidSyntaxError("QC-S061: Expected ']' after '[' in array", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                array_dims++;
            }

            for (int i = 0; i < array_dims; ++i) field_type += "[]";

            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new MissingSemicolonError(this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            if (std::ranges::any_of(fields, [&](const StructField& thing) { return thing.name == field_name.value; })) {
                res.failure(new InvalidSyntaxError("QC-ST01: Duplicate field in struct", this->current_tok.pos));
                return res.to_prs();
            }
            fields.push_back({field_name.value, field_type});
        }
        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S073: Expected '}' at end of struct", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        std::string full_key = currentNamespace.empty() ? struct_name.value : currentNamespace + "::" + struct_name.value;
        if (this->current_tok.type == TokenType::SEMICOLON) { this->advance(); }
        UserTypeInfo info;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        info.kind = UserTypeKind::Struct;
        info.fields = fields;
        info.pos = struct_name.pos;
        info.generics = generics;
        info.namespace_path = currentNamespace;
        user_types[base_type_name(full_key)] = info;
        this->current_generics = std::move(saved_generics);
        return res.success(std::monostate{});
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "type") {
        this->advance();

        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S074: Expected type name after 'type'", this->current_tok.pos));
            return res.to_prs();
        }

        Token type_name = this->current_tok;
        this->advance();
        std::vector<GenericType> generics;
        parseGenerics(generics, res);
        if (res.error) return res.to_prs();
        auto saved_generics = this->current_generics;
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
        if (this->current_tok.type != TokenType::EQ) {
            res.failure(new InvalidSyntaxError("QC-S075: Expected '=' after type name '" + type_name.value + "'", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        auto parse_type_atom = [&](Token first_tok) -> UnionMember {
            std::string type_str;
            switch (first_tok.type) {
            case TokenType::STRING: this->advance(); return UnionMember{"string:\"" + first_tok.value + "\""};
            case TokenType::INT: this->advance(); return UnionMember{"int:" + first_tok.value};
            case TokenType::FLOAT: this->advance(); return UnionMember{"float:" + first_tok.value};
            case TokenType::DOUBLE: this->advance(); return UnionMember{"double:" + first_tok.value};
            case TokenType::LONG_INT: this->advance(); return UnionMember{"long_int:" + first_tok.value};
            case TokenType::SHORT_INT: this->advance(); return UnionMember{"short_int:" + first_tok.value};
            case TokenType::LONG_DOUBLE: this->advance(); return UnionMember{"long_double:" + first_tok.value};
            case TokenType::ADDR_T: this->advance(); return UnionMember{"addr_t:" + first_tok.value};
            case TokenType::CHAR: this->advance(); return UnionMember{"char:" + first_tok.value};
            case TokenType::BYTE: this->advance(); return UnionMember{"byte:" + first_tok.value};
            case TokenType::NIBBLE: this->advance(); return UnionMember{"nibble:" + first_tok.value};
            case TokenType::BOOL: this->advance(); return UnionMember{"bool:" + first_tok.value};
            case TokenType::QBOOL: this->advance(); return UnionMember{"qbool:" + first_tok.value};
            default: break;
            }
            type_str = parseTypeString();
            int array_dims = 0;
            while (this->current_tok.type == TokenType::LBRACKET) {
                this->advance();
                if (this->current_tok.type == TokenType::INT) { this->advance(); }
                if (this->current_tok.type != TokenType::RBRACKET) {
                    res.failure(new InvalidSyntaxError("QC-S061: Expected ']' after '[' in array", this->current_tok.pos));
                    return UnionMember{""};
                }
                this->advance();
                array_dims++;
            }
            for (int i = 0; i < array_dims; ++i) { type_str += "[]"; }

            return UnionMember{type_str};
        };

        auto is_type_or_literal_token = [&](TokenType tt) {
            return tt == TokenType::STRING || tt == TokenType::IDENTIFIER || tt == TokenType::KEYWORD || tt == TokenType::INT ||
                   tt == TokenType::NIBBLE || tt == TokenType::FLOAT || tt == TokenType::DOUBLE || tt == TokenType::ADDR_T || tt == TokenType::BOOL ||
                   tt == TokenType::QBOOL || tt == TokenType::CHAR || tt == TokenType::LONG_INT || tt == TokenType::SHORT_INT ||
                   tt == TokenType::LONG_DOUBLE || tt == TokenType::BYTE;
        };

        if (!is_type_or_literal_token(this->current_tok.type)) {
            res.failure(new InvalidSyntaxError("QC-S076: Expected type or literal in type alias", this->current_tok.pos));
            return res.to_prs();
        }

        std::vector<UnionMember> members;
        {
            Token first_tok = this->current_tok;
            members.push_back(parse_type_atom(first_tok));
        }
        while (this->current_tok.type == TokenType::PIPE) {
            this->advance();

            if (!is_type_or_literal_token(this->current_tok.type)) {
                res.failure(new InvalidSyntaxError("QC-S077: Expected type or literal after '|' in type alias", this->current_tok.pos));
                return res.to_prs();
            }

            Token t = this->current_tok;
            members.push_back(parse_type_atom(t));
        }
        if (this->current_tok.type != TokenType::SEMICOLON) {
            res.failure(new MissingSemicolonError(this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        std::string full_key = currentNamespace.empty() ? type_name.value : currentNamespace + "::" + type_name.value;
        UserTypeInfo info;
        if (members.size() == 1) {
            info.kind = UserTypeKind::Alias;
            info.aliasTarget = members[0].type;
        } else {
            info.kind = UserTypeKind::Union;
            info.members = members;
        }
        info.pos = type_name.pos;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        info.generics = generics;
        info.namespace_path = currentNamespace;
        user_types[base_type_name(full_key)] = info;
        this->current_generics = std::move(saved_generics);
        return res.success(std::monostate{});
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "enum") {
        this->advance();
        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S080: Expected enum name", this->current_tok.pos));
            return res.to_prs();
        }
        Token enum_name = this->current_tok;
        std::string enum_type = "int";
        this->advance();
        std::vector<GenericType> generics;
        parseGenerics(generics, res);
        if (res.error) return res.to_prs();
        auto saved_generics = this->current_generics;
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
        if (this->current_tok.type == TokenType::COLON) {
            this->advance();
            if (!is_primint_type(this->current_tok)) {
                res.failure(new InvalidSyntaxError("QC-S277: Expected a numeric new type to follow ':' in enum declaration", this->current_tok.pos));
                return res.to_prs();
            }
            enum_type = this->current_tok.value;
        }
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after enum name", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        std::vector<UnionMember> members;
        std::vector<EnumEntry> entries;
        uint64_t counter = 0;
        bool is_signed_type = enum_type.contains("int");
        bool can_auto = true;
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-S081: Expected enum member name", this->current_tok.pos));
                return res.to_prs();
            }
            Token member_name = this->current_tok;
            std::string value = "";
            this->advance();
            std::vector<std::string> tags;
            if (this->current_tok.type == TokenType::LPAREN) {
                while (this->current_tok.type != TokenType::RPAREN && this->current_tok.type != TokenType::EOFT) {
                    this->advance();
                    tags.push_back(this->parseTypeString());
                }
                if (this->current_tok.type == TokenType::EOFT) {
                    res.failure(new InvalidSyntaxError("QC-EM02: Unexpected EOF", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
            }
            if (this->current_tok.type != TokenType::EQ) {
                if (!can_auto) {
                    res.failure(new InvalidSyntaxError("QC-EM01: Enums may not use implicit value increment after explicit value increment",
                                                       this->current_tok.pos));
                    return res.to_prs();
                }
                if (is_signed_type) {
                    value = std::to_string(static_cast<int64_t>(counter));
                } else {
                    value = std::to_string(counter);
                }
                counter++;
            } else {
                this->advance();
                value = this->current_tok.value;
                can_auto = false;
                this->advance();
            }
            entries.push_back(EnumEntry{member_name.value, value, tags});
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new InvalidSyntaxError("QC-S083: Expected ';' after enum member", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
        }
        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S084: Expected '}' at end of enum", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        if (this->current_tok.type == TokenType::SEMICOLON) this->advance();
        std::string full_key = currentNamespace.empty() ? enum_name.value : currentNamespace + "::" + enum_name.value;
        UserTypeInfo info;
        info.pos = enum_name.pos;
        info.enumType = enum_type;
        info.kind = UserTypeKind::Enum;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        info.enumEntries = entries;
        info.namespace_path = currentNamespace;
        info.generics = generics;
        user_types[base_type_name(full_key)] = info;
        this->current_generics = std::move(saved_generics);
        return res.success(std::monostate{});
    }
    if (tok.type == TokenType::KEYWORD && tok.value == "concept") {
        this->advance();

        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S065: Expected concept name", this->current_tok.pos));
            return res.to_prs();
        }
        Token name = this->current_tok;
        this->advance();
        std::vector<GenericType> generics;
        parseGenerics(generics, res);
        if (res.error) return res.to_prs();
        auto saved_generics = this->current_generics;
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            UserTypeInfo tempInfo;
            tempInfo.generics = generics;
            tempInfo.kind = UserTypeKind::Concept;
            tempInfo.pos = name.pos;
            tempInfo.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
            std::string full_key = currentNamespace.empty() ? name.value : currentNamespace + "::" + name.value;
            user_types[full_key] = tempInfo;
            this->current_generics = saved_generics;
            return res.success(std::monostate{});
        }
        this->current_generics.insert(this->current_generics.end(), generics.begin(), generics.end());
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after concept name", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        int defNumber = 0;
        std::vector<ConceptInfo::Block> blocks;
        std::vector<std::pair<int, ConceptInfo::DefaultBlock>> defaultBlocks;
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            fn(blocks, res, blocks, defaultBlocks);
            if (res.error) return res.to_prs();
        }
        if (this->current_tok.type != TokenType::RBRACE) {
            res.failure(new InvalidSyntaxError("QC-S073: Expected '}' at end of concept", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        std::string full_key = currentNamespace.empty() ? name.value : currentNamespace + "::" + name.value;
        if (this->current_tok.type == TokenType::SEMICOLON) { this->advance(); }
        UserTypeInfo info;
        info.pos = name.pos;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        info.kind = UserTypeKind::Concept;
        info.generics = generics;
        info.namespace_path = currentNamespace;
        ConceptInfo ci;
        for (auto& b : blocks) { ci.blocks.push_back({b, std::nullopt}); }
        for (auto& [blockIdx, defBlock] : defaultBlocks) {
            if (blockIdx >= 0 && blockIdx < (int)ci.blocks.size()) {
                ci.blocks[blockIdx].second = defBlock;
            } else {
                ConceptInfo::Block emptyBlock;
                ci.blocks.push_back({emptyBlock, defBlock});
            }
        }
        info.conceptInfo = ci;
        user_types[base_type_name(full_key)] = info;
        this->current_generics = std::move(saved_generics);
        return res.success(std::monostate{});
    }
    if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "modifier") {
        ModifierInfo modifierInfo;
        this->advance();
        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S080: Expected modifier name", this->current_tok.pos));
            return res.to_prs();
        }
        Token modifier_name = this->current_tok;
        this->advance();
        if (this->current_tok.type == TokenType::SEMICOLON) {
            this->advance();
            UserTypeInfo info;
            info.kind = UserTypeKind::Modifier;
            std::string full_key = currentNamespace.empty() ? modifier_name.value : currentNamespace + "::" + modifier_name.value;
            info.pos = modifier_name.pos;
            info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
            info.namespace_path = currentNamespace;
            user_types[base_type_name(full_key)] = info;
            return res.success(std::monostate{});
        }
        if (this->current_tok.type != TokenType::LBRACE) {
            res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after modifier name", this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();
        while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
            if (this->current_tok.type != TokenType::IDENTIFIER && this->current_tok.type != TokenType::KEYWORD) {
                res.failure(new InvalidSyntaxError("QC-S099: Expected modifier handler ('on_call', 'on_return', 'on_use')", this->current_tok.pos));
                return res.to_prs();
            }
            Token name = this->current_tok;
            this->advance();
            StatementsNode* block = nullptr;
            if (!parse_block_into(block, res)) return res.to_prs();
            modifierInfo.handlers.push_back(std::make_pair(name, block));
        }
        this->advance();
        std::string full_key = currentNamespace.empty() ? modifier_name.value : currentNamespace + "::" + modifier_name.value;
        UserTypeInfo info;
        info.pos = modifier_name.pos;
        info.baseFile = SourceManager::instance().get(this->current_tok.pos.file_id).filename;
        info.namespace_path = currentNamespace;
        info.kind = UserTypeKind::Modifier;
        info.modifierInfo = modifierInfo;
        user_types[base_type_name(full_key)] = info;
        return res.success(std::monostate{});
    }
    if (this->current_tok.type == TokenType::LBRACE) {
        StatementsNode* block;
        if (!parse_block_into(block, res)) return res.to_prs();
        block->is_scoped = true;
        return res.success(block);
    }
    std::vector<Token> modifiers;
    if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
        user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
        modifiers.push_back(this->current_tok);
        this->advance();
        tok = this->current_tok;
    }
    if (tok.type == TokenType::KEYWORD || tok.type == TokenType::IDENTIFIER && tok.value != "this") {
        if (tok.type == TokenType::IDENTIFIER) {
            Token saved_tok = this->current_tok;
            size_t saved_index = this->index;
            auto maybe_qualified = this->try_parse_qualified_name();
            bool is_type = false;
            if (maybe_qualified.has_value()) {
                std::string base = *maybe_qualified;
                size_t lc = base.rfind("::");
                if (lc != std::string::npos) base = base.substr(lc + 2);
                is_type = (find_type(base) != nullptr || find_type(*maybe_qualified) != nullptr || is_known_type(base) ||
                           is_known_type(*maybe_qualified));
            }
            if (!is_type) {
                if (!maybe_qualified.has_value()) this->parseTypeString();
                while (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                    this->parseTypeString();
                }
                bool looks_like_generic_func = (this->current_tok.type == TokenType::IDENTIFIER && peek(1).type == TokenType::LESS);
                if (!looks_like_generic_func) {
                    this->index = saved_index;
                    this->current_tok = saved_tok;
                    size_t next_i = index + 1;
                    if (next_i < tokens.size() && (tokens[next_i].type == TokenType::EQ || tokens[next_i].type == TokenType::PLUS_EQ ||
                                                   tokens[next_i].type == TokenType::MINUS_EQ || tokens[next_i].type == TokenType::MUL_EQ ||
                                                   tokens[next_i].type == TokenType::DIV_EQ || tokens[next_i].type == TokenType::MOD_EQ ||
                                                   tokens[next_i].type == TokenType::RSH_EQ || tokens[next_i].type == TokenType::LSH_EQ ||
                                                   tokens[next_i].type == TokenType::LRSH_EQ || tokens[next_i].type == TokenType::RROT_EQ ||
                                                   tokens[next_i].type == TokenType::LROT_EQ || tokens[next_i].type == TokenType::BIT_X_EQ ||
                                                   tokens[next_i].type == TokenType::BIT_O_EQ || tokens[next_i].type == TokenType::BIT_A_EQ)) {
                        AnyNode assign_node = res.reg(this->assignment_expr());
                        if (res.error) return res.to_prs();

                        if (this->current_tok.type != TokenType::SEMICOLON) {
                            res.failure(new MissingSemicolonError(this->current_tok.pos));
                            return res.to_prs();
                        }

                        this->advance();
                        return res.success(assign_node);
                    }
                    AnyNode node = res.reg(this->assignment_expr());
                    if (res.error) return res.to_prs();
                    if (this->current_tok.type == TokenType::SEMICOLON) {
                        this->advance();
                        return res.success(node);
                    }
                    res.failure(new MissingSemicolonError(this->current_tok.pos));
                    return res.to_prs();
                }
            }
            this->index = saved_index;
            this->current_tok = saved_tok;
        }
        bool is_const = false;
        if (tok.value == "const") {
            is_const = true;
            this->advance();
        }
        bool is_volatile = false;
        if (this->current_tok.value == "volatile") {
            is_volatile = true;
            this->advance();
        }
        if (this->current_tok.value == "atomic") { this->advance(); }
        if (this->current_tok.value == "volatile") {
            is_volatile = true;
            this->advance();
        }
        if (this->current_tok.value == "const") {
            is_const = true;
            this->advance();
        }
        Position type_pos = this->current_tok.pos;
        std::string type_str = parseTypeString();
        if (type_str.empty()) return res.to_prs();
        Token type_tok(TokenType::KEYWORD, type_str, type_pos);
        if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "proves") {
            Position& proved_pos = this->current_tok.pos;
            this->advance();
            std::string conceptName = parseTypeString();
            if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "with_proof") {
                ConceptProvee proof;
                proof.namespacePath = namespaceStack;
                this->advance();
                if (this->current_tok.type != TokenType::LBRACE) {
                    res.failure(new InvalidSyntaxError("QC-S003: Expected '{' after with_proof", this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                while (this->current_tok.type != TokenType::RBRACE && this->current_tok.type != TokenType::EOFT) {
                    std::vector<Token> modifiers;
                    if (this->current_tok.type == TokenType::IDENTIFIER && user_types.find(this->current_tok.value) != user_types.end() &&
                        user_types[this->current_tok.value].kind == UserTypeKind::Modifier) {
                        modifiers.push_back(this->current_tok);
                        this->advance();
                    }
                    if (this->current_tok.type != TokenType::KEYWORD && this->current_tok.type != TokenType::IDENTIFIER) {
                        res.failure(new InvalidSyntaxError("QC-T009: Expected method return type(s)", this->current_tok.pos));
                        return res.to_prs();
                    }
                    std::vector<Token> type_list;
                    auto parse_one_type_into = [&](Token& out_tok) -> bool {
                        std::string field_type = parseTypeString();
                        out_tok = Token(TokenType::KEYWORD, field_type, this->current_tok.pos);
                        return true;
                    };
                    {
                        Token t;
                        if (!parse_one_type_into(t)) return res.to_prs();
                        type_list.push_back(t);
                    }
                    while (this->current_tok.type == TokenType::COMMA) {
                        this->advance();
                        Token t;
                        if (!parse_one_type_into(t)) return res.to_prs();
                        type_list.push_back(t);
                    }
                    Token name_tok;
                    if (this->current_tok.type == TokenType::IDENTIFIER && this->current_tok.value != "operator") {
                        name_tok = this->current_tok;
                        this->advance();
                    } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "operator") {
                        this->advance();
                        Token op_tok = this->current_tok;
                        Token long_ops[2] = {};
                        switch (op_tok.type) {
                        case TokenType::PLUS:
                        case TokenType::MINUS:
                        case TokenType::MUL:
                        case TokenType::DIV:
                        case TokenType::EQ_TO:
                        case TokenType::NOT_EQ:
                        case TokenType::EQ:
                        case TokenType::NOT:
                        case TokenType::AND:
                        case TokenType::OR:
                        case TokenType::MORE:
                        case TokenType::LESS:
                        case TokenType::MORE_EQ:
                        case TokenType::LESS_EQ:
                        case TokenType::POWER:
                        case TokenType::MOD:
                        case TokenType::XOR:
                        case TokenType::QNOT:
                        case TokenType::QAND:
                        case TokenType::QOR:
                        case TokenType::QXOR:
                        case TokenType::INCREMENT:
                        case TokenType::DECREMENT:
                        case TokenType::BITWISE_NOT:
                        case TokenType::RSHIFT:
                        case TokenType::LOGICAL_RSHIFT:
                        case TokenType::R_ROT:
                        case TokenType::LSHIFT:
                        case TokenType::L_ROT:
                        case TokenType::BITWISE_XOR:
                        case TokenType::PIPE:
                        case TokenType::AMPERSAND:
                        case TokenType::COLLAPSE_OR:
                        case TokenType::PLUS_EQ:
                        case TokenType::MINUS_EQ:
                        case TokenType::MUL_EQ:
                        case TokenType::DIV_EQ:
                        case TokenType::MOD_EQ:
                        case TokenType::BIT_X_EQ:
                        case TokenType::BIT_A_EQ:
                        case TokenType::BIT_O_EQ:
                        case TokenType::LSH_EQ:
                        case TokenType::RSH_EQ:
                        case TokenType::LRSH_EQ:
                        case TokenType::RROT_EQ:
                        case TokenType::LROT_EQ:
                        case TokenType::COLLAPSE_AND: break;
                        case TokenType::LPAREN:
                            this->advance();
                            if (this->current_tok.type != TokenType::RPAREN) {
                                res.failure(new InvalidSyntaxError("QC-S094: expected closing paren in operator()", op_tok.pos));
                                return res.to_prs();
                            }
                            break;
                        case TokenType::LBRACKET:
                            this->advance();
                            if (this->current_tok.type == TokenType::RBRACKET) {
                                long_ops[0] = this->current_tok;
                                if (this->peek().type == TokenType::EQ) {
                                    this->advance();
                                    long_ops[1] = this->current_tok;
                                    break;
                                }
                                long_ops[1] = Token(TokenType::EOFT, "N/A", op_tok.pos);
                                break;
                            } else {
                                res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos));
                                return res.to_prs();
                            }
                        default:
                            res.failure(new InvalidSyntaxError("QC-S095: Unsupported operator in operator method", op_tok.pos));
                            return res.to_prs();
                        }
                        std::string op_name;
                        switch (op_tok.type) {
                        case TokenType::PLUS: op_name = "operator+"; break;
                        case TokenType::MINUS: op_name = "operator-"; break;
                        case TokenType::MUL: op_name = "operator*"; break;
                        case TokenType::DIV: op_name = "operator/"; break;
                        case TokenType::EQ_TO: op_name = "operator=="; break;
                        case TokenType::NOT_EQ: op_name = "operator!="; break;
                        case TokenType::EQ: op_name = "operator="; break;
                        case TokenType::NOT: op_name = "operator!"; break;
                        case TokenType::AND: op_name = "operator&&"; break;
                        case TokenType::OR: op_name = "operator||"; break;
                        case TokenType::MORE: op_name = "operator>"; break;
                        case TokenType::LESS: op_name = "operator<"; break;
                        case TokenType::PLUS_EQ: op_name = "operator+="; break;
                        case TokenType::MINUS_EQ: op_name = "operator-="; break;
                        case TokenType::MUL_EQ: op_name = "operator*="; break;
                        case TokenType::DIV_EQ: op_name = "operator/="; break;
                        case TokenType::MOD_EQ: op_name = "operator%="; break;
                        case TokenType::BIT_X_EQ: op_name = "operator$="; break;
                        case TokenType::BIT_A_EQ: op_name = "operator&="; break;
                        case TokenType::BIT_O_EQ: op_name = "operator|="; break;
                        case TokenType::LSH_EQ: op_name = "operator<<="; break;
                        case TokenType::RSH_EQ: op_name = "operator|>="; break;
                        case TokenType::LRSH_EQ: op_name = "operator:>="; break;
                        case TokenType::RROT_EQ: op_name = "operator|>>="; break;
                        case TokenType::LROT_EQ: op_name = "operator<<<="; break;
                        case TokenType::MORE_EQ: op_name = "operator>="; break;
                        case TokenType::LESS_EQ: op_name = "operator<="; break;
                        case TokenType::POWER: op_name = "operator#^"; break;
                        case TokenType::MOD: op_name = "operator%"; break;
                        case TokenType::XOR: op_name = "operator^"; break;
                        case TokenType::QNOT: op_name = "operator!!"; break;
                        case TokenType::QAND: op_name = "operator&&&"; break;
                        case TokenType::QOR: op_name = "operator|||"; break;
                        case TokenType::QXOR: op_name = "operator^^"; break;
                        case TokenType::COLLAPSE_OR: op_name = "operator|&|"; break;
                        case TokenType::COLLAPSE_AND: op_name = "operator&|&"; break;
                        case TokenType::LBRACKET: op_name = ((long_ops[1].type == TokenType::EQ) ? "operator[]=" : "operator[]"); break;
                        case TokenType::LPAREN: op_name = "operator()"; break;
                        case TokenType::INCREMENT: op_name = "operator++"; break;
                        case TokenType::DECREMENT: op_name = "operator--"; break;
                        case TokenType::BITWISE_NOT: op_name = "operator~"; break;
                        case TokenType::RSHIFT: op_name = "operator|>"; break;
                        case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                        case TokenType::R_ROT: op_name = "operator|>>"; break;
                        case TokenType::LSHIFT: op_name = "operator<<"; break;
                        case TokenType::L_ROT: op_name = "operator<<<"; break;
                        case TokenType::BITWISE_XOR: op_name = "operator$"; break;
                        case TokenType::PIPE: op_name = "operator|"; break;
                        case TokenType::AMPERSAND: op_name = "operator&"; break;
                        default: break;
                        }
                        name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                        this->advance();
                    } else if (this->current_tok.type == TokenType::KEYWORD && this->current_tok.value == "roperator") {
                        this->advance();
                        Token op_tok = this->current_tok;
                        Token long_ops[2] = {};
                        switch (op_tok.type) {
                        case TokenType::MINUS:
                        case TokenType::DIV:
                        case TokenType::POWER:
                        case TokenType::MOD:
                        case TokenType::RSHIFT:
                        case TokenType::LOGICAL_RSHIFT:
                        case TokenType::R_ROT:
                        case TokenType::LSHIFT:
                        case TokenType::L_ROT: break;
                        default:
                            res.failure(new InvalidSyntaxError("QC-S096: Unsupported operator in roperator method", op_tok.pos));
                            return res.to_prs();
                        }
                        std::string op_name;
                        switch (op_tok.type) {
                        case TokenType::MINUS: op_name = "operator-"; break;
                        case TokenType::DIV: op_name = "operator/"; break;
                        case TokenType::POWER: op_name = "operator#^"; break;
                        case TokenType::MOD: op_name = "operator%"; break;
                        case TokenType::RSHIFT: op_name = "operator|>"; break;
                        case TokenType::LOGICAL_RSHIFT: op_name = "operator:>"; break;
                        case TokenType::R_ROT: op_name = "operator|>>"; break;
                        case TokenType::LSHIFT: op_name = "operator<<"; break;
                        case TokenType::L_ROT: op_name = "operator<<<"; break;
                        default: break;
                        }
                        name_tok = Token(TokenType::IDENTIFIER, op_name, op_tok.pos);
                        this->advance();
                    } else {
                        res.failure(new InvalidSyntaxError("QC-T007: Expected method name after type(s)", this->current_tok.pos));
                        return res.to_prs();
                    }
                    std::vector<GenericType> genericsM;
                    parseGenerics(genericsM, res);
                    if (res.error) return res.to_prs();
                    if (this->current_tok.type == TokenType::LPAREN) {
                        ClassMethodInfo mi;
                        mi.name_tok = name_tok;
                        auto m_pr = this->func_def_multi(type_list, std::make_optional(name_tok), genericsM, true, false, modifiers);
                        if (std::holds_alternative<Error*>(m_pr)) return m_pr;
                        auto fn = std::get<FuncDefNode*>(m_pr);
                        mi.params.clear();
                        mi.modifiers = fn->modifiers;
                        mi.params.reserve(fn->params.size());
                        for (auto it = fn->params.begin(); it != fn->params.end(); ++it) { mi.params.push_back(*it); }
                        mi.return_types = fn->return_types;
                        mi.body = fn->body;
                        mi.is_constructor = false;
                        mi.generics = genericsM;
                        proof.additionalProof.push_back(mi);
                    }
                }
                this->advance();
                proof.proverName = Token(TokenType::IDENTIFIER, type_str, proved_pos);
                proof.conceptName = Token(TokenType::IDENTIFIER, conceptName, proved_pos);
                auto it = user_types.find(base_type_name(conceptName));
                if (it == user_types.end()) {
                    res.failure(new InvalidSyntaxError("QC-C005: Unknown concept '" + conceptName + "'", proved_pos));
                    return res.to_prs();
                }
                if (it->second.kind != UserTypeKind::Concept) {
                    res.failure(new InvalidSyntaxError("QC-C006: '" + conceptName + "' is not a concept", proved_pos));
                    return res.to_prs();
                }
                it->second.provees.push_back(proof);
                it = user_types.find(base_type_name(type_str));
                if (it != user_types.end()) { it->second.provees.push_back(proof); }
            } else {
                auto it = user_types.find(base_type_name(conceptName));
                if (it == user_types.end()) {
                    res.failure(new InvalidSyntaxError("QC-C005: Unknown concept '" + conceptName + "'", proved_pos));
                    return res.to_prs();
                }
                if (it->second.kind != UserTypeKind::Concept) {
                    res.failure(new InvalidSyntaxError("QC-C006: '" + conceptName + "' is not a concept", proved_pos));
                    return res.to_prs();
                }
                it->second.provees.push_back(
                    {Token(TokenType::IDENTIFIER, conceptName, proved_pos), Token(TokenType::IDENTIFIER, type_str, proved_pos), {}, namespaceStack});
                it = user_types.find(base_type_name(type_str));
                if (it != user_types.end()) {
                    it->second.provees.push_back(
                        {Token(TokenType::IDENTIFIER, conceptName, proved_pos), Token(TokenType::IDENTIFIER, type_str, proved_pos)});
                }
            }
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new InvalidSyntaxError("QC-MS004: Expected ';' after proves statement", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            return res.success(std::monostate{});
        }
        bool is_reference = type_str.ends_with("&");
        bool is_array = type_str.find("[]") != std::string::npos;
        if (type_str.find("[][]") != std::string::npos) {
            if (this->current_tok.type != TokenType::IDENTIFIER) {
                res.failure(new InvalidSyntaxError("QC-T010: Multi-dimensional types can only be used in function returns", this->current_tok.pos));
                return res.to_prs();
            }
            Token func_name = this->current_tok;
            this->advance();
            std::vector<GenericType> genericsM;
            parseGenerics(genericsM, res);
            if (res.error) return res.to_prs();
            if (this->current_tok.type == TokenType::LPAREN)
                return this->func_def_multi({type_tok}, func_name, genericsM, false, is_volatile, modifiers);
            res.failure(new InvalidSyntaxError("QC-S100: Expected '(' after function name", this->current_tok.pos));
            return res.to_prs();
        }
        std::vector<Token> return_types = {type_tok};
        if (this->current_tok.type == TokenType::COMMA) {
            size_t peek_idx = this->index + 1;
            bool is_multi_return = false;
            if (peek_idx < tokens.size()) {
                Token peek = tokens[peek_idx];
                if (peek.type == TokenType::KEYWORD) {
                    is_multi_return = true;
                } else if (peek.type == TokenType::IDENTIFIER) {
                    size_t saved = this->index;
                    Token saved_cur = this->current_tok;
                    this->advance();
                    auto peek_qual = this->try_parse_qualified_name();
                    if (peek_qual.has_value()) {
                        std::string base = *peek_qual;
                        size_t lc = base.rfind("::");
                        if (lc != std::string::npos) base = base.substr(lc + 2);
                        is_multi_return = (find_type(base) != nullptr || is_known_type(*peek_qual));
                    }
                    this->index = saved;
                    this->current_tok = saved_cur;
                }
            }
            if (is_multi_return) {
                while (this->current_tok.type == TokenType::COMMA) {
                    this->advance();
                    Position next_pos = this->current_tok.pos;
                    std::string next_str = parseTypeString();
                    if (next_str.empty()) return res.to_prs();
                    return_types.push_back(Token(TokenType::KEYWORD, next_str, next_pos));
                }
            }
        }
        if (this->current_tok.type != TokenType::IDENTIFIER) {
            res.failure(new InvalidSyntaxError("QC-S085: Expected identifier", this->current_tok.pos));
            return res.to_prs();
        }
        Token name_tok = this->current_tok;
        this->advance();
        std::vector<std::optional<int>> array_sizes;
        int dimensions = 0;
        while (this->current_tok.type == TokenType::LBRACKET) {
            this->advance();
            dimensions++;
            if (this->current_tok.type == TokenType::INT) {
                array_sizes.push_back(std::stoi(this->current_tok.value));
                this->advance();
            } else {
                array_sizes.push_back(std::nullopt);
            }
            if (this->current_tok.type != TokenType::RBRACKET) {
                res.failure(new InvalidSyntaxError("QC-S049: Expected ']'", this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
        }
        if (dimensions > 0) is_array = true;
        std::vector<GenericType> genericsM;
        parseGenerics(genericsM, res);
        if (res.error) return res.to_prs();
        if (this->current_tok.type == TokenType::LPAREN) {
            auto func_def = res.reg(this->func_def_multi(return_types, name_tok, genericsM, false, is_volatile, modifiers));
            if (res.error) return res.to_prs();
            return res.success(func_def);
        }
        if (is_volatile) { type_tok.value = "volatile " + type_tok.value; }
        if (this->current_tok.type == TokenType::COMMA) {
            std::vector<Token> var_names = {name_tok};
            std::vector<Token> var_types = {type_tok};
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                Position next_pos = this->current_tok.pos;
                std::string next_str = parseTypeString();
                if (next_str.empty()) return res.to_prs();
                var_types.push_back(Token(TokenType::KEYWORD, next_str, next_pos));
                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-S085: Expected identifier", this->current_tok.pos));
                    return res.to_prs();
                }
                var_names.push_back(this->current_tok);
                this->advance();
            }
            if (this->current_tok.type != TokenType::EQ) {
                res.failure(new InvalidSyntaxError("QC-S101: Expected '=' in multi-variable declaration", name_tok.pos));
                return res.to_prs();
            }
            this->advance();
            AnyNode value = res.reg(this->qout_expr());
            if (res.error) return res.to_prs();
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new MissingSemicolonError(this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            if (var_types.size() != var_names.size()) {
                res.failure(new InvalidSyntaxError("QC-T011: Number of types must match number of variables", var_names[0].pos));
                return res.to_prs();
            }
            return res.success(new MultiVarDeclNode(is_const, var_types, var_names, value));
        }
        if (return_types.size() > 1) {
            std::vector<Token> var_names = {name_tok};
            std::vector<Token> var_types = {type_tok};
            while (this->current_tok.type == TokenType::COMMA) {
                this->advance();
                Position next_pos = this->current_tok.pos;
                std::string next_str = parseTypeString();
                if (next_str.empty()) return res.to_prs();
                var_types.push_back(Token(TokenType::KEYWORD, next_str, next_pos));
                if (this->current_tok.type != TokenType::IDENTIFIER) {
                    res.failure(new InvalidSyntaxError("QC-S085: Expected identifier", this->current_tok.pos));
                    return res.to_prs();
                }
                var_names.push_back(this->current_tok);
                this->advance();
            }
            if (this->current_tok.type != TokenType::EQ) {
                res.failure(new InvalidSyntaxError("QC-S101: Expected '=' in multi-variable declaration", name_tok.pos));
                return res.to_prs();
            }
            this->advance();
            AnyNode value = res.reg(this->qout_expr());
            if (res.error) return res.to_prs();
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new MissingSemicolonError(this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            if (var_types.size() != var_names.size()) {
                res.failure(new InvalidSyntaxError("QC-T011: Number of types must match number of variables", var_names[0].pos));
                return res.to_prs();
            }
            return res.success(new MultiVarDeclNode(is_const, var_types, var_names, value));
        }
        if (is_array) {
            AnyNode value;
            if (this->current_tok.type == TokenType::EQ) {
                this->advance();
                value = res.reg(this->qout_expr());
                if (res.error) return res.to_prs();
            } else {
                if (is_const) {
                    res.failure(new InvalidSyntaxError("QC-S102: QC-CN07: const variables must be initialized", name_tok.pos));
                    return res.to_prs();
                }
                value = default_value_for_type(type_tok, name_tok.pos);
            }
            if (this->current_tok.type != TokenType::SEMICOLON) {
                res.failure(new MissingSemicolonError(this->current_tok.pos));
                return res.to_prs();
            }
            this->advance();
            return res.success(new ArrayDeclNode(is_const, type_tok, name_tok, value, dimensions, array_sizes));
        }
        AnyNode value;
        if (this->current_tok.type == TokenType::EQ) {
            this->advance();
            if (is_reference) {
                value = res.reg(this->qout_expr());
                if (res.error) return res.to_prs();
                if (this->current_tok.type != TokenType::SEMICOLON) {
                    res.failure(new MissingSemicolonError(this->current_tok.pos));
                    return res.to_prs();
                }
                this->advance();
                return res.success(new RefVarDeclNode(type_tok, name_tok, value, type_tok.pos));
            }
            value = res.reg(this->qout_expr());
            if (res.error) return res.to_prs();
        } else {
            if (is_const || is_reference) {
                res.failure(new InvalidSyntaxError("QC-S103: QC-CN07: const variables and references must be initialized", name_tok.pos));
                return res.to_prs();
            }
            value = default_value_for_type(type_tok, name_tok.pos);
        }
        if (this->current_tok.type != TokenType::SEMICOLON) {
            res.failure(new MissingSemicolonError(this->current_tok.pos));
            return res.to_prs();
        }
        this->advance();

        return res.success(new VarAssignNode(is_const, type_tok, name_tok, value, this->in_foreign));
    }
    // Expression statement: 2 + 3;
    AnyNode node = res.reg(this->assignment_expr());
    if (res.error) return res.to_prs();

    if (this->current_tok.type == TokenType::SEMICOLON) {
        this->advance();
        return res.success(modifiers.empty() ? node : new ModifierNode(modifiers, node));
    }

    res.failure(new MissingSemicolonError(this->current_tok.pos));
    return res.to_prs();
}
Aer Parser::parse() {
    std::vector<AnyNode> stmts;
    bool has_main = false;
    FuncDefNode* main_func_ptr = nullptr;

    while (this->current_tok.type != TokenType::EOFT) {
        Prs result = this->statement();

        if (std::holds_alternative<Error*>(result)) { return Aer{nullptr, std::get<Error*>(result)}; }

        AnyNode stmt = std::visit(
            [&has_main, &main_func_ptr](auto arg) -> AnyNode {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, FuncDefNode*>) {
                    if (arg->name_tok.has_value() && arg->name_tok->value == entrypointName) {
                        if (arg->return_types.empty() || arg->return_types[0].value != "int") {
                            std::string actual = arg->return_types.empty() ? "void" : arg->return_types[0].value;
                            throw InvalidSyntaxError("QC-S104: the entrypoint must return int, not " + actual, get_pos(arg));
                        }
                        if (!arg->params.empty() && (arg->params.front().type.value != "string[]" &&
                                                     ((((arg->params.size())) == 2 &&
                                                       (arg->params.front().type.value != "int" || arg->params.back().type.value != "char**"))))) {
                            throw InvalidSyntaxError(
                                "QC-S105: the entrypoint must have no parameters, take a integer argc and a char** argv, or take a array of strings.",
                                get_pos(arg));
                        }
                        if (!arg->params.empty()) {
                            if (arg->params.front().type.value == "string[]") {
                                main_type = MainType::RT_ARRAY;
                            } else {
                                main_type = MainType::C_STYLE;
                            }
                        }
                        has_main = true;
                        main_func_ptr = arg;
                    }
                }

                if constexpr (std::is_constructible_v<AnyNode, T>) { return AnyNode(arg); }
                return std::monostate{};
            },
            result);

        stmts.push_back(stmt);
    }

    if (!has_main && !no_main) {
        return Aer{nullptr, new Error("Missing the entrypoint function", "Program must have an 'int entrypointname()' function",
                                      Position(Position::INVALID_FILE_ID, 0, 0, 0))};
    }
    for (auto& [name, ut] : user_types) {
        if (ut.kind == UserTypeKind::Class && !ut.baseClassName.empty()) {
            std::string baseKey = ut.baseClassName;
            if (baseKey.find("::") == std::string::npos) {
                bool found = false;
                for (auto& [key, info] : user_types) {
                    if (key.find(base_type_name(baseKey)) != std::string::npos && info.kind == UserTypeKind::Class) {
                        baseKey = key;
                        found = true;
                        break;
                    }
                }
                if (!found) { throw InvalidSyntaxError("QC-S106: Base class '" + ut.baseClassName + "' not found", ut.pos); }
            }

            auto it = user_types.find(base_type_name(baseKey));
            if (it == user_types.end() || it->second.kind != UserTypeKind::Class) {
                throw InvalidSyntaxError("QC-S107: class inherits from non-class or non-existent object", ut.pos);
            }
        }
    }
    return Aer{new StatementsNode(stmts, true), nullptr, this->user_types};
}
