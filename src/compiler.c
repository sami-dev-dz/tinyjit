#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "compiler.h"

/* ======================== 1. LEXER ======================== */

typedef enum {
    TOK_EOF,
    TOK_FN, TOK_VAR, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_RETURN,
    TOK_IDENT, TOK_NUMBER,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMICOLON,
    TOK_ASSIGN, TOK_EQ, TOK_LT,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH
} TokenType;

typedef struct {
    TokenType type;
    char text[64];
    int64_t number;
    int line;
} Token;

typedef struct {
    const char *src;
    size_t pos;
    int line;
    Token current;
} Lexer;

static void next_token(Lexer *lex) {
    while (lex->src[lex->pos] != '\0') {
        char c = lex->src[lex->pos];
        if (isspace((unsigned char)c)) {
            if (c == '\n') lex->line++;
            lex->pos++;
            continue;
        }
        // Commentaires //
        if (c == '/' && lex->src[lex->pos + 1] == '/') {
            while (lex->src[lex->pos] != '\0' && lex->src[lex->pos] != '\n') {
                lex->pos++;
            }
            continue;
        }
        break;
    }

    char c = lex->src[lex->pos];
    lex->current.line = lex->line;
    lex->current.text[0] = '\0';

    if (c == '\0') {
        lex->current.type = TOK_EOF;
        return;
    }

    if (isdigit((unsigned char)c)) {
        char *end;
        lex->current.type = TOK_NUMBER;
        lex->current.number = strtoll(&lex->src[lex->pos], &end, 10);
        lex->pos = (size_t)(end - lex->src);
        return;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        size_t len = 0;
        while (isalnum((unsigned char)lex->src[lex->pos]) || lex->src[lex->pos] == '_') {
            if (len < sizeof(lex->current.text) - 1) {
                lex->current.text[len++] = lex->src[lex->pos];
            }
            lex->pos++;
        }
        lex->current.text[len] = '\0';

        if (strcmp(lex->current.text, "fn") == 0) lex->current.type = TOK_FN;
        else if (strcmp(lex->current.text, "var") == 0) lex->current.type = TOK_VAR;
        else if (strcmp(lex->current.text, "if") == 0) lex->current.type = TOK_IF;
        else if (strcmp(lex->current.text, "else") == 0) lex->current.type = TOK_ELSE;
        else if (strcmp(lex->current.text, "while") == 0) lex->current.type = TOK_WHILE;
        else if (strcmp(lex->current.text, "return") == 0) lex->current.type = TOK_RETURN;
        else lex->current.type = TOK_IDENT;
        return;
    }

    lex->pos++;
    switch (c) {
        case '(': lex->current.type = TOK_LPAREN; break;
        case ')': lex->current.type = TOK_RPAREN; break;
        case '{': lex->current.type = TOK_LBRACE; break;
        case '}': lex->current.type = TOK_RBRACE; break;
        case ',': lex->current.type = TOK_COMMA; break;
        case ';': lex->current.type = TOK_SEMICOLON; break;
        case '+': lex->current.type = TOK_PLUS; break;
        case '-': lex->current.type = TOK_MINUS; break;
        case '*': lex->current.type = TOK_STAR; break;
        case '/': lex->current.type = TOK_SLASH; break;
        case '<': lex->current.type = TOK_LT; break;
        case '=':
            if (lex->src[lex->pos] == '=') {
                lex->pos++;
                lex->current.type = TOK_EQ;
            } else {
                lex->current.type = TOK_ASSIGN;
            }
            break;
        default:
            fprintf(stderr, "[Compiler] Ligne %d: caractere inattendu '%c'\n", lex->line, c);
            lex->current.type = TOK_EOF;
            break;
    }
}

/* ======================== 2. PARSEUR & ÉMISSION BYTECODE ======================== */

typedef struct {
    Lexer lex;
    Program *prog;
    BytecodeFunction *fn;
    char locals[64][64];
    int local_count;
} Parser;

static void consume(Parser *p, TokenType expected, const char *msg) {
    if (p->lex.current.type != expected) {
        fprintf(stderr, "[Compiler] Erreur ligne %d: %s\n", p->lex.current.line, msg);
        exit(1);
    }
    next_token(&p->lex);
}

static size_t emit(Parser *p, OpCode op, int32_t arg) {
    BytecodeFunction *fn = p->fn;
    fn->code = realloc(fn->code, (fn->code_count + 1) * sizeof(Instruction));
    fn->code[fn->code_count] = (Instruction){ .op = op, .arg = arg };
    return fn->code_count++;
}

static int32_t parser_add_constant(Parser *p, int64_t val) {
    BytecodeFunction *fn = p->fn;
    for (size_t i = 0; i < fn->const_count; i++) {
        if (fn->constants[i] == val) return (int32_t)i;
    }
    fn->constants = realloc(fn->constants, (fn->const_count + 1) * sizeof(int64_t));
    fn->constants[fn->const_count] = val;
    return (int32_t)fn->const_count++;
}

static int resolve_local(Parser *p, const char *name) {
    for (int i = 0; i < p->local_count; i++) {
        if (strcmp(p->locals[i], name) == 0) return i;
    }
    return -1;
}

static int add_local(Parser *p, const char *name) {
    int idx = p->local_count++;
    snprintf(p->locals[idx], sizeof(p->locals[idx]), "%s", name);
    p->fn->num_locals = p->local_count;
    return idx;
}

static int find_function_index(Program *prog, const char *name) {
    for (size_t i = 0; i < prog->count; i++) {
        if (strcmp(prog->functions[i]->name, name) == 0) return (int)i;
    }
    return -1;
}

static void parse_expression(Parser *p);
static void parse_statement(Parser *p);

static void parse_primary(Parser *p) {
    if (p->lex.current.type == TOK_NUMBER) {
        int32_t c_idx = parser_add_constant(p, p->lex.current.number);
        emit(p, OP_CONST, c_idx);
        next_token(&p->lex);
        return;
    }

    if (p->lex.current.type == TOK_IDENT) {
        char name[64];
        snprintf(name, sizeof(name), "%s", p->lex.current.text);
        next_token(&p->lex);

        // Appel de fonction : ident(arg1, arg2, ...)
        if (p->lex.current.type == TOK_LPAREN) {
            next_token(&p->lex);
            if (p->lex.current.type != TOK_RPAREN) {
                parse_expression(p);
                while (p->lex.current.type == TOK_COMMA) {
                    next_token(&p->lex);
                    parse_expression(p);
                }
            }
            consume(p, TOK_RPAREN, "Attendu ')' apres les arguments");

            int fn_idx = find_function_index(p->prog, name);
            if (fn_idx < 0) {
                fprintf(stderr, "[Compiler] Fonction inconnue '%s'\n", name);
                exit(1);
            }
            emit(p, OP_CALL, fn_idx);
            return;
        }

        // Variable locale
        int slot = resolve_local(p, name);
        if (slot < 0) {
            fprintf(stderr, "[Compiler] Variable non declaree '%s'\n", name);
            exit(1);
        }
        emit(p, OP_LOAD, slot);
        return;
    }

    if (p->lex.current.type == TOK_LPAREN) {
        next_token(&p->lex);
        parse_expression(p);
        consume(p, TOK_RPAREN, "Attendu ')'");
        return;
    }

    fprintf(stderr, "[Compiler] Expression invalide ligne %d\n", p->lex.current.line);
    exit(1);
}

static void parse_term(Parser *p) {
    parse_primary(p);
    while (p->lex.current.type == TOK_STAR || p->lex.current.type == TOK_SLASH) {
        TokenType op = p->lex.current.type;
        next_token(&p->lex);
        parse_primary(p);
        emit(p, op == TOK_STAR ? OP_MUL : OP_DIV, 0);
    }
}

static void parse_additive(Parser *p) {
    parse_term(p);
    while (p->lex.current.type == TOK_PLUS || p->lex.current.type == TOK_MINUS) {
        TokenType op = p->lex.current.type;
        next_token(&p->lex);
        parse_term(p);
        emit(p, op == TOK_PLUS ? OP_ADD : OP_SUB, 0);
    }
}

static void parse_expression(Parser *p) {
    parse_additive(p);
    while (p->lex.current.type == TOK_LT || p->lex.current.type == TOK_EQ) {
        TokenType op = p->lex.current.type;
        next_token(&p->lex);
        parse_additive(p);
        emit(p, op == TOK_LT ? OP_LT : OP_EQ, 0);
    }
}

static void parse_block(Parser *p) {
    consume(p, TOK_LBRACE, "Attendu '{'");
    while (p->lex.current.type != TOK_RBRACE && p->lex.current.type != TOK_EOF) {
        parse_statement(p);
    }
    consume(p, TOK_RBRACE, "Attendu '}'");
}

static void parse_statement(Parser *p) {
    if (p->lex.current.type == TOK_VAR) {
        next_token(&p->lex);
        char name[64];
        snprintf(name, sizeof(name), "%s", p->lex.current.text);
        consume(p, TOK_IDENT, "Attendu nom de variable apres 'var'");
        consume(p, TOK_ASSIGN, "Attendu '=' dans la declaration");
        parse_expression(p);
        consume(p, TOK_SEMICOLON, "Attendu ';'");
        int slot = add_local(p, name);
        emit(p, OP_STORE, slot);
        return;
    }

    if (p->lex.current.type == TOK_RETURN) {
        next_token(&p->lex);
        parse_expression(p);
        consume(p, TOK_SEMICOLON, "Attendu ';'");
        emit(p, OP_RET, 0);
        return;
    }

    if (p->lex.current.type == TOK_IF) {
        next_token(&p->lex);
        consume(p, TOK_LPAREN, "Attendu '(' apres 'if'");
        parse_expression(p);
        consume(p, TOK_RPAREN, "Attendu ')'");

        size_t jump_false_idx = emit(p, OP_JUMP_IF_FALSE, 0);
        parse_block(p);

        if (p->lex.current.type == TOK_ELSE) {
            size_t jump_end_idx = emit(p, OP_JUMP, 0);
            p->fn->code[jump_false_idx].arg = (int32_t)p->fn->code_count;
            next_token(&p->lex);
            parse_block(p);
            p->fn->code[jump_end_idx].arg = (int32_t)p->fn->code_count;
        } else {
            p->fn->code[jump_false_idx].arg = (int32_t)p->fn->code_count;
        }
        return;
    }

    if (p->lex.current.type == TOK_WHILE) {
        next_token(&p->lex);
        size_t loop_start = p->fn->code_count;
        consume(p, TOK_LPAREN, "Attendu '(' apres 'while'");
        parse_expression(p);
        consume(p, TOK_RPAREN, "Attendu ')'");

        size_t jump_exit = emit(p, OP_JUMP_IF_FALSE, 0);
        parse_block(p);
        emit(p, OP_JUMP, (int32_t)loop_start);
        p->fn->code[jump_exit].arg = (int32_t)p->fn->code_count;
        return;
    }

    if (p->lex.current.type == TOK_IDENT) {
        char name[64];
        snprintf(name, sizeof(name), "%s", p->lex.current.text);
        next_token(&p->lex);

        if (p->lex.current.type == TOK_ASSIGN) {
            next_token(&p->lex);
            parse_expression(p);
            consume(p, TOK_SEMICOLON, "Attendu ';'");
            int slot = resolve_local(p, name);
            if (slot < 0) {
                fprintf(stderr, "[Compiler] Variable inconnue '%s'\n", name);
                exit(1);
            }
            emit(p, OP_STORE, slot);
            return;
        }
    }

    fprintf(stderr, "[Compiler] Instruction invalide ligne %d\n", p->lex.current.line);
    exit(1);
}

static void parse_function(Parser *p) {
    consume(p, TOK_FN, "Attendu 'fn'");
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "%s", p->lex.current.text);
    consume(p, TOK_IDENT, "Attendu nom de fonction");

    // Pré-enregistrer la fonction dans Program pour permettre la récursivité
    BytecodeFunction *fn = calloc(1, sizeof(BytecodeFunction));
    fn->name = strdup(fn_name);

    p->prog->functions = realloc(p->prog->functions, (p->prog->count + 1) * sizeof(BytecodeFunction *));
    p->prog->functions[p->prog->count++] = fn;
    if (strcmp(fn_name, "main") == 0) {
        p->prog->entry_fn = fn;
    }

    p->fn = fn;
    p->local_count = 0;

    consume(p, TOK_LPAREN, "Attendu '('");
    if (p->lex.current.type != TOK_RPAREN) {
        add_local(p, p->lex.current.text);
        fn->num_args++;
        consume(p, TOK_IDENT, "Attendu nom de parametre");

        while (p->lex.current.type == TOK_COMMA) {
            next_token(&p->lex);
            add_local(p, p->lex.current.text);
            fn->num_args++;
            consume(p, TOK_IDENT, "Attendu nom de parametre");
        }
    }
    consume(p, TOK_RPAREN, "Attendu ')'");

    parse_block(p);

    // Garantir un OP_RET implicite en fin de fonction si absent
    if (fn->code_count == 0 || fn->code[fn->code_count - 1].op != OP_RET) {
        int32_t zero_idx = parser_add_constant(p, 0);
        emit(p, OP_CONST, zero_idx);
        emit(p, OP_RET, 0);
    }
}

Program *compile_source(const char *source) {
    Program *prog = calloc(1, sizeof(Program));
    Parser parser = {
        .lex = { .src = source, .pos = 0, .line = 1 },
        .prog = prog
    };
    next_token(&parser.lex);

    while (parser.lex.current.type != TOK_EOF) {
        parse_function(&parser);
    }

    // Relier la table de fonctions à chaque BytecodeFunction pour OP_CALL
    for (size_t i = 0; i < prog->count; i++) {
        prog->functions[i]->func_table = prog->functions;
        prog->functions[i]->func_table_count = prog->count;
    }

    return prog;
}

void free_program(Program *prog) {
    if (!prog) return;
    for (size_t i = 0; i < prog->count; i++) {
        BytecodeFunction *fn = prog->functions[i];
        free((void *)fn->name);
        free(fn->constants);
        free(fn->code);
        free(fn);
    }
    free(prog->functions);
    free(prog);
}