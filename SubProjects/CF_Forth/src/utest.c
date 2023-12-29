#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include "fcore.h"
#include "instructions.h"

/* --------------------------------------------------------------------*/

/*
 *
 *
 */
static void test_parse_name(T_Context* ctx)
{
    set_code(ctx, "  WORD1   WURD_2 WORD__3");
    execution_token_t parse_name = dict_find(ctx, "PARSE-NAME");

    CF_Assert(parse_name != NULL_TOKEN);
    push(ctx->dataspace, &(ctx->data_stack), parse_name);
    instr_execute(ctx);
    cell_t u = pop(ctx->dataspace, &(ctx->data_stack));
    CF_Assert(u == 5);  /* WORD1 is 5 long */
    cell_t c_addr = pop(ctx->dataspace, &(ctx->data_stack));
    char c = fetch8bits(ctx->dataspace, c_addr);
    CF_Assert(c == 'W');
    push(ctx->dataspace, &(ctx->data_stack), parse_name);
    instr_execute(ctx);
    u = pop(ctx->dataspace, &(ctx->data_stack));
    CF_Assert(u == 6);  /* WURD_2 is 6 long */
    c_addr = pop(ctx->dataspace, &(ctx->data_stack));
    c = fetch8bits(ctx->dataspace, c_addr + 1);
    CF_Assert(c == 'U');

    push(ctx->dataspace, &(ctx->data_stack), parse_name);
    instr_execute(ctx);
    u = pop(ctx->dataspace, &(ctx->data_stack));
    CF_Assert(u == 7);  /* WORD__3 is 7 long */
    (void)pop(ctx->dataspace, &(ctx->data_stack));

}

static void test_remain(T_Context* ctx)
{
    execution_token_t xt_remain = dict_find(ctx, "(REMAIN)");

    /* Pretend code was added */
    set_code(ctx, "100 DUP");

    push(ctx->dataspace, &(ctx->data_stack), xt_remain);
    instr_execute(ctx);

    cell_t chars_remaining = pop(ctx->dataspace, &(ctx->data_stack));
    CF_Assert(chars_remaining == 7);

}

/* Test PARSE-NAME in combination with FIND2
 *
 * DUP should be found and is marked as being not an immediate word
 */
static void test_parsing_1(T_Context* ctx)
{
    execution_token_t xt_pn = dict_find(ctx, "PARSE-NAME");
    execution_token_t xt_f2 = dict_find(ctx, "FIND2");

    set_code(ctx, " DUP ");

    /* Get pointer to and length of first unparsed word in the buffer */
    push(ctx->dataspace, &(ctx->data_stack), xt_pn);
    instr_execute(ctx);
    /* Now use FINS2 to find it */
    push(ctx->dataspace, &(ctx->data_stack), xt_f2);
    instr_execute(ctx);

    cell_t kind = pop(ctx->dataspace, &(ctx->data_stack));
    CF_Assert(kind == 0xFFFF); /* -1 == IMMEDIATE flag is not set */
    execution_token_t xt = pop(ctx->dataspace, &(ctx->data_stack));
    /* Is also should be a valid token */
    CF_Assert(xt != NULL_TOKEN);

}

typedef struct {
    char* number;
    signed_cell_t zoll;
    bool wrong;
} T_NumberTestCase;

static void test_number_parsing(T_Context* ctx)
{
    signed_cell_t value;
    T_NumberTestCase cases[] = {
        {"p", 0, true},
        {"#133", 133, false},
        {"$A000", 0xA000, false},
        {"-125", -125, false},
        {"-1-25", 0, true},
        {"--125", 0, true},
        {"125", 125, false},
        {"12A", 0, true},
        {"%1110", 14, false},
        {NULL, 0, false} };

    execution_token_t xt_pn = dict_find(ctx, "PARSE-NAME");
    execution_token_t xt_num = dict_find(ctx, "(NUMBER)");
    uint32_t i = 0;
    while (cases[i].number != NULL) {
        set_code(ctx, cases[i].number);
        push(ctx->dataspace, &(ctx->data_stack), xt_pn);
        instr_execute(ctx);
        push(ctx->dataspace, &(ctx->data_stack), xt_num);
        instr_execute(ctx);
        if (cases[i].wrong) {
            value = (signed_cell_t)pop(ctx->dataspace, &(ctx->data_stack));
            CF_Assert(value == 0);
            (void)pop(ctx->dataspace, &(ctx->data_stack));
        } else {
            value = (signed_cell_t)pop(ctx->dataspace, &(ctx->data_stack));
            CF_Assert(value == 1);
            value = (signed_cell_t)pop(ctx->dataspace, &(ctx->data_stack));
            CF_Assert(value == cases[i].zoll);
        }
        ++i;
    }
}


static void test_over(T_Context* ctx)
{

}

static void test_find2(T_Context* ctx)
{
    execution_token_t xt = dict_find2(ctx, "DUP xxx", 3);
    CF_Assert(xt != NULL_TOKEN);
}

static void test_find(T_Context* ctx)
{
    execution_token_t xt = dict_find(ctx, "DOESNOTEXIST");
    CF_Assert(xt == NULL_TOKEN);
    xt = dict_find(ctx, "(LDN)");
    CF_Assert(xt != NULL_TOKEN);
    T_DictHeader* header = dict_get_entry(ctx, xt);
    CF_Assert(strlen("(LDN)") == header->name_length);
}

static void unit_test(T_Context* ctx)
{
    test_over(ctx);
    test_remain(ctx);
    test_find(ctx);
    test_find2(ctx);
    test_parse_name(ctx);
    test_parsing_1(ctx);
    test_number_parsing(ctx);
}

static uint32_t parse_options(int argc, char** argv)
{
    return 0U;
}

/* --------------------------------------------------------------------*/

int main(int argc, char** argv)
{
    T_Context context;
    /* uint32_t actions */ (void) parse_options(argc, argv);

    context.dataspace = malloc(MEMORY_SIZE);

    fs_reset(&context);
    fs_init_dictionary(&context);
    unit_test(&context);

    return EXIT_SUCCESS;
}

