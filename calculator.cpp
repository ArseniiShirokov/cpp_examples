#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <variant>

inline bool IsOperation(char op) {
    return op == '*' || op == '/' || op == '+' || op == '-';
}

inline bool IsScope(char c) {
    return c == '(' || c == ')';
}

class Tokenizer {
public:
    Tokenizer(std::istream* in) : in_(in) {
    }

    enum TokenType { kUnknown, kNumber, kSymbol, kEnd };

    bool NextSub() {
        TokenType next_token_type = WatchNext();
        while (next_token_type == TokenType::kUnknown) {
            char c;
            in_->get(c);
            next_token_type = WatchNext();
        }
        char c = in_->peek();
        return c == '-';
    }

    TokenType WatchNext() {
        char c = in_->peek();
        if (std::isdigit(c)) {
            return TokenType::kNumber;
        } else if (IsOperation(c) || IsScope(c)) {
            return TokenType::kSymbol;
        } else if (c == EOF) {
            return TokenType::kEnd;
        } else {
            return TokenType::kUnknown;
        }
    }

    void Consume() {
        TokenType next_token_type = WatchNext();
        while (next_token_type == TokenType::kUnknown) {
            char c;
            in_->get(c);
            next_token_type = WatchNext();
        }

        if (next_token_type == TokenType::kSymbol) {
            type_ = TokenType::kSymbol;
            in_->get(symbol_);
        } else if (next_token_type == TokenType::kNumber) {
            char c = in_->peek();
            type_ = TokenType::kNumber;
            number_ = 0;
            while (std::isdigit(c)) {
                char digit;
                in_->get(digit);
                number_ = number_ * 10 + (digit - '0');
                c = in_->peek();
            }
        } else {
            type_ = TokenType::kEnd;
        }
    }

    TokenType GetType() {
        return type_;
    }

    int64_t GetNumber() {
        return number_;
    }

    char GetSymbol() {
        return symbol_;
    }

private:
    std::istream* in_;

    TokenType type_ = TokenType::kUnknown;
    int64_t number_;
    char symbol_;
};

class Expression {
public:
    virtual ~Expression() {
    }
    virtual int64_t Evaluate() = 0;
};

class Const : public Expression {
public:
    Const(int64_t value) : val_(value){};
    int64_t Evaluate() {
        return val_;
    }

private:
    int64_t val_;
};

class Operation : public Expression {
public:
    Operation(char op_type, Expression* left, Expression* right)
        : op_type_(op_type), left_(left), right_(right){};

    int64_t Evaluate() override {
        switch (op_type_) {
            case '*':
                std::cout << (left_->Evaluate() * right_->Evaluate()) << "\n";
                return left_->Evaluate() * right_->Evaluate();
            case '/':
                std::cout << (left_->Evaluate() / right_->Evaluate()) << "\n";
                return left_->Evaluate() / right_->Evaluate();
            case '+':
                std::cout << (left_->Evaluate() + right_->Evaluate()) << "\n";
                return left_->Evaluate() + right_->Evaluate();
            case '-':
                std::cout << (left_->Evaluate() - right_->Evaluate()) << "\n";
                return left_->Evaluate() - right_->Evaluate();
            default:
                assert("Unsupported operation");
                return 0;
        }
    }

private:
    char op_type_;
    std::unique_ptr<Expression> left_;
    std::unique_ptr<Expression> right_;
};

Expression* ParserE(Tokenizer* tok);
Expression* ParserT1(Tokenizer* tok);
Expression* ParserT2(Tokenizer* tok);
Expression* ParserT3(Tokenizer* tok);

std::unique_ptr<Expression> ParseExpression(Tokenizer* tok) {
    return std::unique_ptr<Expression>(ParserE(tok));
}

Expression* ParserE(Tokenizer* tok) {
    Expression* left = ParserT1(tok);
    if (tok->GetType() == Tokenizer::TokenType::kEnd) {
        return left;
    } else {
        while (tok->GetType() == Tokenizer::TokenType::kSymbol &&
               (tok->GetSymbol() == '-' || tok->GetSymbol() == '+')) {
            auto op = tok->GetSymbol();
            Expression* right = ParserT1(tok);
            Expression* vertex = new Operation(op, left, right);
            left = vertex;
        }
        return left;
    }
}

Expression* ParserT1(Tokenizer* tok) {
    Expression* left = ParserT2(tok);
    tok->Consume();
    if (tok->GetType() == Tokenizer::TokenType::kEnd) {
        return left;
    } else {
        while (tok->GetType() == Tokenizer::TokenType::kSymbol &&
               (tok->GetSymbol() == '*' || tok->GetSymbol() == '/')) {
            auto op = tok->GetSymbol();
            Expression* right = ParserT2(tok);
            Expression* vertex = new Operation(op, left, right);
            left = vertex;
            tok->Consume();
        }
        return left;
    }
}

Expression* ParserT2(Tokenizer* tok) {
    if (tok->NextSub()) {
        Expression* left = new Const(0);
        tok->Consume();
        Expression* right = ParserT3(tok);
        Expression* vertex = new Operation('-', left, right);
        return vertex;
    } else {
        return ParserT3(tok);
    }
}

Expression* ParserT3(Tokenizer* tok) {
    tok->Consume();
    Expression* ans = nullptr;
    if (tok->GetType() == Tokenizer::TokenType::kNumber) {
        return new Const(tok->GetNumber());
    } else if (tok->GetType() == Tokenizer::TokenType::kSymbol &&
               (tok->GetSymbol() == '(' || tok->GetSymbol() == ')')) {
        ans = ParserE(tok);
    }
    return ans;
}
