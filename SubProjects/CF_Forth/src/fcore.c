#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fcore.h"
#include "instructions.h"

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

cell_t pop(address_unit_t* dataspace, T_Stack* s)
{
    cell_t v;
    if (s->where == s->bottom) {
        // TODO Handle with exception
        CF_Assert(false);
        v = 0;
    } else {
        s->where -= CELL_SIZE;
        address_unit_t* cell = dataspace + s->where;
        v = *((cell_t*)(cell));
    }
    return v;
}

void push(address_unit_t* dataspace, T_Stack* s, cell_t v)
{
    address_unit_t* cell = dataspace + s->where;
    *((cell_t*)(cell)) = v;
    s->where += CELL_SIZE;
    if (s->where == s->top) {
        // TODO Handle with exception
        CF_Assert(false);
    }
}

uint8_t fetch8bits(address_unit_t* dataspace, dataspace_index_t i)
{
    uint8_t v = dataspace[i];
    return v;
}

/* Fetch a cell value from a possibly unaligned address */
cell_t fetch16bits(address_unit_t* dataspace, dataspace_index_t i)
{
    /* Little Endian */
    cell_t v = dataspace[i+1];
    v = (v << 8U);
    v = v | dataspace[i];
    return v;
}

void store16bits(address_unit_t* dataspace, dataspace_index_t i, cell_t v)
{
    /* Little Endian */
    dataspace[i] = v & 0xFF;
    v = v >> 8U;
    dataspace[i+1] = v;
}

void store8bits(address_unit_t* dataspace, dataspace_index_t i, uint8_t v)
{
    dataspace[i] = v & 0xFF;
}


/*
 * Initialize a stack.  Claims a region of the memory
 * starting at the byte pointed to by 'where'.
 *
 * Stacks grow from the top down
 */
dataspace_index_t init_stack(
        T_Stack* s, dataspace_index_t where, uint16_t size)
{
    s->top = where;
    where -= CELL_SIZE*size;
    s->bottom = where;
    s->where = where;
    where -= CELL_SIZE;
    return where;
}

void fs_reset(T_Context* ctx)
{
    ctx->run = true;

    dataspace_index_t where = 0xFFFF - 1;
    where = init_stack(&(ctx->return_stack), where, 20);
    where = init_stack(&(ctx->control_stack), where, 20);
    where = init_stack(&(ctx->data_stack), where, 20);
    ctx->top = where;

    /* Address of the instruction to be executed after warm reset. */
    ctx->dataspace[0] = 0;
    ctx->dataspace[0] = 0;

    ctx->ip = 0;

    /* Dictionary */
    ctx->dict_head = 0;
    ctx->dict_new  = 0;

    /* Default variables */
    ctx->dataspace[LOC_INPUT_BUFFER] = 0;
    /* Where the parse area starts */
    store16bits(ctx->dataspace, LOC_INPUT_BUFFER_INDEX, 0);
    /* Total number of characters in the input buffer */
    store16bits(ctx->dataspace, LOC_INPUT_BUFFER_COUNT, 0);
    store16bits(ctx->dataspace, LOC_WHERE, LOC_INPUT_BUFFER_COUNT + CELL_SIZE);
}

/* --------------------------------------------------------------------*/

/*
 * Executes instructions pointed to by context->ip
 */
void inner_interpreter(T_Context* context)
{
    instruction_t instruction;
    while(context->run) {
        /* Fetch instruction */
        instruction = context->dataspace[context->ip];
        /* Instruction will fetch any additional data needed
         * and advance the instruction pointer */
        context->ip = semantic_table[instruction](context);
    }
}

/* Compute have many bytes it is from the start of the header
 * to the first byte of the code
 */

uint16_t dict_get_code_offset(T_DictHeader* header)
{
    uint16_t offset;
    if (header->flags & F_COLONDEF) {
        offset = sizeof(T_DictHeader) + header->name_length;
        offset = allign(offset);
    } else {
        offset = 0;
    }
    return offset;
}

/* Given an execution token give a point to the corresponding
 * entry in the dictionary.
 */
T_DictHeader* dict_get_entry(T_Context* ctx, execution_token_t xt)
{
    dataspace_index_t here = xt;
    T_DictHeader* header = (T_DictHeader*)(ctx->dataspace+here);
    return header;
}

execution_token_t dict_find(T_Context* ctx, char* name)
{
    T_DictHeader* header;
    execution_token_t token = NULL_TOKEN;
    dataspace_index_t here = ctx->dict_head;

    do {
        header = (T_DictHeader*)(ctx->dataspace+here);
        if (strcasecmp(header->name, name) == 0) {
            token = here;
            break;
        }
        here = header->previous;
    } while (here != 0);

    return token;
}

execution_token_t dict_find2(T_Context* ctx, char* string, uint8_t length)
{
    T_DictHeader* header;
    execution_token_t token = NULL_TOKEN;
    dataspace_index_t here = ctx->dict_head;

    do {
        header = (T_DictHeader*)(ctx->dataspace+here);
        if (header->name_length == length) {
            if (strncasecmp(header->name, string, length) == 0) {
                token = here;
                break;
            }
        }
        here = header->previous;
    } while (here != 0);

    return token;

}

/* Initialize the head and return the total size */
uint16_t init_header(
        T_DictHeader* header,
        dataspace_index_t previous_word,
        char* name,
        word_flag_t flags)
{
    header->previous = previous_word;
    header->flags = flags;

    uint8_t n = strlen(name);
    CF_Assert(n < MAX_DE_NAME_LENGTH);
    header->name_length = n;
    strcpy(header->name, name);
    /* Also count the '\0' that was added */
    n++;
    return (n + sizeof(T_DictHeader));
}


dataspace_index_t add_variable(
        T_Context* ctx,
        dataspace_index_t previous_word,
        dataspace_index_t location,
        char* name
        )
{
    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    dataspace_index_t this_word = here;
    T_DictHeader* header = (T_DictHeader*)(ctx->dataspace+here);
    uint16_t n = init_header(header, previous_word, name, F_COLONDEF);
    /* Adds
     * LDN <location>
     * LEAVE
     */
    store8bits(ctx->dataspace, here + n, OPCODE_LDN);
    ++n;
    store16bits(ctx->dataspace, here + n, location);
    n += 2;
    store8bits(ctx->dataspace, here + n, OPCODE_LEAVE_COLON);
    ++n;

    here = here + n;
    CF_Assert(here < ctx->top);
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return this_word;
}

/* Given an address make it 16 bit aligned by adding
 * one if necessary.
 */
dataspace_index_t allign(dataspace_index_t address)
{
    dataspace_index_t n = address;
    n += (n & 0x1);
    return n;
}

dataspace_index_t add_word(
        T_Context* ctx,
        dataspace_index_t previous_word,
        instruction_t instruction, char* name,
        word_flag_t flags)
{
    dataspace_index_t here = fetch16bits(ctx->dataspace, LOC_WHERE);
    dataspace_index_t this_word = here;
    T_DictHeader* header = (T_DictHeader*)(ctx->dataspace + here);

    uint16_t n = init_header(header, previous_word, name, flags);
    /* Link to the instruction */
    header->semantics = instruction;

    here = here + n;
    CF_Assert(here < ctx->top);
    store16bits(ctx->dataspace, LOC_WHERE, here);
    return this_word;
}

/* Fill the dictionary with the standard words */
void fs_init_dictionary(T_Context* ctx)
{
    dataspace_index_t prev = 0;

    prev = add_word(ctx, prev, OPCODE_HALT,    "(HALT)", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_LDN,     "(LDN)", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_FETCH,   "@", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_STORE,   "!", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_DOT,     ".", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_HERE,    "HERE", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_DUP,     "DUP", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_DROP,    "DROP", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_EQUAL,   "=", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_EMIT,    "EMIT", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_BL,      "BL", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_NOP,     "NOP", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_JMP,     "JMP", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_JMP_FALSE, "JMP_FALSE", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_BEGIN, "BEGIN", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_AGAIN, "AGAIN", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_IF,    "IF", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_ELSE,  "ELSE", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_THEN,  "THEN", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_COLON, ":", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_SEMICOLON, ";", F_INSTRUCTION|F_IMMEDIATE);
    prev = add_word(ctx, prev, OPCODE_PARSE_NAME,  "PARSE-NAME", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_PREMAIN, "(REMAIN)", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_PACCEPT, "(ACCEPT)", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_CR,      "CR", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_COMPILE, "COMPILE,", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_EXECUTE, "EXECUTE", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_FIND2,   "FIND2", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_PNUM,    "(NUMBER)", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_OVER,    "OVER", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_SWAP,    "SWAP", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_ADD,     "+", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_DECOMPILE, "DECOMPILE", F_INSTRUCTION);
    prev = add_word(ctx, prev, OPCODE_LITERAL,  "LITERAL", F_INSTRUCTION);

    prev = add_variable(ctx, prev, LOC_STATE, "STATE");
    ctx->dict_head = prev;
}

void set_code(T_Context* ctx, char* code)
{
    char* dest = (char*)(ctx->dataspace + LOC_INPUT_BUFFER);
    uint16_t n = strlen(code);
    CF_Assert(n < 256);
    strcpy(dest, code);
    store16bits(ctx->dataspace, LOC_INPUT_BUFFER_COUNT, n);
    store8bits(ctx->dataspace, LOC_INPUT_BUFFER_INDEX, 0);
}

/* ------------------------ end of file -------------------------------*/
