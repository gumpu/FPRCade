#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "instructions.h"
#include "decompile.h"


#define INSTRUCTION_COUNT (256)
XTfunction_T semantic_table[INSTRUCTION_COUNT] = {
/*  0 */    instr_halt,
/*  1 */    instr_load_number,
/*  2 */    instr_enter,
/*  3 */    instr_add,
/*  4 */    instr_compile,
/*  5 */    instr_execute,
/*  6 */    instr_here,
/*  7 */    instr_dot,
/*  8 */    instr_allot,
/*  9 */    instr_fetch,   /* @ */
/* 10 */    instr_store,   /* ! */
/* 11 */    instr_leave_colon, /* ; runtime */
/* 12 */    instr_dup,
/* 13 */    instr_drop,
/* 14 */    instr_equal,   /* = */
/* 15 */    instr_emit,
/* 16 */    instr_bl,
/* 17 */    instr_jmp,
/* 18 */    instr_nop,
/* 19 */    instr_jmp_false,
/* 20 */    instr_begin,
/* 21 */    instr_again,
/* 22 */    instr_if,
/* 23 */    instr_else,
/* 24 */    instr_then,
/* 25 */    instr_colon,
/* 26 */    instr_semicolon,
/* 27 */    instr_parse_name,
/* 28 */    instr_literal,
/* 29 */    instr_premain,
/* 30 */    instr_paccept,
/* 31 */    instr_cr,
/* 32 */    instr_find2,
/* 33 */    instr_pnumber,
/* 34 */    instr_over,
/* 35 */    instr_swap,
/* 36 */    instr_decompile
};



/* --------------------------------------------------------------------*/

instruction_pointer_t instr_halt(T_Context* ctx)
{
    printf("\nprocessor halted\n");
    exit(100);
    return 0;
}

instruction_pointer_t instr_enter(T_Context* ctx)
{
    /* Save the location of the next instruction */
    push(ctx->dataspace, &(ctx->return_stack), (ctx->ip + 3));
    /* Load position code the be executed */
    instruction_pointer_t ip = fetch16bits(ctx->dataspace, (ctx->ip)+1);
    return ip;
}

instruction_pointer_t instr_leave_colon(T_Context* ctx)
{
    /* Restore the ip */
    instruction_pointer_t ip = pop(ctx->dataspace, &(ctx->return_stack));
    return ip;
}

instruction_pointer_t instr_add(T_Context* ctx)
{
    cell_t v1 = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t v2 = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t r = v1 + v2;
    push(ctx->dataspace, &(ctx->data_stack), r);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_load_number(T_Context* ctx)
{
    cell_t v = fetch16bits(ctx->dataspace, (ctx->ip)+1);
    push(ctx->dataspace, &(ctx->data_stack), v);
    return (ctx->ip + 3);
}

instruction_pointer_t instr_allot(T_Context* ctx)
{
    cell_t n = pop(ctx->dataspace, &(ctx->data_stack));
    n += fetch16bits(ctx->dataspace, LOC_WHERE);
    store16bits(ctx->dataspace, LOC_WHERE, n);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_compile(T_Context* ctx)
{
    execution_token_t xt = pop(ctx->dataspace, &(ctx->data_stack));
    T_DictHeader* entry = (T_DictHeader*)(ctx->dataspace + xt);
    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);

    instruction_t instruction = entry->semantics;
    if (entry->flags & F_INSTRUCTION) {
        store8bits(ctx->dataspace, here, instruction);
        here += 1;
    } else {
        dataspace_index_t ip =
            xt + sizeof(T_DictHeader) + entry->name_length + 1;
        store8bits(ctx->dataspace, here, OPCODE_ENTER);
        store16bits(ctx->dataspace, here + 1, ip);
        here += 3;
    }

    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_execute(T_Context* ctx)
{
    execution_token_t xt = pop(ctx->dataspace, &(ctx->data_stack));
    T_DictHeader* entry = (T_DictHeader*)(ctx->dataspace + xt);
    instruction_pointer_t ip = (ctx->ip);
    ip++;
    if (entry->flags & F_INSTRUCTION) {
        instruction_t instruction = entry->semantics;
        (void)(semantic_table[instruction](ctx));
    } else {
        push(ctx->dataspace, &(ctx->return_stack), ip);

        ip = xt + sizeof(T_DictHeader) + entry->name_length + 1;
    }
    return ip;
}

instruction_pointer_t instr_here(T_Context* ctx)
{
    dataspace_index_t here = ctx->dataspace[LOC_WHERE];
    push(ctx->dataspace, &(ctx->data_stack), here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_dot(T_Context* ctx)
{
    int16_t n = (int16_t)pop(ctx->dataspace, &(ctx->data_stack));
    printf("%d", n);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_emit(T_Context* ctx)
{
    char v = (char)pop(ctx->dataspace, &(ctx->data_stack));
    printf("%c", v);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_cr(T_Context* ctx)
{
    printf("\n");
    return (ctx->ip + 1);
}

instruction_pointer_t instr_bl(T_Context* ctx)
{
    cell_t n1 = ' ';
    push(ctx->dataspace, &(ctx->data_stack), n1);
    return (ctx->ip + 1);
}

/*
 * FIND2  ( c_addr u -- c_addr 0 | xt 1 | xt -1 )
 *
 * 0 - not found
 */
instruction_pointer_t instr_find2(T_Context* ctx)
{
    cell_t u = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t c_addr = pop(ctx->dataspace, &(ctx->data_stack));
    char* string = (char*) (ctx->dataspace + c_addr);
    execution_token_t xt = dict_find2(ctx, string, u);
    if (xt == NULL_TOKEN) {
        push(ctx->dataspace, &(ctx->data_stack), c_addr);
        push(ctx->dataspace, &(ctx->data_stack), 0);
    } else {
        T_DictHeader* header = dict_get_entry(ctx, xt);
        push(ctx->dataspace, &(ctx->data_stack), xt);
        if (header->flags & F_IMMEDIATE) {
            push(ctx->dataspace, &(ctx->data_stack), 1);
        } else {
            push(ctx->dataspace, &(ctx->data_stack), -1);
        }
    }
    return (ctx->ip + 1);
}

static int16_t to_digit(char c, uint16_t base)
{
    int16_t d;

    d = (int16_t)c;
    if ((c >= 'a') && (c <= 'z')) {
        d = 10 + c - 'a';
    } else if ((c >= 'A') && (c <= 'Z')) {
        d = 10 + c - 'A';
    } else {
        d = d - '0';
    }
    if ((d >= base) || (d < 0)) {
        d = -1;
    }

    return d;
}

/*
 * (NUMBER)  ( c_addr u -- c_addr 0 | n 1 )
 */
instruction_pointer_t instr_pnumber(T_Context* ctx)
{
    cell_t u = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t c_addr = pop(ctx->dataspace, &(ctx->data_stack));
    char* string = (char*) (ctx->dataspace + c_addr);
    bool negative = false;
    bool wrong = false;
    uint16_t base;
    uint32_t result = 0;
    char first_char = string[0];

    if (first_char == '\'') {
        if ((u == 3) && (string[2] == '\'')) {
            result = (int32_t)string[1];
        } else {
            wrong = true;
        }
    } else {
        uint32_t i = 1;

        switch (first_char) {
            case '%':
                {
                    base = 2;
                    if (u > 1) {
                        negative = (string[1] == '-');
                        wrong = (u == 2);
                    } else {
                        wrong = true;
                    }
                }
                break;
            case '#':
                {
                    base = 10;
                    if (u > 1) {
                        negative = (string[1] == '-');
                        wrong = (u == 2);
                    } else {
                        wrong = true;
                    }
                }
                break;
            case '$':
                {
                    base = 16;
                    if (u > 1) {
                        negative = (string[1] == '-');
                        wrong = (u == 2);
                    } else {
                        wrong = true;
                    }
                }
                break;
            case '-':
                negative = true;
                /* TODO Read from memory (BASE) */
                base = 10;
                break;
            default:
                /* TODO Read from memory (BASE) */
                {
                    base = 10;
                    int32_t d = to_digit(string[0], base);
                    if (d >= 0) {
                        result += d;
                    } else {
                        wrong = true;
                        break;
                    }
                }
        }

        for (; (i < u) && !wrong; i++) {
            result = result*base;
            if (string[i] == '-') {
                if (negative) {
                    wrong = true;
                    break;
                }
            } else {
                int32_t d = to_digit(string[i], base);
                if (d >= 0) {
                    result += d;
                } else {
                    wrong = true;
                    break;
                }
            }
        }
    }

    if (wrong) {
        push(ctx->dataspace, &(ctx->data_stack), c_addr);
        push(ctx->dataspace, &(ctx->data_stack), FORTH_FALSE);
    } else {
        if (negative) {
            result = 0 - result;
        }
        push(ctx->dataspace, &(ctx->data_stack), result);
        push(ctx->dataspace, &(ctx->data_stack), 1);
    }

    return (ctx->ip + 1);
}

/*
 * ( x1 x2 -- x1 x2 x1 )
 */

instruction_pointer_t instr_over(T_Context* ctx)
{
    cell_t x2 = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t x1 = pop(ctx->dataspace, &(ctx->data_stack));
    push(ctx->dataspace, &(ctx->data_stack), x1);
    push(ctx->dataspace, &(ctx->data_stack), x2);
    push(ctx->dataspace, &(ctx->data_stack), x1);
    return (ctx->ip + 1);
}

/*
 * ( x2 x1 -- x1 x2 )
 */

instruction_pointer_t instr_swap(T_Context* ctx)
{
    cell_t x1 = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t x2 = pop(ctx->dataspace, &(ctx->data_stack));
    push(ctx->dataspace, &(ctx->data_stack), x1);
    push(ctx->dataspace, &(ctx->data_stack), x2);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_dup(T_Context* ctx)
{
    cell_t n1 = pop(ctx->dataspace, &(ctx->data_stack));
    push(ctx->dataspace, &(ctx->data_stack), n1);
    push(ctx->dataspace, &(ctx->data_stack), n1);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_drop(T_Context* ctx)
{
    (void)pop(ctx->dataspace, &(ctx->data_stack));
    return (ctx->ip + 1);
}

instruction_pointer_t instr_equal(T_Context* ctx)
{
    cell_t n1 = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t n2 = pop(ctx->dataspace, &(ctx->data_stack));
    if (n1 == n2) {
        push(ctx->dataspace, &(ctx->data_stack), FORTH_TRUE);
    } else {
        push(ctx->dataspace, &(ctx->data_stack), FORTH_FALSE);
    }
    return (ctx->ip + 1);
}

instruction_pointer_t instr_fetch(T_Context* ctx)
{
    cell_t xt = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t v = fetch16bits(ctx->dataspace, xt);
    push(ctx->dataspace, &(ctx->data_stack), v);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_store(T_Context* ctx)
{
    cell_t v  = pop(ctx->dataspace, &(ctx->data_stack));
    cell_t xt = pop(ctx->dataspace, &(ctx->data_stack));
    store16bits(ctx->dataspace, xt, v);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_nop(T_Context* ctx)
{
    return (ctx->ip + 1);
}

instruction_pointer_t instr_jmp(T_Context* ctx)
{
    return fetch16bits(ctx->dataspace, (ctx->ip)+1);
}

instruction_pointer_t instr_jmp_false(T_Context* ctx)
{
    cell_t v  = pop(ctx->dataspace, &(ctx->data_stack));
    if (v) {
        return ctx->ip + 3;
    } else {
        return fetch16bits(ctx->dataspace, (ctx->ip)+1);
    }
}

instruction_pointer_t instr_begin(T_Context* ctx)
{
    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    push(ctx->dataspace, &(ctx->control_stack), here);
    store8bits(ctx->dataspace, here, OPCODE_NOP);
    here += 1;
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_again(T_Context* ctx)
{
    dataspace_index_t location;
    location  = pop(ctx->dataspace, &(ctx->control_stack));
    instruction_pointer_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    /* jmp <location> */
    store8bits(ctx->dataspace, here, OPCODE_JMP);
    store16bits(ctx->dataspace, here + 1, location);
    here += 3;
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_if(T_Context* ctx)
{
    instruction_pointer_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    /* jmf <location> */
    store8bits(ctx->dataspace, here, OPCODE_JMP_FALSE);
    store16bits(ctx->dataspace, here + 1, 0xDEAD);
    /* Location will be filled in by ELSE or END */
    push(ctx->dataspace, &(ctx->control_stack), here + 1);
    here += 3;
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_else(T_Context* ctx)
{
    instruction_pointer_t location  = pop(ctx->dataspace, &(ctx->control_stack));
    instruction_pointer_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    store16bits(ctx->dataspace, location, here + 3);
    /* jmp <location>, will be filled in by THEN */
    store8bits(ctx->dataspace, here, OPCODE_JMP);
    store16bits(ctx->dataspace, here + 1, 0xDEAD);
    push(ctx->dataspace, &(ctx->control_stack), here + 1);
    here += 3;
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_then(T_Context* ctx)
{
    instruction_pointer_t location  = pop(ctx->dataspace, &(ctx->control_stack));
    instruction_pointer_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    store16bits(ctx->dataspace, location, here);
    store8bits(ctx->dataspace, here, OPCODE_NOP);
    here += 1;
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

/*
 * ( "<spaces>name<space>" -- c-addr u )
 * c-addr points to 'name'
 * u will be the length of 'name'
 *
 * In case nothing is found u will be zero.
 */
instruction_pointer_t instr_parse_name(T_Context* ctx)
{
    cell_t u = 0;
    cell_t c_addr = 0;
    dataspace_index_t index = fetch16bits(ctx->dataspace, LOC_INPUT_BUFFER_INDEX);
    dataspace_index_t n = fetch16bits(ctx->dataspace, LOC_INPUT_BUFFER_COUNT);
    char* source = (char*)(ctx->dataspace + LOC_INPUT_BUFFER);
    dataspace_index_t i;
    for (i = index; isspace(source[i]) && i < n; ++i);
    c_addr = LOC_INPUT_BUFFER + i;

    if (i < n) {
        for (; !isspace(source[i]) && i < n; ++i) {
            ++u;
        }
    } else {
        u = 0;
    }
    store16bits(ctx->dataspace, LOC_INPUT_BUFFER_INDEX, i);
    push(ctx->dataspace, &(ctx->data_stack), c_addr);
    push(ctx->dataspace, &(ctx->data_stack), u);

    return (ctx->ip + 1);
}


instruction_pointer_t instr_semicolon(T_Context* ctx)
{
    dataspace_index_t dict_new = ctx->dict_new;
    T_DictHeader* header = (T_DictHeader*)(ctx->dataspace + dict_new);
    header->previous = ctx->dict_head;
    /* Make new entry findable */
    ctx->dict_head = ctx->dict_new;
    ctx->dict_new  = 0U;

    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);

    store8bits(ctx->dataspace, here, OPCODE_LEAVE_COLON);
    here++;
    /* Make aligned */
    here += (here &0x01);
    /* TODO Check memory overflow (dict full) */
    store16bits(ctx->dataspace, LOC_WHERE, here);
    /* Switch to interpretation mode */
    store16bits(ctx->dataspace, LOC_STATE, FORTH_FALSE);
    return (ctx->ip + 1);
}

instruction_pointer_t instr_colon(T_Context* ctx)
{
    (void)instr_parse_name(ctx);
    cell_t u = pop(ctx->dataspace, &(ctx->data_stack));
    dataspace_index_t c_addr = pop(ctx->dataspace, &(ctx->data_stack));

    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    /* Remember where we started, so during ';' we can finalize */
    ctx->dict_new = here;

    T_DictHeader* header = (T_DictHeader*)(ctx->dataspace + here);
    char* source = (char*)(ctx->dataspace + c_addr);
    header->previous = 0;
    header->flags = F_COLONDEF;
    header->name_length = u;
    /* TODO check on name length */
    memcpy(header->name, source, u);
    /* Make it nul terminated */
    header->name[u] = '\0';
    uint16_t n = sizeof(T_DictHeader);
    n += (u + 1);  /* Also count '\0' */
    here += n;
    /* TODO Check memory overflow (dict full) */
    store16bits(ctx->dataspace, LOC_WHERE, here);
    /* Switch to compilation mode */
    store16bits(ctx->dataspace, LOC_STATE, FORTH_TRUE);
    return (ctx->ip + 1);
}

/*
 * LITERAL ( u -- )
 *
 * Append the run-time semantics below to the current definition.
 *
 * Run-time:
 *      ( -- u )
 *      push u on the data stack
 */
instruction_pointer_t instr_literal(T_Context* ctx)
{
    cell_t u = pop(ctx->dataspace, &(ctx->data_stack));

    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    store8bits(ctx->dataspace, here, OPCODE_LDN);
    store16bits(ctx->dataspace, here + 1, u);
    here += 3;

    store16bits(ctx->dataspace, LOC_WHERE, here);
    return (ctx->ip + 1);
}

/*
 * (REMAIN)  ( -- u | 0 )
 *
 * How many unparsed characters are left in the input buffer.
 *
 * u -- there are u chars of soure data remaining
 * 0 -- source buffer is empty
 *
 */
instruction_pointer_t instr_premain(T_Context* ctx)
{
    dataspace_index_t current = fetch16bits(ctx->dataspace, LOC_INPUT_BUFFER_INDEX);
    /* Total number of characters in the input buffer */
    cell_t count = fetch16bits(ctx->dataspace, LOC_INPUT_BUFFER_COUNT);
    if (current < count) {
        cell_t u = (count - current);
        push(ctx->dataspace, &(ctx->data_stack), u);
    } else {
        push(ctx->dataspace, &(ctx->data_stack), 0);
    }
    return (ctx->ip + 1);
}

instruction_pointer_t instr_paccept(T_Context* ctx)
{
    char buffer[256];
    fgets(buffer, 255, stdin);
    set_code(ctx, buffer);
    return (ctx->ip + 1);
}


instruction_pointer_t instr_decompile(T_Context* ctx)
{
    decompile_dictionary(ctx);
    return (ctx->ip + 1);
}


/* ------------------------ end of file -------------------------------*/
