/** vi: spell spl=en
 *
 * This is an implementation of FORTH based on the FORTH-79 standard.
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include "interpret.h"
#include "ff.h"

/* --------------------------------------------------------------------*/

#define MAX_INSTRUCTION_COUNT (256)
function_type instruction_table[MAX_INSTRUCTION_COUNT];

/* --------------------------------------------------------------------*/

static void FF_Exit(int line)
{
    printf("Assert failed at line %d\n", line);
    exit(EXIT_FAILURE);
}

void CF_RealAssert(bool value, int line)
{
    if (value) {
        return;
    } else {
        FF_Exit(line);
    }
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
static address_type align(address_type address)
{
    address_type n = address;
    n += (n & 0x1);
    return n;
}

/**
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

static bool compare_packed_string(
        T_PackedString* ps1,
        T_PackedString* ps2)
{
    bool result;
    if (ps1->count != ps2->count) {
        result = false;
    } else {
        if (memcmp(ps1->s, ps2->s, ps1->count) == 0) {
            result = true;
        } else {
            result = false;
        }
    }
    return result;
}

static void copy_packed_string(
        T_PackedString* source,
        T_PackedString* dest)
{
    dest->count = source->count;
    memcpy(dest->s, source->s, dest->count);
}

static void clone_c_string(char* string, T_PackedString* dest)
{
    dest->count = strlen(string);
    memcpy(dest->s, string, dest->count);
}


/**
 * Add a new entry to the dictionary with the name given.
 */
static T_DictHeader* add_dict_entry(T_Context* ctx, T_PackedString* name)
{
    T_DictHeader* header;
    /* Soon to be the previous head */
    address_type current_head = ctx->dict_head;
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type new_head = here;
    header = (T_DictHeader*)(ctx->dataspace+here);
    int8_t n = name->count;

    header->previous = current_head;
    header->flags = 0U;
    header->code_field = 0U;
    header->does_code = 0U;
    /* Copy name */
    copy_packed_string(name, &(header->name));

    here = here + DE_SIZE_MIN + n;
    here = align(here);
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    /* Make it the new head */
    ctx->dict_head = new_head;

    return header;
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
 * Given an opcode find the first dictionary entry that uses this
 * opcode.  This is used to find words such as BEGIN, END, IF, etc
 * during compilation.
 *
 * NOTE: This is a place where optimization is possible.
 */
static address_type find_via_opcode(T_Context* ctx, T_OpCode opcode)
{
    address_type h = ctx->dict_head;
    address_type word_found;
    T_DictHeader* header;
    bool found;

    do {
        /* Assume we found the word, until proven otherwise */
        word_found = h;
        header = (T_DictHeader*)(ctx->dataspace+h);

        /* Check if they match */
        found = (header->code_field == opcode);

        /* Next entry in the dictionary */
        h = header->previous;
    } while ((h != 0) && (!found));

    /* This function is used internally, and should always succeed */
    CF_Assert(found);

    return word_found;
}

/* --------------------------------------------------------------------*/

/**
 * BEGIN
 *
 * Compile time behaviour for BEGIN.  Stores the current value
 * of HERE on the control stack.  The data will be later used by
 * one of the loop constructs to create a JUMP...  to this address.
 */
static instruction_pointer_type instr_begin(
        T_Context* ctx, address_type dict_entry)
{
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    push(ctx->dataspace, &(ctx->control_stack), here);
    return (ctx->ip + 2);
}

/**
 * Implementation of WORD
 *
 * ( char --   )
 *
 * Scan through the input stream, starting at >IN, looking
 * for the delimiter, skip the delimiters, then copy
 * all characters upon till the next delimiter to the
 * word buffer.
 *
 */
static instruction_pointer_type instr_word(
        T_Context* ctx, address_type dict_entry)
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
    /* Push address of the copy of the word that was found on stack */
    push(ctx->dataspace, &(ctx->data_stack), FF_LOC_WORD_BUFFER);

    return (ctx->ip + 2);
}

/**
 * FIND (  --  addr )
 *
 * Fetch the next word from the input stream and look it up
 * in the dictionary.
 * Return the dictionary entry address when it was found, and 0 otherwise.
 *
 * To see if the 0 is due to the word not being in the dictionary or
 * due to the input stream being empty the extracted word has to
 * be checked.
 *
 * The word extracted will be in the word buffer
 */
static instruction_pointer_type instr_find(
        T_Context* ctx, address_type dict_entry)
{
    T_DictHeader* header;
    T_PackedString* word_to_find;
    address_type h = ctx->dict_head;

    push(ctx->dataspace, &(ctx->data_stack), ' ');
    instr_word(ctx, 0);
    address_type word = pop(ctx->dataspace, &(ctx->data_stack));
    word_to_find = (T_PackedString*)(&(ctx->dataspace[word]));

    address_type word_found;
    bool found;

    if (word_to_find->count == 0) {
        /* There was nothing in the input stream */
        found = false;
    } else {
        do {
            /* Assume we found the word, until proven otherwise */
            word_found = h;
            header = (T_DictHeader*)(ctx->dataspace+h);

            /* Check if they match */
            found = compare_packed_string(word_to_find, &(header->name));

            /* Next entry in the dictionary */
            h = header->previous;
        } while ((h != 0) && (!found));
    }

    if (found) {
        push(ctx->dataspace, &(ctx->data_stack), word_found);
    } else {
        push(ctx->dataspace, &(ctx->data_stack), 0);
    }

    return (ctx->ip + 2);
}

/**
 * CREATE <name>
 *
 * Read the next word in the input stream and use its name
 * to create a new entry in the dictionary.
 */

static instruction_pointer_type instr_create(
        T_Context* ctx, address_type dict_entry)
{
    T_PackedString* word_found;
    push(ctx->dataspace, &(ctx->data_stack), ' ');
    instr_word(ctx, dict_entry);
    word_found = (T_PackedString*)(&(ctx->dataspace[FF_LOC_WORD_BUFFER]));
    if (word_found->count > 0) {
        T_DictHeader* header = add_dict_entry(ctx, word_found);
        header->flags |= EF_MACHINE_CODE;
        header->code_field = eOP_CREATE_RT;
    } else {
        // TODO
        // Error and abort
    }

    return (ctx->ip + 2);
}

/**
 * CREATE Run Time
 *
 * Pushes the address of the parameter field of the created word
 * on stack.
 *
 */
static instruction_pointer_type instr_create_rt(
        T_Context* ctx, address_type dict_entry)
{
    // TODO
    T_DictHeader* header = (T_DictHeader*)(&(ctx->dataspace[dict_entry]));
    uint32_t n = header->name.count;
    address_type parameter_field = dict_entry;
    parameter_field += n + DE_SIZE_MIN;
    //
    parameter_field = align(parameter_field);
    push(ctx->dataspace, &(ctx->data_stack), parameter_field);
    return (ctx->ip + 2);
}

/**
 * QUERY read upto 80 characters from the terminal
 *
 *
 * TODO: Also to be used for  EXPECT  which read n characters into
 * a buffer given by an address.
 */
static instruction_pointer_type instr_query(
        T_Context* ctx, address_type dict_entry)
{
    char buffer[81];
    fgets(buffer, 80, stdin);
    paste_code(ctx, buffer);
    return (ctx->ip + 2);
}

/**
 * EXECUTE   ( xt -- )
 */
static instruction_pointer_type instr_execute(
        T_Context* ctx, address_type dict_entry)
{
    // the xt points to the dictionary entry
    address_type word = pop(ctx->dataspace, &(ctx->data_stack));
    T_DictHeader* header = (T_DictHeader*)(&(ctx->dataspace[word]));
    instruction_table[header->code_field](ctx, dict_entry);

    return (ctx->ip + 2);
}

/**
 * JUMP <new location>
 *
 * Needed to implement loops and branches.
 */
static instruction_pointer_type instr_jump(
        T_Context* ctx, address_type dict_entry)
{
    address_type address;

    address = fetch16ubits(ctx->dataspace, ctx->ip + 2);

    return address;
}

/**
 * JUMP_IF_FALSE <new location>
 *
 * (flag -- )
 *
 * Needed to implement loops and branches.
 */

static instruction_pointer_type instr_jump_if_false(
        T_Context* ctx, address_type word)
{
    address_type address;

    cell_type n = pop(ctx->dataspace, &(ctx->data_stack));

    if (n == 0) { /* False */
        address = fetch16ubits(ctx->dataspace, ctx->ip + 2);
    } else { /* True */
        /* Continue at the location after the jump instruction */
        address = ctx->ip + 4;
    }

    return address;
}

/**
 * ALLOT  (n -- )  add n bytes to the parameter field of the most
 * recently defined word.
 */
static instruction_pointer_type instr_allot(
        T_Context* ctx, address_type dict_entry)
{
    cell_type n = pop(ctx->dataspace, &(ctx->data_stack));
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    here = here + n;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);
    return (ctx->ip + 2);
}

/**
 * All unused Opcodes map to this instruction
 */
static instruction_pointer_type instr_illegal(
        T_Context* ctx, address_type dict_entry)
{
    uint16_t code = fetch16ubits(ctx->dataspace, ctx->ip);
    printf("Illegal instruction %d at %d\n", code, ctx->ip);
    CF_Assert(false);
    return 0;
}

/**
 * STATE  ( -- addr )
 *
 * Address of the variable containing the system state,
 * that is, is compiling or non compiling.
 *
 * 0 means non-compiling, any other value means compiling.
 */
static instruction_pointer_type instr_state(
        T_Context* ctx, address_type dict_entry)
{
    push(ctx->dataspace, &(ctx->data_stack), FF_LOC_STATE);
    return (ctx->ip + 2);
}

/**
 * ENTER
 *
 * Push the address of the next instruction onto the return stack
 * Set the IP to the address of the parameter field of the word
 * associated with this ENTER and continue running there.
 */
static instruction_pointer_type instr_enter(
        T_Context* ctx, address_type dict_entry)
{
    push(ctx->dataspace, &(ctx->return_stack), ctx->ip + 2);

    T_DictHeader* header = (T_DictHeader*)(&(ctx->dataspace[dict_entry]));
    uint32_t n = header->name.count;
    address_type parameter_field = dict_entry;
    parameter_field += n + DE_SIZE_MIN;
    parameter_field = align(parameter_field);
    return parameter_field;
}

/**
 * ' <name>
 *
 */

static instruction_pointer_type instr_tick(
        T_Context* ctx, address_type dict_entry)
{
#if 0
    T_PackedString* word_found;
    push(ctx->dataspace, &(ctx->data_stack), ' ');
    instr_word(ctx, dict_entry);
    word_found = (T_PackedString*)(&(ctx->dataspace[FF_LOC_WORD_BUFFER]));

    if (word_found->count > 0) {
        address_type parameter_field = dict_entry;
        parameter_field += n + DE_SIZE_MIN;
        //
        parameter_field = align(parameter_field);
        push(ctx->dataspace, &(ctx->data_stack), parameter_field);
    } else {
        // Error condition
    }
#endif
    return (ctx->ip + 2);
}

/**
 * : <name>
 *
 */
static instruction_pointer_type instr_colon(
        T_Context* ctx, address_type dict_entry)
{
    T_PackedString* word_found;
    push(ctx->dataspace, &(ctx->data_stack), ' ');
    instr_word(ctx, dict_entry);
    word_found = (T_PackedString*)(&(ctx->dataspace[FF_LOC_WORD_BUFFER]));
    if (word_found->count > 0) {
        T_DictHeader* header = add_dict_entry(ctx, word_found);
        header->flags |= EF_MACHINE_CODE;
        header->code_field = eOP_ENTER;
        /* Set state to compiling */
        store16ubits(ctx->dataspace, FF_LOC_STATE, FF_STATE_COMPILING);
    } else {
        // TODO
        // Error and abort
    }

    return (ctx->ip + 2);
}

/**
 * ;
 *
 */
static instruction_pointer_type instr_semicolon(
        T_Context* ctx, address_type dict_entry)
{
    store16ubits(ctx->dataspace, FF_LOC_STATE, FF_STATE_INTERPRETING);
    return (ctx->ip + 2);
}

static int16_t to_digit(char c, uint16_t base)
{
    int16_t d;

    d = (int16_t)c;

    if ((c >= 'a') && (c <= 'z')) { d = 10 + c - 'a'; }
    else if ((c >= 'A') && (c <= 'Z')) { d = 10 + c - 'A'; }
    else { d = d - '0'; }

    if ((d >= base) || (d < 0)) { d = -1; }

    return d;
}

/**
 * ,                                                            "comma"
 *
 * ( n -- )
 *
 * Allot two bytes in the dictionary, storing n there.
 *
 * That is store n in the location pointed to by HERE
 * and increase HERE by 2.
 */
static instruction_pointer_type instr_comma(
        T_Context* ctx, address_type word)
{
    cell_type number = pop(ctx->dataspace, &(ctx->data_stack));
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    store16ubits(ctx->dataspace, here, number);
    here += FF_CELL_SIZE;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    return (ctx->ip + 2);
}

/**
 * .                                                            "dot"
 * (n -- )
 *
 * Display  n converted according to BASE in a free field  format with one
 * trailing blank.  Display only a negative sign.
 *
 */

static instruction_pointer_type instr_dot(T_Context* ctx, address_type word)
{
    signed_cell_type n =
        (signed_cell_type)(pop(ctx->dataspace, &(ctx->data_stack)));

    // TODO  Other bases
    printf("%d ", n);

    return (ctx->ip + 2);
}

/**
 * DROP  (n -- )
 */
static instruction_pointer_type instr_drop(T_Context* ctx, address_type word)
{
    (void)(pop(ctx->dataspace, &(ctx->data_stack)));
    return (ctx->ip + 2);
}

/**
 * (NUMBER)  ( addr -- addr 0 | n 1 )
 *
 * Convert the counted string at address addr to the the number it represents.
 *
 * Support negative numbers,
 * binary numbers;  %1001
 * decimal numbers;  #9
 * hexadecimal numbers;  $AA
 * and chars;   ';'
 *
 * Numbers without a prefix are converted using the base value
 * stored in BASE.   (TODO currently fixed to 10)
 *
 * Returns  addr 0  when the conversion failed
 * Returns  n    1  when the conversion succeeded
 */
static instruction_pointer_type instr_pnumber(T_Context* ctx, address_type word)
{
    cell_type c_addr = pop(ctx->dataspace, &(ctx->data_stack));

    cell_type u = fetch8ubits(ctx->dataspace, c_addr);
    char* string = (char*) (ctx->dataspace + c_addr + 1);

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
        if (negative) { result = 0 - result; }
        push(ctx->dataspace, &(ctx->data_stack), result);
        push(ctx->dataspace, &(ctx->data_stack), 1);
    }

    return (ctx->ip + 2);
}

/**
 * LITERAL
 *
 * If  compiling,  then  compile  the stack value n as  a  16-bit literal,
 * which when later executed, will leave n on the stack.
 */
static instruction_pointer_type instr_literal(T_Context* ctx, address_type word)
{
    cell_type n = pop(ctx->dataspace, &(ctx->data_stack));
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type push_entry = find_via_opcode(ctx, eOP_PUSH);
    store16ubits(ctx->dataspace, here, push_entry);
    here += 2;
    store16ubits(ctx->dataspace, here, n);
    here += 2;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);
    return (ctx->ip + 2);
}

/**
 * EMIT
 * ( char -- )
 * Transmit character to the current output device.
 */
static instruction_pointer_type instr_emit(T_Context* ctx, address_type word)
{
    cell_type c = pop(ctx->dataspace, &(ctx->data_stack));
    printf("%c", (char)c);
    return (ctx->ip + 2);
}

/**
 * ABORT
 *
 * Clear  the  data and return stacks,  setting  execution  mode.
 * Return control to the terminal.
 */

static instruction_pointer_type instr_abort(T_Context* ctx, address_type word)
{
    // TODO
    return 0; // TODO
}


static instruction_pointer_type instr_at(
        T_Context* ctx, address_type word)
{
    cell_type a = pop(ctx->dataspace, &(ctx->data_stack));
    cell_type v = fetch16ubits(ctx->dataspace, a);
    push(ctx->dataspace, &(ctx->data_stack), v);
    return (ctx->ip + 2);
}

static instruction_pointer_type instr_c_at(
        T_Context* ctx, address_type word)
{
    cell_type a = pop(ctx->dataspace, &(ctx->data_stack));
    cell_type v = fetch8bits(ctx->dataspace, a);
    push(ctx->dataspace, &(ctx->data_stack), v);
    return (ctx->ip + 2);
}

static instruction_pointer_type instr_pass(
        T_Context* ctx, address_type word)
{
    return (ctx->ip + 2);
}

/**
 * IMMEDIATE
 *
 * Mark the most recently defined word as being an immediate word
 */
static instruction_pointer_type instr_immediate(
        T_Context* ctx, address_type word)
{
    T_DictHeader* header;

    address_type current_head = ctx->dict_head;
    header = (T_DictHeader*)(ctx->dataspace + current_head);
    header->flags |= EF_IMMEDIATE;

    return (ctx->ip + 2);
}

/**
 * ?IMMEDIATE
 *
 * ( xt -- flag )
 *
 * Return TRUE if the word pointed to by xt is an immediate word
 *
 */

static instruction_pointer_type instr_qimmediate(
        T_Context* ctx, address_type word)
{
    T_DictHeader* header;

    cell_type xt = pop(ctx->dataspace, &(ctx->data_stack));
    header = (T_DictHeader*)(ctx->dataspace + xt);

    if (header->flags & EF_IMMEDIATE) {
        push(ctx->dataspace, &(ctx->data_stack), FORTH_TRUE);
    } else {
        push(ctx->dataspace, &(ctx->data_stack), FORTH_FALSE);
    }

    return (ctx->ip + 2);
}

static instruction_pointer_type instr_push(
        T_Context* ctx, address_type word)
{
    CF_Assert(false); // TODO
    return (ctx->ip + 2);
}

/**
 * IF
 *
 * Compile time behaviour for IF.  Stores the current value + 2
 * of HERE on the control stack.  This data will be later used by
 * THEN so fill in the address for the JUMP_IF_FALSE instruction.
 *
 * There are two types:
 *
 * Type 1
 *
 * IF                   jump_if_false <address1>
 *
 * ELSE                 jump <address2>
 *           <address1> pass
 *
 * THEN      <address2> pass
 *
 *
 * Type 2
 *
 * IF                   jump_if_false <address2>
 *
 * THEN      <address2> pass
 *
 */

static instruction_pointer_type instr_if(
        T_Context* ctx, address_type dict_entry)
{
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);

    /* Add  JUMP_IF_FALSE <address> to the current entry */
    address_type jump_if_false_entry = find_via_opcode(ctx, eOP_JUMP_IF);
    store16ubits(ctx->dataspace, here, jump_if_false_entry);
    here += 2;
    /* Place holder for the address to jump to, will lead to an illegal
     * instruction if by mistake it is not filled in. */
    store16ubits(ctx->dataspace, here, 0U);

    /* Push this address on the control stack, so the next ELSE or
     * THEN can overwrite it with the real value.
     */
    push(ctx->dataspace, &(ctx->control_stack), here);

    here += 2;

    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    return (ctx->ip + 2);
}

/**
 * THEN
 */

static instruction_pointer_type instr_then(
        T_Context* ctx, address_type word)
{
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type pass_entry = find_via_opcode(ctx, eOP_PASS);
    store16ubits(ctx->dataspace, here, pass_entry);

    /* Fill in the address of the previous ELSE or IF */
    address_type a = pop(ctx->dataspace, &(ctx->control_stack));
    store16ubits(ctx->dataspace, a, here);

    here += 2;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    return (ctx->ip + 2);
}

/**
 * ELSE
 */

static instruction_pointer_type instr_else(
        T_Context* ctx, address_type word)
{
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type jump_entry = find_via_opcode(ctx, eOP_JUMP);
    store16ubits(ctx->dataspace, here, jump_entry);
    here += 2;

    /* Fill in the address field of the previous by IF generated jump_if_false
     * instruction.
     */
    address_type a = pop(ctx->dataspace, &(ctx->control_stack));
    store16ubits(ctx->dataspace, a, here);

    push(ctx->dataspace, &(ctx->control_stack), here);

    here += 2;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    return (ctx->ip + 2);
}

static instruction_pointer_type instr_again(
        T_Context* ctx, address_type word)
{
    address_type here = fetch16ubits(ctx->dataspace, FF_LOC_HERE);
    address_type jump_entry = find_via_opcode(ctx, eOP_PASS);
    store16ubits(ctx->dataspace, here, jump_entry);
    here += 2;

    /* Fill in the address field of the previous by
     * BEGIN instruction.
     */
    address_type a = pop(ctx->dataspace, &(ctx->control_stack));
    store16ubits(ctx->dataspace, a, here);

    push(ctx->dataspace, &(ctx->control_stack), here);

    here += 2;
    store16ubits(ctx->dataspace, FF_LOC_HERE, here);

    return (ctx->ip + 2);
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

static void fill_instruction_table(void)
{
    /* First mark all possible opcodes as illegal */
    for (int i = 0; i < MAX_INSTRUCTION_COUNT; ++i) {
        instruction_table[i] = instr_illegal;
    }

    /* Then overwrite some to make them legal. */
    instruction_table[eOP_DROP]      = instr_drop;
    instruction_table[eOP_WORD]      = instr_word;
    instruction_table[eOP_CREATE]    = instr_create;
    instruction_table[eOP_CREATE_RT] = instr_create_rt;
    instruction_table[eOP_EXECUTE]   = instr_execute;
    instruction_table[eOP_ALLOT]     = instr_allot;
    instruction_table[eOP_STATE]     = instr_state;
    instruction_table[eOP_COLON]     = instr_colon;
    instruction_table[eOP_SEMICOLON] = instr_semicolon;
    instruction_table[eOP_COMMA]     = instr_comma;
    instruction_table[eOP_DOT]       = instr_dot;
    instruction_table[eOP_EMIT]      = instr_emit;
    instruction_table[eOP_BEGIN]     = instr_begin;
    instruction_table[eOP_IF]        = instr_if;
    instruction_table[eOP_THEN]      = instr_then;
    instruction_table[eOP_JUMP]      = instr_jump;
    instruction_table[eOP_JUMP_IF]   = instr_jump_if_false;
    instruction_table[eOP_QUERY]     = instr_query;
    instruction_table[eOP_IMMEDIATE] = instr_immediate;
    instruction_table[eOP_QIMMEDIATE] = instr_qimmediate;
    instruction_table[eOP_AT]        = instr_at;
    instruction_table[eOP_C_AT]      = instr_c_at;
    instruction_table[eOP_PASS]      = instr_pass;
    instruction_table[eOP_PUSH]      = instr_push;
    instruction_table[eOP_LITERAL]   = instr_literal;
    instruction_table[eOP_AGAIN]     = instr_again;
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
    if (h == 0) {
        printf("The dictionary is empty.\n");
    } else {
        do {
            word_flag_type flags;
            header = (T_DictHeader*)(ctx->dataspace+h);
            printf("name: ");
            for (int i = 0; i < header->name.count; ++i) {
                printf("%c", header->name.s[i]);
            }
            printf(" (%d characters)", header->name.count);
            printf("\n");

            printf("  previous: %u\n", header->previous);
            printf("  code_field: %u\n", header->code_field);
            printf("  flags (%02x):", header->flags);
            flags = header->flags;
            if (flags & EF_IMMEDIATE)      { printf(" immediate,"); }
            if (flags & EF_MACHINE_CODE)   { printf(" machine_code,"); }
            if (flags & EF_HIDDEN)         { printf(" hidden,"); }
            if (flags & EF_COMPILE_ONLY)   { printf(" compile time only,"); }
            printf("\n");

            h = header->previous;
        } while (h != 0);
    }
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

/**
 * Inner Interpreter
 *
 * Executes instruction pointed to by context->ip This is what makes FORTH
 * run.
 *
 * This runs until ctx->run becomes false. This is used by BYE to force an
 * exit of the program.
 */
void inner_interpreter(T_Context* ctx)
{
    address_type word;
    T_DictHeader* header;
    instruction_type opcode;

    while(ctx->run) {
        /* Fetch instruction (dictionary address of the word to be executed) */
        word = ctx->dataspace[ctx->ip];
        header = (T_DictHeader*)(&(ctx->dataspace[word]));
        /* The word itself will fetch any additional data needed and
         * advance the instruction pointer and return the new pointer */
        opcode = header->code_field;
        ctx->ip = instruction_table[opcode](ctx, word);
    }
}

/**
 * Mini version of INTERPRET that allows to run and compile
 * code. It does not do any error checking.
 * This function is used to compile the full version of INTERPRET
 * written in FORTH.
 */
void mini_interpret(T_Context* ctx)
{
    bool a_word_was_found = true;

    do {
        cell_type state;

        state = fetch16ubits(ctx->dataspace, FF_LOC_STATE);

        instr_find(ctx, 0);
        address_type word = pop(ctx->dataspace, &(ctx->data_stack));
        uint8_t n = fetch8ubits(ctx->dataspace, FF_LOC_WORD_BUFFER);

        if ((word == 0) && (n == 0)) {
            /* Zero can mean, the name of word was not found in the dictionary,
             * OR there were no more words in the input stream.
             */
            a_word_was_found = (n > 0);
        }

        if (!a_word_was_found) {
            /* There were no more words, so nothing to do */
        } else {
            /* A word was found */
            if (word == 0) {
                /* Not a word that was in the dictionary, so it should be a
                 * number */
                /* run:  WORD_BUFFER PNUMBER */
                push(ctx->dataspace, &(ctx->data_stack), FF_LOC_WORD_BUFFER);
                instr_pnumber(ctx, 0);
                /* Assume conversion always succeeds */
                instr_drop(ctx, 0);

                if (state == FF_STATE_COMPILING) {
                    instr_literal(ctx, 0);
                } else {
                    /* Leave the number on the data stack */
                }
            } else {
                push(ctx->dataspace, &(ctx->data_stack), word);

                if (state == FF_STATE_COMPILING) {
                    /* Is it an immediate word? */
                    T_DictHeader* header;
                    word_flag_type flags;
                    header = (T_DictHeader*)(ctx->dataspace + word);
                    flags = header->flags;
                    if (flags & EF_IMMEDIATE) {
                        /* Execute immediately */
                        instr_execute(ctx, word);
                    } else {
                        /* Compile into the current dictionary entry */
                        instr_comma(ctx, 0);
                    }
                } else {
                    instr_execute(ctx, word);
                }
            }
        }
    } while (a_word_was_found);
}

/**
 * Add a core word to the dictionary. These are words that
 * are directly implemented in C.
 */
T_DictHeader* add_core_word(
        T_Context* ctx, char* c_name, T_OpCode op_code, word_flag_type flags)
{
    static char buffer[sizeof(T_PackedString) + FF_MAX_DE_NAME_LENGTH];

    T_PackedString* name = (T_PackedString*) &(buffer[0]);
    clone_c_string(c_name, name);
    T_DictHeader* header = add_dict_entry(ctx, name);
    header->code_field = op_code;
    header->flags = flags;
    return header;
}

/**
 * Fill the dictionary with enough words to compile INTERPRET.
 * Then use that to compile the remaining words.
 */
void bootstrap(T_Context* ctx)
{
    add_core_word(ctx, "CREATE",     eOP_CREATE,    0);
    add_core_word(ctx, "ALLOT",      eOP_ALLOT,     0);
    add_core_word(ctx, "STATE",      eOP_STATE,     0);
    add_core_word(ctx, ":",          eOP_COLON,     0);
    add_core_word(ctx, ",",          eOP_COMMA,     0);
    add_core_word(ctx, ".",          eOP_DOT,       0);
    add_core_word(ctx, "@",          eOP_AT,        0);
    add_core_word(ctx, "C@",         eOP_C_AT,      0);
    add_core_word(ctx, "EMIT",       eOP_EMIT,      0);
    add_core_word(ctx, "PASS",       eOP_PASS,      0);
    add_core_word(ctx, "QUERY",      eOP_QUERY,     0);
    add_core_word(ctx, "IMMEDIATE",  eOP_IMMEDIATE, 0);
    add_core_word(ctx, "?IMMEDIATE", eOP_QIMMEDIATE, 0);

    add_core_word(ctx, ";",      eOP_SEMICOLON, EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "BEGIN",  eOP_BEGIN,     EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "AGAIN",  eOP_AGAIN,     EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "IF",     eOP_IF,        EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "THEN",   eOP_THEN,      EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "WHILE",  eOP_WHILE,     EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "REPEAT", eOP_REPEAT,    EF_IMMEDIATE | EF_COMPILE_ONLY);
    add_core_word(ctx, "LITERAL", eOP_LITERAL,  EF_IMMEDIATE | EF_COMPILE_ONLY);

    /* Hidden, have run time behaviour only, used by compiling words
     * and immediate words. */
    add_core_word(ctx, "PUSH",     eOP_PUSH,     EF_HIDDEN);
    add_core_word(ctx, "JUMP",     eOP_JUMP,     EF_HIDDEN);
    add_core_word(ctx, "JUMP_IF",  eOP_JUMP_IF,  EF_HIDDEN);

    /* We now have enough words to compile a new version
     * of INTERPRET
     */
    paste_code(ctx, ": test ; CREATE FOO 2 ALLOT");
    mini_interpret(ctx);
    // paste_code(ctx, ": CR 13 EMIT ;  'O' EMIT 'K' EMIT CR");

    paste_code(ctx, interpret_code);
    mini_interpret(ctx);

    {
        paste_code(ctx, "INTERPRET");
        instr_find(ctx, 0);
        address_type word = pop(ctx->dataspace, &(ctx->data_stack));
        T_DictHeader* header = (T_DictHeader*)(&(ctx->dataspace[word]));
        uint32_t n = header->name.count;
        address_type parameter_field = word;
        parameter_field += n + DE_SIZE_MIN;
        parameter_field = align(parameter_field);
        ctx->ip = parameter_field;
        {
            instruction_type opcode;
            while(ctx->run) {
                /* Fetch instruction (dictionary address of the word to be executed) */
                word = fetch16ubits(ctx->dataspace, ctx->ip);
                header = (T_DictHeader*)(&(ctx->dataspace[word]));
                /* The word itself will fetch any additional data needed and
                 * advance the instruction pointer and return the new pointer */
                opcode = header->code_field;
                ctx->ip = instruction_table[opcode](ctx, word);
            }
        }
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
        bootstrap(&context);
    //    info(&context);
    } else {
        if (actions & DO_INFO) { info(&context); }
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
