#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include "ff.h"
#include "fcore.h"
#include "instructions.h"

/* --------------------------------------------------------------------*/

static void compile_outer_interpreter(T_Context* ctx);
static void usage(void);
static uint32_t parse_options(int argc, char** argv);

/* --------------------------------------------------------------------*/


/*
 * The code for outer interpreter split in individual words so they can be
 * easily digested by the mini compiler.
 */

char* outer_interpreter_code[] = {
    "BEGIN",
        "(REMAIN)", "0", "=",
        "IF",
            "CR", "75", "79", "EMIT", "EMIT", "CR",
            "(ACCEPT)",
        "ELSE",
            "PARSE-NAME",  /* ( "<spaces>name<space>" -- c_addr u ) */
            "DUP",         /*    --  c_addr u u */
            "IF",          /*    --  c_addr u */
                "SWAP",    /*    --  u c_addr */
                "OVER",    /*    --  u c_addr u */
                "FIND2",   /* ( c_addr u -- c_addr 0 | xt 1 | xt -1 ) */
                           /*  u + c_addr 0 | xt 1 | xt -1  */
                "DUP",
                "IF",
                    /* u xt 1|-1 */
                    "STATE", "@",
                    "IF",
                        "1", "=",  /* Immediate check  1/-1 */
                                   /* u xt */
                        "IF",
                            "SWAP", "DROP",
                            "EXECUTE",
                        "ELSE",
                            "SWAP", "DROP",
                            "COMPILE,",
                        "THEN",
                    "ELSE",
                                 /* u xt 1|-1 */
                        "DROP",  /* Drop the flag */
                        "SWAP",  /* xt u */
                        "DROP",  /* xt */
                        "EXECUTE",
                    "THEN",
                "ELSE",
                    /* Not a word we know, it might be a number */
                    "DROP", /* Drop the flag */
                    "SWAP", /* u c_addr -> c_addr u */
                    "(NUMBER)",
                    "IF",
                        "STATE", "@",
                        "IF",
                            // Similar to compile, but for numbers
                            "LITERAL",
                        "ELSE",
                            // Leave it on the stack
                            "NOP",
                        "THEN",
                    "ELSE",
                        /* Not a number and not a word we know */
                        "(HALT)",   // <-- Should be "ABORT",
                    "THEN",
                "THEN",
            "ELSE",
                /* No name can be found only spaces, need new input */
                "DROP", "DROP",
            "THEN",
        "THEN",
    "AGAIN", ";",
    NULL
};

/*
 * Mini version of the outer interpreter in C to get the outer interpreter
 * (written in Forth) compiled.  Once that is compiled it is used for any
 * further compilation.
 */
static void compile_outer_interpreter(T_Context* ctx)
{
    set_code(ctx, "INTERPRET ");
    instr_colon(ctx);
    uint32_t i;
    for (i = 0; outer_interpreter_code[i] != NULL; ++i) {
        execution_token_t xt = dict_find(ctx, outer_interpreter_code[i]);
        if (xt != NULL_TOKEN) {
            T_DictHeader* header = (T_DictHeader*)(ctx->dataspace + xt);
            if (header->flags & F_IMMEDIATE) {
                push(ctx->dataspace, &(ctx->data_stack), xt);
                instr_execute(ctx);
            } else {
                push(ctx->dataspace, &(ctx->data_stack), xt);
                instr_compile(ctx);
            }
        } else {
            /* Must be a number, read the value and compile it */
            uint32_t v = strtol(outer_interpreter_code[i], NULL, 10);
            push(ctx->dataspace, &(ctx->data_stack), v);
            instr_literal(ctx);
        }
    }
}

/* --------------------------------------------------------------------*/
/* --------------------------------------------------------------------*/

static void usage(void)
{
    printf("Usage:\n" "ff [options] source\n");
    printf("   -d  -- decompile dictionary\n");
    printf("   -i  -- show info and stats\n");

}

#define DO_UNIT_TEST (1 << 0)
#define DO_INFO      (1 << 1)
#define DO_DECOMPILE (1 << 2)

static uint32_t parse_options(int argc, char** argv)
{
    uint32_t actions = 0;
    int c;
    while((c = getopt(argc, argv, "hitd")) != EOF) {
        switch (c)
        {
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
            break;
        case 'i':
            actions |= DO_INFO;
            break;
        case 'd':
            actions |= DO_DECOMPILE;
            break;
        default:
            usage();
            exit(EXIT_SUCCESS);
            break;
        }
    }
    return actions;
}

/* --------------------------------------------------------------------*/

void info(T_Context* ctx)
{
    T_DictHeader* header;
    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);

    printf("Memory size: %d\n", MEMORY_SIZE);
    printf("Header size: %ld\n", sizeof(T_DictHeader));
    printf("Maximum length of name of a word %d\n", MAX_DE_NAME_LENGTH);
    printf("Data space used: %d out of %d\n", here, ctx->top);
    printf("Words available: ");

    dataspace_index_t h = ctx->dict_head;
    do {
        header = (T_DictHeader*)(ctx->dataspace+h);
        printf("%s ", header->name);
        h = header->previous;
    } while (h != 0);
}

/* --------------------------------------------------------------------*/

static void test(T_Context* ctx)
{
    execution_token_t xt = dict_find(ctx, "INTERPRET");
    push(ctx->dataspace, &(ctx->data_stack), xt);
    ctx->ip = instr_execute(ctx);
    inner_interpreter(ctx);
}


int main(int argc, char** argv)
{
    T_Context context;
    uint32_t actions = parse_options(argc, argv);

    context.dataspace = malloc(MEMORY_SIZE);

    fs_reset(&context);
    fs_init_dictionary(&context);
    compile_outer_interpreter(&context);
    if (actions == 0) {
#if 0
        set_code(&context, ": TEST 100 100 + . ; ");
//        set_code(&context, ": ff dup dup + + ; 1 ff");
//        set_code(&context, ": ff dup dup + + ;");
#endif
        test(&context);
        decompile_dictionary(&context);
    } else {
        if (actions & DO_INFO) { info(&context); }
        if (actions & DO_DECOMPILE) { decompile_dictionary(&context); }
    }
    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
