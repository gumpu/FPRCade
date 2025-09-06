#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

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
static address_type align(address_type address)
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

// TODO packed  instead of packet
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
instruction_pointer_type instr_word(T_Context* ctx, address_type dict_entry)
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
instruction_pointer_type instr_find(T_Context* ctx, address_type dict_entry)
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

instruction_pointer_type instr_create(
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
instruction_pointer_type instr_create_rt(T_Context* ctx, address_type dict_entry)
{
    // TODO
    T_DictHeader* header = (T_DictHeader*)(&(ctx->dataspace[dict_entry]));
    uint32_t n = header->name.count;
    address_type parameter_field = dict_entry;
    parameter_field += n + DE_SIZE_MIN;
    //
    // TODO FIXME this should be align
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
instruction_pointer_type instr_query(T_Context* ctx)
{
    char buffer[81];
    fgets(buffer, 80, stdin);
    paste_code(ctx, buffer);
    return (ctx->ip + 2);
}

/**
 * EXECUTE   ( xt -- )
 */
instruction_pointer_type instr_execute(T_Context* ctx, address_type dict_entry)
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
instruction_pointer_type instr_jump(T_Context* ctx, address_type dict_entry)
{
    address_type  address = ctx->dataspace[ctx->ip + 2];
    return address;
}

/**
 * ALLOT  (n -- )  add n bytes to the parameter field of the most
 * recently defined word.
 */
instruction_pointer_type instr_allot(T_Context* ctx, address_type dict_entry)
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
instruction_pointer_type instr_illegal(T_Context* ctx, address_type dict_entry)
{
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
instruction_pointer_type instr_state(T_Context* ctx, address_type dict_entry)
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
instruction_pointer_type instr_enter(T_Context* ctx, address_type dict_entry)
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
 * : <name>
 *
 */
instruction_pointer_type instr_colon(T_Context* ctx, address_type dict_entry)
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
        store16ubits(ctx->dataspace, FF_LOC_STATE, 1);
    } else {
        // TODO
        // Error and abort
    }

    return (ctx->ip + 2);
}

instruction_pointer_type instr_semicolon(T_Context* ctx, address_type dict_entry)
{
    store16ubits(ctx->dataspace, FF_LOC_STATE, 0);
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
instruction_pointer_type instr_comma(T_Context* ctx, address_type word)
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

instruction_pointer_type instr_dot(T_Context* ctx, address_type word)
{

    signed_cell_type n =
        (signed_cell_type)(pop(ctx->dataspace, &(ctx->data_stack)));

    // TODO  Other bases
    printf("%d ", n);

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
 */
instruction_pointer_type instr_pnumber(T_Context* ctx, address_type word)
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
    instruction_table[eOP_WORD]      = instr_word;
    instruction_table[eOP_CREATE]    = instr_create;
    instruction_table[eOP_CREATE_RT] = instr_create_rt;
    instruction_table[eOP_EXECUTE]   = instr_execute;
    instruction_table[eOP_JUMP]      = instr_jump;
    instruction_table[eOP_ALLOT]     = instr_allot;
    instruction_table[eOP_STATE]     = instr_state;
    instruction_table[eOP_COLON]     = instr_colon;
    instruction_table[eOP_SEMICOLON] = instr_semicolon;
    instruction_table[eOP_COMMA]     = instr_comma;
    instruction_table[eOP_DOT]       = instr_dot;
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
            if (flags & EF_IMMEDIATE)      { printf(" immediate"); }
            if (flags & EF_MACHINE_CODE)   { printf(" machine_code"); }
            if (flags & EF_COLON_DEF_ONLY) { printf(" colon_def_only"); }
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
 * Executes instruction pointed to by context->ip This is what makes FORTH
 * run.
 */
void inner_interpreter(T_Context* ctx)
{
    address_type word;
    T_DictHeader* header;
    instruction_type opcode;

    while(ctx->run) {
        /* Fetch instruction (address of the word to be executed) */
        word = ctx->dataspace[ctx->ip];
        header = (T_DictHeader*)(&(ctx->dataspace[word]));
        /* The word itself will fetch any additional data needed and
         * advance the instruction pointer and return the new pointer */
        opcode = header->code_field;
        ctx->ip = instruction_table[opcode](ctx, word);
    }
}

/**
 *
 */
void mini_interpret(T_Context* ctx, int n)
{
    for (unsigned i = 0; i < n; ++i) {
        instr_find(ctx, 0);
        address_type word = pop(ctx->dataspace, &(ctx->data_stack));
        if (word == 0) {
            /* Not a word but a number */
            /* run:  WORD_BUFFER PNUMBER */
            push(ctx->dataspace, &(ctx->data_stack), FF_LOC_WORD_BUFFER);
            instr_pnumber(ctx, 0);
        } else {
            push(ctx->dataspace, &(ctx->data_stack), word);
            instr_execute(ctx, word);
        }
    }
}

void add_core_word(T_Context* ctx, char* c_name, T_OpCode op_code)
{
    static char buffer[sizeof(T_PackedString) + FF_MAX_DE_NAME_LENGTH];
    T_PackedString* name = (T_PackedString*) &(buffer[0]);
    clone_c_string(c_name, name);
    T_DictHeader* header = add_dict_entry(ctx, name);
    header->code_field = op_code;
}

void bootstrap(T_Context* ctx)
{
    add_core_word(ctx, "CREATE", eOP_CREATE);
    add_core_word(ctx, "ALLOT",  eOP_ALLOT);
    add_core_word(ctx, "STATE",  eOP_STATE);
    add_core_word(ctx, ":",      eOP_COLON);
    add_core_word(ctx, ";",      eOP_SEMICOLON);
    add_core_word(ctx, ",",      eOP_COMMA);
    add_core_word(ctx, ".",      eOP_DOT);

    paste_code(ctx, ": test ; CREATE FOO 2 ALLOT");
    mini_interpret(ctx, 5);
    paste_code(ctx, "FOO .");
    mini_interpret(ctx, 2);
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
        info(&context);
    } else {
        if (actions & DO_INFO) { info(&context); }
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
