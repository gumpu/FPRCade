#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>

#include "ff.h"

/* --------------------------------------------------------------------*/

static void FF_Exit(int line)
{
    printf("Assert failed at line %d\n", line);
    exit(EXIT_FAILURE);
}

void CF_RealAssert(bool value, int line)
{
    if (value) { return; } else { FF_Exit(line); }
}

/* --------------------------------------------------------------------*/

static cell_type pop(address_unit_type* dataspace, T_Stack* s)
{
    cell_type v;
    if (s->where == s->bottom) {
        // TODO Handle with exception
        CF_Assert(false);
        v = 0;
    } else {
        s->where -= FF_CELL_SIZE;
        address_unit_type* cell = dataspace + s->where;
        v = *((cell_type*)(cell));
    }
    return v;
}

static void push(address_unit_type* dataspace, T_Stack* s, cell_type v)
{
    address_unit_type* cell = dataspace + s->where;
    *((cell_type*)(cell)) = v;
    s->where += FF_CELL_SIZE;
    if (s->where == s->top) {
        // TODO Handle with exception
        CF_Assert(false);
    }
}

/* Fetch a character from a possibly unaligned address */
static char fetch_char(address_unit_type* dataspace, address_type i)
{
    char v = (char)dataspace[i];
    return v;
}

/* Fetch singed 8 bit number from a possibly unaligned address */
static int8_t fetch8bits(address_unit_type* dataspace, address_type i)
{
    int8_t v = dataspace[i];
    return v;
}

/* Fetch unsinged 8 bit number from a possibly unaligned address */
static uint8_t fetch8ubits(address_unit_type* dataspace, address_type i)
{
    uint8_t v = dataspace[i];
    return v;
}

/* Fetch a cell value (unsigned) from a possibly unaligned address */
static cell_type fetch16ubits(address_unit_type* dataspace, address_type i)
{
    /* Little Endian */
    cell_type v = dataspace[i+1];
    v = (v << 8U);
    v = v | dataspace[i];
    return v;
}

static void store16ubits(address_unit_type* dataspace, address_type i, cell_type v)
{
    /* Little Endian */
    dataspace[i] = v & 0xFF;
    v = v >> 8U;
    dataspace[i+1] = v;
}

static void store8ubits(address_unit_type* dataspace, address_type i, uint8_t v)
{
    dataspace[i] = v;
}

static void store8bits(address_unit_type* dataspace, address_type i, int8_t v)
{
    dataspace[i] = v;
}

/*
 * Given an address make it 16 bit aligned by adding one if necessary.
 */
static address_type allign(address_type address)
{
    address_type n = address;
    n += (n & 0x1);
    return n;
}

/*
 * Initialize a stack.  Claims a region of the memory
 * starting at the byte pointed to by 'where'.
 *
 * Stacks grow from the top down
 */
static address_type init_stack(T_Stack* s, address_type where, uint16_t size)
{
    s->top = where;
    where -= FF_CELL_SIZE*size;
    s->bottom = where;
    s->where = where;
    where -= FF_CELL_SIZE;
    return where;
}

/*
 *
 */
static void add_dict_entry(T_Context* ctx, char* name)
{
    T_DictHeader* header;
    /* Soon to be the previous head */
    address_type current_head = ctx->dict_head;
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type new_head = here;
    header = (T_DictHeader*)(ctx->dataspace+here);
    int8_t n = strlen(name);

    header->previous = current_head;
    header->flags = 0U;
    header->code_field = 0U;
    header->does_code = 0U;
    /* Copy name */
    header->name_length = n;
    for (int i = 0; i < n; ++i) { header->name[i] = name[i]; }

    here = here + DE_SIZE_MIN + n;
    here = allign(here);
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    /* Make it the new head */
    ctx->dict_head = new_head;
}

/* --------------------------------------------------------------------*/

/**
 * Paste code into the input stream buffer and
 * set all relevant variables.
 */

static void paste_code(T_Context* ctx, char* code)
{
    uint32_t code_length = strlen(code);

    index_type i;
    index_type n;
    address_type input_buffer;

    if (code_length < FF_INPUT_BUFFER_SIZE) {
        n = code_length;
        input_buffer = FF_LOC_INPUT_BUFFER;

        for (i = 0; i < n; ++i) {
            store8ubits(ctx->dataspace, input_buffer + i, code[i]);
        }
        store8ubits(ctx->dataspace, input_buffer + i, 0);
        store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_INDEX, 0);
        store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_COUNT, n);
    } else {
        exit(100);
    }
}


static void ff_reset(T_Context* ctx)
{
    ctx->run = true;

    address_type where = 0xFFFF - 1;
    where = init_stack(&(ctx->return_stack), where, FF_RETURN_STACK_SIZE);
    where = init_stack(&(ctx->control_stack), where, FF_CONTROL_STACK_SIZE);
    where = init_stack(&(ctx->data_stack), where, FF_DATA_STACK_SIZE);
    ctx->top = where;
    ctx->dict_head = 0; /* There is dictionary yet */
    store8ubits(ctx->dataspace,  FF_LOC_INPUT_BUFFER, 0);
    store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_INDEX, 0);
    store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_COUNT, 0);
    store16ubits(ctx->dataspace, FF_LOC_HERE, FF_SYSTEM_VARIABLES_END);
    store16ubits(ctx->dataspace, FF_SYSTEM_VARIABLES_END, 0);
}

/* --------------------------------------------------------------------*/

/**
 * Implementation of WORD
 * ( char -- )
 */
instruction_pointer_type instr_word(T_Context* ctx, address_type word)
{
    index_type i;
    index_type n;
    address_type word_buffer;
    address_type input_buffer;
    char c;
    char delimitor = (char)pop(ctx->dataspace, &(ctx->data_stack));

    i = fetch16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_INDEX);
    n = fetch16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_COUNT);
    word_buffer = FF_LOC_WORD_BUFFER;
    input_buffer = FF_LOC_INPUT_BUFFER;

    if (i == n) {
        /* There is no data to parse, create
         * a zero length string */
        store8ubits(ctx->dataspace, word_buffer, 0);
    } else {
        /* Skip initial delimitor */
        c = fetch_char(ctx->dataspace, input_buffer + i);
        for (; (c == delimitor) && (i < n);) {
            i++;
            c = fetch_char(ctx->dataspace, input_buffer + i);
        }
        if (i == n) {
            store8ubits(ctx->dataspace, word_buffer, 0);
            store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_INDEX, i);
        } else {
            int8_t l = 0;
            for (; !(c == delimitor) && (i < n);) {
                store8ubits(ctx->dataspace, word_buffer + l + 1, c);
                l++;
                i++;
                c = fetch_char(ctx->dataspace, input_buffer + i);
            }
            if (i == n) {
                store8ubits(ctx->dataspace, word_buffer + l + 1, 0);
            } else if (c == delimitor) {
                store8ubits(ctx->dataspace, word_buffer + l + 1, c);
            } else {
                /* Should be either one of the two conditions
                 * in the previous for()
                 */
                CF_Assert(false);
            }
            store8ubits(ctx->dataspace, FF_LOC_WORD_BUFFER, l);
            store16ubits(ctx->dataspace, FF_LOC_INPUT_BUFFER_INDEX, i);
        }
    }
    /* Push address of word found on stack */
    push(ctx->dataspace, &(ctx->data_stack), FF_LOC_WORD_BUFFER);

    return 2;
}

instruction_pointer_type instr_create(T_Context* C, address_type word)
{
    // TODO
    return 0;
}

instruction_pointer_type instr_create_rt(T_Context* C, address_type word)
{
    // TODO
    return 0;
}

/**
 * TODO: turn into EXPECT  which read n characters into
 * a buffer given by an address.
 */
instruction_pointer_type instr_paccept(T_Context* ctx)
{
    char buffer[256];
    fgets(buffer, 255, stdin);
    paste_code(ctx, buffer);
    return (ctx->ip + 2);
}

instruction_pointer_type instr_execute(T_Context* ctx, address_type word)
{
    // TODO
    return (ctx->ip + 2);
}

/**
 * JUMP <new location>
 *
 * Needed to implement loops and branches.
 */
instruction_pointer_type instr_jump(T_Context* ctx, address_type word)
{
    address_type  address = ctx->dataspace[ctx->ip + 2];
    return address;
}


/**
 * ALLOT  (n -- )  add n bytes to the parameter field of the most
 * recently defined word.
 */
instruction_pointer_type instr_allot(T_Context* ctx, address_type word)
{
    cell_type n = pop(ctx->dataspace, &(ctx->data_stack));
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    here = here + n;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);
    return (ctx->ip + 2);
}

instruction_pointer_type instr_illegal(T_Context* ctx, address_type word)
{
    CF_Assert(false);
    return 0;
}

/* --------------------------------------------------------------------*/

static void usage(void)
{
    printf("Usage:\n" "ff [options] source\n");
    printf("   -d  -- decompile dictionary\n");
    printf("   -i  -- show info and stats\n");

}

#define DO_INFO      (1)
#define DO_DECOMPILE (1<<1)

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

#define MAX_INSTRUCTION_COUNT (256)
function_type instruction_table[MAX_INSTRUCTION_COUNT];

typedef enum {
    eOP_SP = 0,
    eOP_CREATE,
    eOP_CREATE_RT,
    eOP_WORD,
    eOP_ALLOT,
    eOP_ENTER,
    eOP_EXIT,
    eOP_EXECUTE,
    eOP_JUMP,
    eOP_PUSH,
    eOP_DROP,
    /* This always needs to be the last entry */
    eOP_MAX_OP_CODE
} FF_OpCodes;

/* --------------------------------------------------------------------*/

static void fill_instruction_table(void)
{
    for (int i = 0; i < MAX_INSTRUCTION_COUNT; ++i) {
        instruction_table[i] = instr_illegal;
    }
    instruction_table[eOP_WORD] = instr_word;
    instruction_table[eOP_CREATE] = instr_create;
    instruction_table[eOP_CREATE_RT] = instr_create_rt;
    instruction_table[eOP_EXECUTE] = instr_execute;
    instruction_table[eOP_JUMP] = instr_jump;
    instruction_table[eOP_ALLOT] = instr_allot;
}

/* --------------------------------------------------------------------*/

void info(T_Context* ctx)
{
    T_DictHeader* header;
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);

    printf("Memory size: %d\n", FF_MEMORY_SIZE);
    printf("Header size: %ld (without name)\n", sizeof(T_DictHeader));
    printf("Maximum length of name of a word %d\n", FF_MAX_DE_NAME_LENGTH);
    printf("Data space used: %d out of %d\n", here, ctx->top);
    printf("Cell size %d\n", FF_CELL_SIZE);
    printf("Data stack size:    %d\n", FF_DATA_STACK_SIZE);
    printf("Control stack size: %d\n", FF_CONTROL_STACK_SIZE);
    printf("Return stack size:  %d\n", FF_RETURN_STACK_SIZE);
    printf("\nNumber of instructions: %d\n", eOP_MAX_OP_CODE);

    printf("Words available:\n");

    address_type h = ctx->dict_head;
    do {
        word_flag_type flags;
        header = (T_DictHeader*)(ctx->dataspace+h);
        printf("previous: %u\n", header->previous);
        printf("code_field: %u\n", header->code_field);
        printf("flags:");
        flags = header->flags;
        if (flags & EF_IMMEDIATE) { printf(" immediate"); }
        if (flags & EF_MACHINE_CODE) { printf(" machine_code"); }
        if (flags & EF_COLON_DEF_ONLY) { printf(" colon_def_only"); }
        printf("\n");
        printf("name: (%d)", header->name_length);
        for (int i = 0; i < header->name_length; ++i) {
            printf("%c", header->name[i]);
        }
        printf("\n");
        h = header->previous;
    } while (h != 0);
}

static void tst_dump_word_buffer(T_Context* ctx)
{
    uint32_t n = fetch8ubits(ctx->dataspace, FF_LOC_WORD_BUFFER);
    printf("Length: %d  chars: \"", n);
    for (uint32_t i = 0; i < n; ++i) {
        char c = fetch_char(ctx->dataspace, FF_LOC_WORD_BUFFER + i + 1);
        printf("(%d) %c", c, c);
    }
    printf("\"\n");
}


/* --------------------------------------------------------------------*/

/**
 * Executes instruction pointed to by context->ip
 * This is what makes FORTH run.
 */
void inner_interpreter(T_Context* context)
{
    instruction_type instruction;
    while(context->run) {
        /* Fetch instruction */
        instruction = context->dataspace[context->ip];
        /* The instruction itself will fetch any additional data needed and
         * advance the instruction pointer and return the new pointer */
        context->ip = instruction_table[instruction](context, 0);
    }
}

/* --------------------------------------------------------------------*/

int main(int argc, char** argv)
{
    T_Context context;
    uint32_t actions = parse_options(argc, argv);

    context.dataspace = malloc(FF_MEMORY_SIZE);

    fill_instruction_table();

    ff_reset(&context);

    if (actions == 0) {

        paste_code(&context, "  WORD1   WORD2 A B CC ");

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        push(context.dataspace, &(context.data_stack), ' ');
        (void)instr_word(&context, 0);
        tst_dump_word_buffer(&(context));

        add_dict_entry(&(context), "SPACE");
        add_dict_entry(&(context), "HERE");
        add_dict_entry(&(context), "WORD");

        info(&context);
    } else {
        if (actions & DO_INFO) { info(&context); }
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
