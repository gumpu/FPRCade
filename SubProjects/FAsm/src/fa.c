/**
 * Assembler for the FPRCade processor Stackmaster-16
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <getopt.h>
#include <assert.h>

#define FA_MAX_WORD_SIZE 32
#define FA_LINE_BUFFER_SIZE 1024
#define FA_MAX_SYMBOL_SIZE 32
// TODO Need prefix FA
#define MAX_DIRECTIVE_NAME_LENGTH 32
#define MAX_INSTRUCTION_NAME_LENGTH 32
#define MAX_NUMBER_OF_SYMBOLS 1024
#define MAX_ERROR_MESSAGE_LENGTH 1024

#define FA_MAX_OUTPUT_FILENAME_LENGTH (5*80)
#define FA_MAX_LISTING_FILENAME_LENGTH (5*80)
#define FA_MAX_SYMBOL_FILENAME_LENGTH (5*80)

#define HEX_MAX_BUFFER_SIZE 16

/* --------------------------------------------------------------------*/
/* Valid stack IDs fit in two bits */
enum FA_StackID {
   eSID_Data = 0x0,
   eSID_Control = 0x1,
   eSID_Return = 0x2,
   eSID_Temp = 0x3,
   eSID_Unknown = 0xF
};

enum FA_SizeID {
   eSZ_Byte = 0x1,
   eSZ_Word = 0x2,
   eSZ_Long = 0x4,
   eSZ_Unknown = 0xF
};


enum FA_Phase {
    eFA_Scan = 0,
    eFA_Assemble = 1,
    eFA_NumberOfPhases
};

enum FA_ErrorReason {
    eFA_AllOk = 0,
    eFA_Unknown_Directive,
    eFA_Syntax_Error,
    eFA_DirectiveTooLong,
    eFA_AddressError,
    eFA_StackID_Error,

    /* Should be the last entry */
    eFA_NumberOfErrors
};

/* --------------------------------------------------------------------*/

typedef struct symbol_value_pair {
    char symbol[FA_MAX_SYMBOL_SIZE];
    uint16_t value;
} symbol_value_pair_t;

/**
 * Lookup table to match symbols to the corresponding
 * value (location / constants )
 */
typedef struct symbol_table {
    uint16_t number;
    symbol_value_pair_t symbols[MAX_NUMBER_OF_SYMBOLS];
} symbol_table_type;

/**
 * TODO
 */
typedef uint16_t instruction_callback_type(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    FILE* listf,
    uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reaso);

typedef uint16_t directive_callback_type(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    FILE* listf,
    uint16_t address, symbol_table_type* table,
    enum FA_Phase phase, enum FA_ErrorReason* reason);

/**
 * TODO
 */
typedef struct instruction_generator {
    char* instruction_name;
    instruction_callback_type* generator;
    uint16_t byte_count;
} instruction_generator_type;

/**
 * TODO
 */
typedef struct directive_generator {
    char* directive_name;
    directive_callback_type* generator;
} directive_generator_type;

/* --------------------------------------------------------------------*/

static void report_error(enum FA_ErrorReason reason, char* line, int line_no);
static enum FA_SizeID get_size_id(char c);
static enum FA_StackID get_stack_id(char c);
static int16_t to_digit(char c, uint16_t base);
static bool read_number(
    const char *word,
    int64_t* value,
    symbol_table_type* table,
    enum FA_ErrorReason* reason,
    bool* is_negative_output
    );
static unsigned skip_word(const char line[FA_LINE_BUFFER_SIZE], unsigned n);
static unsigned skip_spaces(const char line[FA_LINE_BUFFER_SIZE], unsigned n);
static unsigned get_word(
    char word[FA_MAX_WORD_SIZE],
    const char line[FA_LINE_BUFFER_SIZE],   // TODO Size does not make sense
    char seperator, int max_len);
static void init_label_table(symbol_table_type* lt);
static bool find_symbol(
    symbol_table_type* lt, const char* label, uint16_t *address);
static bool add_symbol(symbol_table_type* lt, char* symbol, uint16_t value);
static bool add_label(
    symbol_table_type* lt,
    const char line[FA_LINE_BUFFER_SIZE], uint16_t address);
static bool check_label(
    symbol_table_type* lt,
    const char line[FA_LINE_BUFFER_SIZE],
    uint16_t address);
static void emit_code(
    FILE* outpf, FILE* listf, uint16_t address, uint16_t instruction);
static uint16_t leave_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t enter_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t nop_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t halt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t ldl_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t xor_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t or_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t and_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t eq_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t rd_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t sto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t ird_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t isto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t rd_sto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    FILE* listf,
    uint16_t address,
    enum FA_ErrorReason* reason,
    uint16_t opcode);
static uint16_t lt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t gt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t add_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t neg_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t dup_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t bif_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason);
static uint16_t dot_str_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_Phase phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_b_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_w_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_l_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_def_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_org_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static uint16_t dot_align_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason);
static bool is_label(const char stripped_line[FA_LINE_BUFFER_SIZE]);
static bool is_directive(const char stripped_line[FA_LINE_BUFFER_SIZE]);
static bool is_instruction(const char stripped_line[FA_LINE_BUFFER_SIZE]);
static void remove_remarks(
    char remark_less_line[FA_LINE_BUFFER_SIZE],
    const char line[FA_LINE_BUFFER_SIZE]);
static void strip_line(
    char stripped_line[FA_LINE_BUFFER_SIZE],
    const char line[FA_LINE_BUFFER_SIZE]);
static bool parse_directive(
    char *line, int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf, FILE* listf,
    symbol_table_type* symbol_table);
static bool parse_instruction(
    const char line[FA_LINE_BUFFER_SIZE], int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf,
    FILE* list_outpf,
    symbol_table_type* symbol_table);
static bool assemble(
    FILE* inpf, FILE* outpf, FILE* list_outpf, symbol_table_type* symbol_table);
static void display_usage(void);
static void parse_options(
    int argc,
    char** argv,
    char* output_file_name,
    char* listing_file_name,
    char* symbol_file_name);

/* --------------------------------------------------------------------*/

static char line_buffer[FA_LINE_BUFFER_SIZE];
static char remark_less_line_buffer[FA_LINE_BUFFER_SIZE];
static char stripped_line_buffer[FA_LINE_BUFFER_SIZE];
static char error_message[MAX_ERROR_MESSAGE_LENGTH];

/* For hex file format */
static uint8_t  hex_byte_buffer[HEX_MAX_BUFFER_SIZE];
static uint16_t hex_current_address = 0;
static uint8_t  hex_current_byte = 0;

/* --------------------------------------------------------------------
 * Routines to create output in .hex format
 * --------------------------------------------------------------------*/

/**
 * Create a single .hex record containing n data items
 *
 * @param[in] outpf - file to writ to
 * @param[in] address - address to be used in the record
 * @param[in] n - number of data items
 * @param[in] bytes - array with n byte values
 *
 * @returns  number of bytes written
 */
int hex_write_data_bytes(
        FILE* outpf, uint16_t address, uint8_t* bytes, uint8_t n)
{
    int result;
    uint8_t checksum = 0;
    checksum += n;
    checksum += (uint8_t)((address) & 0xFF);
    checksum += (uint8_t)(address >> 8);
    result = fprintf(outpf, ":%02X%04X%02X", n, address, 0x00);
    for (uint8_t i = 0; (i < n) && (result > 0); ++i) {
        checksum += bytes[i];
        result = fprintf(outpf, "%02X", bytes[i]);
    }
    if (result > 0) {
        checksum = ~(checksum) + 1;
        result = fprintf(outpf, "%02X\n", checksum);
    }
    return result;
}

/**
 * Write an end of records
 */

int hex_write_end_of_file(FILE* outpf)
{
    return fprintf(outpf, ":00000001FF\n");
}

/**
 * @brief Set the start address to be used for
 * all following hex_push_byte() and hex_flush()
 * calls.
 *
 * hex_set_start_address(0xC000)
 * hex_push_byte(outpf, 0xA0))
 * hex_push_byte(outpf, 0xB0))
 * hex_push_byte(outpf, 0xF0))
 *
 * Will put 0xA0 at 0xC000,
 * 0xB0 at 0xC001 and 0xF0 at 0xC002
 *
 * @param[in] adddres   address to be used
 * @result Nothing
 */

int hex_set_start_address(uint16_t address)
{
    hex_current_address = address;
    hex_current_byte = 0;
    return address;
}

/* TODO: Documentation */
int hex_push_byte(FILE* outpf, uint8_t byte)
{
    int result = 0;
    hex_byte_buffer[hex_current_byte] = byte;
    hex_current_byte++;
    if (hex_current_byte == HEX_MAX_BUFFER_SIZE) {
        result = hex_write_data_bytes(
                outpf, hex_current_address,
                hex_byte_buffer, HEX_MAX_BUFFER_SIZE);
        hex_current_address += HEX_MAX_BUFFER_SIZE;
        hex_current_byte = 0;
    }
    return result;
}

/* TODO: Documentation */
int hex_flush(FILE* outpf)
{
    int result = 0;
    if (hex_current_byte > 0) {
        result = hex_write_data_bytes(
                outpf, hex_current_address,
                hex_byte_buffer, hex_current_byte);
        hex_current_address += hex_current_byte;
        hex_current_byte = 0;
    }
    return result;
}

/* --------------------------------------------------------------------*/

/**
 * Error message table based on FA_ErrorReason
 */

static char* error_reason_text [] = {
    "All is OK",
    "Unknown directive",
    "Syntax Error",
    "Directive is too long",
    "Address error",

    "INTERNAL ERROR"
};


// TODO Should take an FA_ErrorReason
static void report_error(enum FA_ErrorReason reason, char* line, int line_no)
{
    snprintf(error_message,
            MAX_ERROR_MESSAGE_LENGTH, "%s", error_reason_text[reason]);
}

/**
 * Check the size parameter
 *
 * Value can be used in a opcode to indicate size.
 */
static enum FA_SizeID get_size_id(char c)
{
    enum FA_SizeID size;

    switch (c) {
        case 'B': size = eSZ_Byte; break;
        case 'W': size = eSZ_Word; break;
        case 'L': size = eSZ_Long; break;
        default:
            size = eSZ_Unknown;
    }
    return size;
}

/**
 * Parse stack indicator
 *
 * Value can be used in a opcode to indicate which stack to
 * use.
 */

static enum FA_StackID get_stack_id(char c)
{
    uint16_t id;
    switch (c) {
        case 'D': id = eSID_Data; break;
        case 'C': id = eSID_Control; break;
        case 'R': id = eSID_Return; break;
        case 'T': id = eSID_Temp; break;
        default:
            /* Error, unknown stack */
            id = eSID_Unknown;
            break;
    }
    return id;
}

/* --------------------------------------------------------------------*/

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



/**
 * Given a word returns the number it represents
 *
 * Handles:
 * - binary numbers;       %1010101 %-11
 * - decimal numbers;      328912  #3282  #-100 -9
 * - hexadecimal numbers:  $DEADBEAF
 * - symbol values;         routine1
 *
 * Returns a 64 bit signed number;
 */

static bool read_number(
        const char *word,
        int64_t* value,
        symbol_table_type* table,
        enum FA_ErrorReason* reason,
        bool* is_negative_output
        )
{
    bool is_wrong = false;
    char first_char = word[0];
    bool is_negative = false;
    bool is_label = false;
    uint16_t u = strlen(word);
    int64_t result = 0;
    int64_t base = 10;

    if (first_char == '\'') {
        if ((u == 3) && (word[2] == '\'')) {
            result = (int32_t)word[1];
        } else {
            is_wrong = true;
        }
    } else {
        uint32_t i = 1;

        switch (first_char) {
            case '%': /* Binary number */
                {
                    base = 2;
                    if (u > 1) {
                        is_negative = (word[1] == '-');
                        is_wrong = is_negative && (u == 2); /* Only %- */
                        if (is_negative) { ++i; }
                    } else {
                        is_wrong = true;
                    }
                }
                break;
            case '#': /* Decimal number */
                {
                    base = 10;
                    if (u > 1) {
                        is_negative = (word[1] == '-');
                        is_wrong = is_negative && (u == 2); /* Only #- */
                        if (is_negative) { ++i; }
                    } else {
                        is_wrong = true;
                    }
                }
                break;
            case '$': /* Hexadecimal number */
                {
                    base = 16;
                    if (u > 1) {
                        is_negative = (word[1] == '-'); /* Only $- */
                        is_wrong = is_wrong && (u == 2);
                        if (is_negative) { ++i; }
                    } else {
                        is_wrong = true;
                    }
                }
                break;
            case '-': /* Negative decimal number */
                is_negative = true;
                base = 10;
                break;
            default: /* Label or a non negative decimal
                        number */
                {
                    base = 10;
                    int32_t d = to_digit(word[0], base);
                    if (d >= 0) {
                        result += d;
                    } else {
                        is_label = true;
                        break;
                    }
                }
        }

        if (is_label) {
            uint16_t location;
            if (find_symbol(table, word, &location)) {
                result = location;
            } else {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                        "%s", "label not found");
                is_wrong = true;
                *reason = eFA_AddressError;
            }
        } else {
            for (; (i < u) && !is_wrong; i++) {
                result = result*base;
                if (word[i] == '-') {
                    if (is_negative) {
                        is_wrong = true;
                        break;
                    }
                } else {
                    int32_t d = to_digit(word[i], base);
                    if (d >= 0) {
                        result += d;
                    } else {
                        is_wrong = true;
                        break;
                    }
                }
            }
        }
    }

    if (is_wrong) {
        if (*reason != eFA_AddressError) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s (%s)", "malformed number", word);
            *reason = eFA_Syntax_Error;
            /* Make sure the function returns non-random values */
            *value = 0;
            *is_negative_output = false;
        }
    } else {
        if (is_negative) {
            result = 0 - result;
        }
        *value = result;
        *is_negative_output = is_negative;
    }

    return !is_wrong;
}

/**
 * Given a line of text and the position n in that line of text
 * skip all characters until a space charter is found.
 *
 * Returns the index to the space character or the index
 * to the end of the line in case no spaces were found.
 *
 * Note: Does not skip leading spaces.
 */
static unsigned skip_word(const char line[FA_LINE_BUFFER_SIZE], unsigned n)
{
    unsigned i;
    for (i = n; (line[i] != '\0') && !isspace(line[i]); ++i);
    return i;
}

/**
 * Given a line of text and the position n in that line of text skip all space
 * characters until a non space character is found.
 *
 * Returns the index to the nonspace character or the index
 * to the end of the line in case only spaces were found.
 */

static unsigned skip_spaces(const char line[FA_LINE_BUFFER_SIZE], unsigned n)
{
    unsigned i = n;
    if (line[i] != '\0') {
        for (; isspace(line[i]); ++i);
    } else {
        /* Do not go past the end of a line */
    }
    return i;
}

/**
 * Return the first word found in a line of text delimited by the given
 * separator.
 *
 * If the word would not fit in the word buffer the routine returns an empty
 * string.
 *
 * Returns the length of the word that was extracted.
 *
 */

static unsigned get_word(
    char word[FA_MAX_WORD_SIZE],
    const char line[FA_LINE_BUFFER_SIZE],   // TODO Size does not make sense
    char seperator, int max_len)
{
    unsigned i;

    assert(max_len > 0);

    for (i = 0; i < max_len; ++i) {
        char x = line[i];

        if (x == seperator) {
            word[i] = '\0';
            break;
        } else if (x == '\0') {
            word[i] = '\0';
            break;
        } else {
            word[i] = x;
        }
    }

    if (i == max_len) {
        /* Too long, return "" */
        word[0] = '\0';
    }

    return i;
}


/* --------------------------------------------------------------------*/

static void init_label_table(symbol_table_type* lt)
{
    lt->number = 0;
}

/*
 * Lookup a given label in the symbol table.
 *
 * Returns true if the label was found.
 * The corresponding location is returned in `address`
 */
static bool find_symbol(
        symbol_table_type* lt, const char* label, uint16_t *address)
{
    int i;
    int n = lt->number;
    bool found = false;

    for (i = 0; i < n; ++i) {
        if (strcmp(label, lt->symbols[i].symbol) == 0) {
            found = true;
            *address = lt->symbols[i].value;
            break;
        }
    }

    return found;
}

void dump_symbol_table(symbol_table_type* lt, FILE* outpf)
{
    int n = lt->number;
    for (unsigned i = 0; i < n; ++i) {
        fprintf(outpf, ".def %s $%04x\n",
                lt->symbols[i].symbol,
                lt->symbols[i].value);
    }
}

/**
 * Adds a symbol and the corresponding value to the symbol table.
 *
 * Returns true if successful
 */
static bool add_symbol(symbol_table_type* lt, char* symbol, uint16_t value)
{
    bool is_ok = true;
    int n = lt->number;
    if (n == MAX_NUMBER_OF_SYMBOLS) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                        "%s", "label table is full");
        is_ok = false;
    } else {
        unsigned l = strlen(symbol);
        assert(l < FA_MAX_SYMBOL_SIZE);

        strcpy(&(lt->symbols[n].symbol[0]), symbol);
        lt->symbols[n].value = value;
        ++(lt->number);
    }
    return is_ok;
}

/**
 * Add a new label to the symbol table
 *
 * Given a line of source code that contains a label
 * add a new symbol to the symbol table with as name
 * the label and as value the current assembly address.
 *
 */

static bool add_label(
        symbol_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE], uint16_t address)
{
    bool is_ok = true;
    unsigned i;
    char label[FA_MAX_SYMBOL_SIZE];

    i = get_word(label, line, ':', FA_MAX_SYMBOL_SIZE);

    if (i < FA_MAX_SYMBOL_SIZE) {
        is_ok = add_symbol(lt, label, address);
    } else {
        is_ok = false;
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                "%s", "symbol name is to long");
    }

    return is_ok;
}


static bool check_label(
        symbol_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE],
        uint16_t address)
{
    bool is_ok = true;
    char label[FA_MAX_SYMBOL_SIZE];
    unsigned i;

    i = get_word(label, line, ':', FA_MAX_SYMBOL_SIZE);

    if (i >= FA_MAX_SYMBOL_SIZE) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                "%s", "Internal error");
        is_ok = false;
    } else {
        uint16_t ist_address;
        if (find_symbol(lt, label, &ist_address)) {
            if (ist_address == address) {
                /* OK */
            } else {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                        "%s", "Label address mismatch");
                is_ok = false;
            }
        } else {
            /* Label can't be found, but it should be there */
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s", "Can't find label location");
            is_ok = false;
        }
    }

    return is_ok;
}

/* --------------------------------------------------------------------*/

static void emit_code(
        FILE* outpf, FILE* listf, uint16_t address, uint16_t instruction)
{
    uint8_t msb = (uint8_t)(instruction >> 8);
    uint8_t lsb = (uint8_t)(instruction & 0xFF);
    hex_push_byte(outpf, lsb);
    hex_push_byte(outpf, msb);
    if (listf) {
        fprintf(listf, ">\t%04X %04X\n", address, instruction);
    }
}

/* Generators */

static uint16_t leave_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0x8100);
    return address + 2;
}

static uint16_t enter_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    uint16_t new_address = address;
    unsigned i = 0;
    i = skip_word(line, i);
    i = skip_spaces(line, i);

    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                "%s", "missing label");
        *reason = eFA_Syntax_Error;
    } else {
        uint16_t location;
        if (find_symbol(table, &(line[i]), &location)) {
            if (location & 0x03) {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                        "destination address, %s (%04x), is not aligned",
                        &(line[i]), location);
                *reason = eFA_AddressError;
            } else {
                uint16_t instruction;
                instruction = 0x4000;
                location = (location >> 2);
                instruction |= location & 0x3FFF;
                emit_code(outpf, listf, address, instruction);
                new_address += 2;
            }
        } else {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "label not found");
            *reason = eFA_Syntax_Error;
        }
    }

    return new_address;
}

static uint16_t nop_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0x8000);
    return address + 2;
}

static uint16_t halt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0x8200);
    return address + 2;
}

/**
 * Handles:
 *
 * LDL d 0x00000
 * LDL r 0x00000
 * LDL c 0x00000
 * LDL t 0x00000
 *
 */
static uint16_t ldl_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    uint16_t new_address = address;
    unsigned i;

    i = skip_word(line, 0);
    i = skip_spaces(line, i);

    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "missing destination and value");
        *reason = eFA_Syntax_Error;
    } else {
        uint16_t used_value;
        int64_t value;
        char destination = line[i];
        if (line[i+1] == '\0') {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "missing value");
            *reason = eFA_Syntax_Error;
        } else if (!isspace(line[i+1])) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "malformed stack ID");
            *reason = eFA_StackID_Error;
        } else {
            uint16_t bits = get_stack_id(destination);
            if (bits == eSID_Unknown) {
                snprintf(error_message,
                        MAX_ERROR_MESSAGE_LENGTH, "%s", "unknown stack ID");
                *reason = eFA_StackID_Error;
            } else {
                i = skip_word(line, i);
                i = skip_spaces(line, i);
                if (line[i] != '\0') {
                    bool is_negative;
                    char numberstring[FA_MAX_WORD_SIZE];
                    get_word(numberstring, &(line[i]), ' ', FA_MAX_WORD_SIZE);
                    if (read_number(numberstring, &value, table, reason, &is_negative)) {
                        assert(*reason == eFA_AllOk);
                        if ((value < 0) || (value > 1023) || is_negative) {
                             snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                                     "%s", "value does not fit");
                            *reason = eFA_Syntax_Error;
                        } else {
                            uint16_t instruction = 0xC000;
                            used_value = (uint16_t)value;
                            instruction |= used_value;
                            instruction |= (bits << 10);
                            emit_code(outpf, listf, address, instruction);
                            new_address += 2;
                        }
                    } else {
                        /* There was some error reading the number */
                        assert(*reason != eFA_AllOk);
                    }
                }
            }
        }
    }
    return new_address;
}

static uint16_t xor_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe780);
    return address + 2;
}

static uint16_t or_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe700);
    return address + 2;
}

static uint16_t and_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe680);
    return address + 2;
}

static uint16_t eq_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe180);
    return address + 2;
}


/**
 * RD
 */
static uint16_t rd_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    return rd_sto_generator(line, outpf, listf, address, reason, 0xF200);
}


/**
 * STO
 */
static uint16_t sto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    return rd_sto_generator(line, outpf, listf, address, reason, 0xE580);
}

/**
 * IRD
 */
static uint16_t ird_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    return rd_sto_generator(line, outpf, listf, address, reason, 0xF280);
}


/**
 * ISTO
 */
static uint16_t isto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    return rd_sto_generator(line, outpf, listf, address, reason, 0xE5C0);
}


/**
 *
 */
static uint16_t rd_sto_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    FILE* listf,
    uint16_t address,
    enum FA_ErrorReason* reason,
    uint16_t opcode)
{
    unsigned i;

    i = skip_word(line, 0);
    i = skip_spaces(line, i);

    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "missing size indicator");
        *reason = eFA_Syntax_Error;
    } else {
        enum FA_SizeID size = get_size_id(line[i]);
        if (size == eSZ_Unknown) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s", "unknown size indicator");
            *reason = eFA_Syntax_Error;
        } else {
            emit_code(outpf, listf, address, (opcode | size));
        }
    }
    return address + 2;
}


static uint16_t lt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe290);
    return address + 2;
}

static uint16_t gt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe310);
    return address + 2;
}

static uint16_t add_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xe010);
    return address + 2;
}

static uint16_t neg_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    emit_code(outpf, listf, address, 0xf000);
    return address + 2;
}

static uint16_t dup_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    uint16_t new_address = address;
    unsigned i = 0;
    i = skip_word(line, i);
    i = skip_spaces(line, i);
    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "no destination");
    } else {
        char destination = line[i];
        uint16_t bits = get_stack_id(destination);
        if (bits == 0x7) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "illegal destination");
        } else {
            uint16_t instruction = 0xB100 | (bits << 6);
            emit_code(outpf, listf, address, instruction);
            new_address += 2;
        }
    }
    return new_address;
}

static uint16_t bif_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_ErrorReason* reason)
{
    uint16_t new_address = address;
    unsigned i = 0;
    i = skip_word(line, i);
    i = skip_spaces(line, i);
    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "missing label");
    } else {
        uint16_t location;
        if (find_symbol(table, &(line[i]), &location)) {
            uint16_t instruction;
            int16_t  delta;
            instruction = 0x1000;
            delta = location - new_address;
            instruction |= delta & 0x0FFF;
            emit_code(outpf, listf, address, instruction);
            new_address += 2;
        } else {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "label not found");
        }
    }
    return new_address;
}

static instruction_generator_type instruction_generators[] = {
    {"NEG",   neg_generator,   2},
    {"ADD",   add_generator,   2},
    {"DUP",   dup_generator,   2},
    {"NOP",   nop_generator,   2},
    {"HALT",  halt_generator,  2},
    {"LDL",   ldl_generator,   2},
    {"LT",    lt_generator,    2},
    {"GT",    gt_generator,    2},
    {"BIF",   bif_generator,   2},
    {"LEAVE", leave_generator, 2},
    {"ENTER", enter_generator, 2},
    {"OR",    or_generator,    2},
    {"AND",   and_generator,   2},
    {"XOR",   xor_generator,   2},
    {"EQ",    eq_generator,    2},
    {"RD",    rd_generator,    2},
    {"STO",   sto_generator,   2},
    {"IRD",   ird_generator,   2},
    {"ISTO",  isto_generator,  2},

    /* Should always be the last one */
    {NULL, NULL}
};

/* --------------------------------------------------------------------*/

/**
 * Handles
 *
 *  .str "some string"
 *
 * '.str' are the first four characters of `line`
 *
 * inputs:
 *
 * - line
 * - outpf
 * - address
 * - table
 * - phase
 *
 * returns new address
 *
 */
static uint16_t dot_str_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, symbol_table_type* table,
    enum FA_Phase phase, enum FA_ErrorReason* error_reason)
{

    assert(line[3] == 'R');
    unsigned n = strlen(line);
    unsigned bytes_needed = 0;
    unsigned i = 0;
    if (n > 4) {
        i = skip_spaces(line, 4);
        if (line[i] == '"') {
            if (phase == eFA_Assemble) {
                for (++i ; (line[i] != '\0') && (line[i] != '"'); ++i) {
                    hex_push_byte(outpf, line[i]);
                    bytes_needed++;
                }
            } else {
                for (++i ; (line[i] != '\0') && (line[i] != '"'); ++i) {
                    bytes_needed++;
                }
            }
        } else {
            /* Missing '"' */
            *error_reason = eFA_Syntax_Error;
        }
    } else {
        /* A ".str" but not followed by a string */
        *error_reason = eFA_Syntax_Error;
    }
    return address + bytes_needed;
}

/**
 * returns new address
 */
static uint16_t dot_b_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{

    char numberstring[FA_MAX_WORD_SIZE];
    unsigned i = 0;
    assert(line[1] == 'B');
    unsigned n = 0;
    unsigned bytes_needed = 0;

    i = skip_word(line, 0);
    i = skip_spaces(line, i);

    n = get_word(numberstring, &(line[i]), ' ', MAX_INSTRUCTION_NAME_LENGTH);
    if (n == 0) {
        /* .b  but no values */
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                ".b needs at least one value");
        *error_reason = eFA_Syntax_Error;
    } else {
        while (n > 0) {
            ++bytes_needed;
            if (phase == eFA_Assemble) {
                bool is_negative;
                int64_t value;
                if (read_number(numberstring, &value, table, error_reason, &is_negative)) {
                    if (is_negative && (value >= -128) && (value < 128)) {
                        uint8_t byte_value = (value & 0xFF);
                        hex_push_byte(outpf, byte_value);
                    } else if (!is_negative && (value >= 0) && (value < 256)) {
                        uint8_t byte_value = (value & 0xFF);
                        hex_push_byte(outpf, byte_value);
                    } else {
                        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                                "%s", "value does not fit in a byte");
                        *error_reason = eFA_Syntax_Error;
                        break;
                    }
                }
            }
            i = skip_spaces(line, n + i);
            n = get_word(numberstring, &(line[i]), ' ', MAX_INSTRUCTION_NAME_LENGTH);
        }
    }
    return address + bytes_needed;
}

static uint16_t dot_w_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    // TODO
    // emit_word(address, value, outpf, listf)
    return 0;
}

static uint16_t dot_l_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    // TODO
    // emit_word(address, value, outpf, listf)
    // emit_word(address, value, outpf, listf)
    return 0;
}

/**
 * Handels:
 * .def  some_symbol  129282
 *
 */

static uint16_t dot_def_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    if (phase != eFA_Scan) {
        /* Do nothing */
    } else {
        unsigned i;
        unsigned n;
        char symbolstring[FA_MAX_SYMBOL_SIZE];
        char numberstring[FA_MAX_WORD_SIZE];

        i = skip_word(line, 0);
        i = skip_spaces(line, i);
        if (line[i] != '\0') {
            n = get_word(symbolstring, &(line[i]), ' ', FA_MAX_SYMBOL_SIZE);
        } else {
            n = 0;
        }

        if (n == 0) {
            /* .def  but no symbol */
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                    ".def needs a symbol name");
            *error_reason = eFA_Syntax_Error;
        } else if (n >= FA_MAX_SYMBOL_SIZE) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                    ".def symbol name is too long");
            *error_reason = eFA_Syntax_Error;
        } else {
            /* Skip symbol name */
            i = skip_word(line, i);
            i = skip_spaces(line, i);
            if (line[i] != '\0') {
                /* Read the number */
                n = get_word(numberstring, &(line[i]), ' ', FA_MAX_WORD_SIZE);
            } else {
                n = 0;
            }
            if (n == 0) {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                        ".def needs a value");
                *error_reason = eFA_Syntax_Error;
            } else if (n == FA_MAX_WORD_SIZE) {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                        "number is too long");
                *error_reason = eFA_Syntax_Error;
            } else {
                bool is_negative;
                int64_t value;
                if (read_number(
                            numberstring, &value, table,
                            error_reason, &is_negative)) {
                    if (value > 65535) {
                        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                                "value is too big");
                        *error_reason = eFA_Syntax_Error;
                    } else {
                        if (value < -32768) {
                            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                                    "value is too small");
                            *error_reason = eFA_Syntax_Error;
                        } else {
                            if (add_symbol(table, symbolstring, value)) {

                            } else {
                                *error_reason = eFA_Syntax_Error;
                            }
                        }
                    }
                } else {
                    snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                            "Malformed number");
                    *error_reason = eFA_Syntax_Error;
                }
            }
        }
    }
    return address;
}



static uint16_t dot_org_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    char numberstring[FA_MAX_WORD_SIZE];
    uint16_t new_address = address;
    unsigned i;
    unsigned n;

    i = skip_word(line, 0);
    i = skip_spaces(line, i);
    n = get_word(numberstring, &(line[i]), ' ', MAX_INSTRUCTION_NAME_LENGTH);
    if (n == 0) {
        /* .b  but no values */
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                ".org needs value");
        *error_reason = eFA_Syntax_Error;
    } else {
        bool is_negative;
        int64_t value;
        if (read_number(numberstring, &value, table, error_reason, &is_negative)) {
            if (is_negative) {
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                        ".org value can not be negative");
                *error_reason = eFA_Syntax_Error;
            } else {
                if (value > 0xFFFF) {
                    snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s",
                            ".org value can not be negative");
                    *error_reason = eFA_AddressError;
                } else {
                    new_address = (uint16_t)value;
                    if (phase == eFA_Assemble) {
                        hex_flush(outpf);
                        hex_set_start_address(new_address);
                    }
                }
            }
        }
    }
    return new_address;
}

static uint16_t dot_align_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address,
    symbol_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    unsigned i;
    uint16_t new_address;
    uint16_t delta;

    i = skip_word(line, 0);
    i = skip_spaces(line, i);

    switch (line[i]) {
        case 'W':
            delta = (address & 0x01) ? (2 - (address & 0x01)) : 0;
            break;
        case 'L':
            delta = (address & 0x03) ? (4 - (address & 0x03)) : 0;
            break;
        default:
            delta = 0;
            snprintf(error_message,
                    MAX_ERROR_MESSAGE_LENGTH, "%s", "Align needs a W or L");
            *error_reason = eFA_Syntax_Error;
    }

    new_address = address + delta;

    if (phase == eFA_Assemble) {
        for (i = 0; i < delta; ++i) {
            hex_push_byte(outpf, 0xAA);
        }
        hex_flush(outpf);
    }

    return new_address;
}

static directive_generator_type directive_generators[] = {
    {".STR",   dot_str_generator},
    {".B",     dot_b_generator},
    {".W",     dot_w_generator},
    {".L",     dot_l_generator},
    {".ALIGN", dot_align_generator},
    {".ORG",   dot_org_generator},
    {".DEF",   dot_def_generator},
    /* Should always be the last one */
    {NULL, NULL}
};

/* --------------------------------------------------------------------*/
/* Parsers */

/**
 * Does the given line contain a label
 *
 * TODO this fails for
 *
 *      .str "fakelabel:"
 */
static bool is_label(const char stripped_line[FA_LINE_BUFFER_SIZE])
{
    bool found_label = false;
    for (const char* c = stripped_line; *c; ++c) {
        if (*c == ':') {
            found_label = true;
        } else if (isspace(*c)) {
            break;
        } else {
            /* Part of the label  */
        }
    }
    return found_label;
}

/**
 * Does the given line contain a directive.
 */
static bool is_directive(const char stripped_line[FA_LINE_BUFFER_SIZE])
{
    return stripped_line[0] == '.';
}

static bool is_instruction(const char stripped_line[FA_LINE_BUFFER_SIZE])
{
    return stripped_line[0] != '\0';
}

/**
 * Replace the ';' with a '\0' to remove remarks from lines
 *
 */
static void remove_remarks(
        char remark_less_line[FA_LINE_BUFFER_SIZE],
        const char line[FA_LINE_BUFFER_SIZE])
{
    unsigned j = 0;
    bool inside_remark = false;

    /* Remove remark by ending the line at the remark marker ';'. This
     * works because remarks always run till the end of a line */
    /* Skips the markers inside a string */
    for (unsigned i = 0; line[i] ; i++) {
        char c = line[i];
        if (inside_remark) {
            if (c == '"') {
                inside_remark = false;
            }
        } else {
            if (c == ';') {
                break;
            } else {
                if (c == '"') {
                    inside_remark = true;
                }
            }
        }
        remark_less_line[j] = line[i];
        j++;
    }
    remark_less_line[j] = '\0';
}

/**
 * Remove leading and trailing spaces from the line.
 * Make everything, except strings, upper-case.
 */
static void strip_line(
        char stripped_line[FA_LINE_BUFFER_SIZE],
        const char line[FA_LINE_BUFFER_SIZE])
{
    bool in_string = false;
    unsigned i = 0;
    unsigned j = 0;

    /* Skip leading space */
    for (; (line[i] != '\0') && isspace(line[i]); ++i);

    for (; line[i] != '\0'; ++i) {
        if (line[i] == '"') {
            in_string = !in_string;
        }
        if (!in_string) {
            stripped_line[j] = toupper(line[i]);
        } else {
            stripped_line[j] = line[i];
        }
        ++j;
    }
    stripped_line[j] = '\0';

    /* Remove trailing spaces */
    if (j > 0) {
        for (--j; isspace(stripped_line[j]); --j) {
            stripped_line[j] = '\0';
        }
    }
}

/**
 *
 */
static bool parse_directive(
    char *line, int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf, FILE* listf,
    symbol_table_type* symbol_table)
{
    bool all_ok = true;
    unsigned n;
    char name[MAX_DIRECTIVE_NAME_LENGTH];

    n = get_word(name, line, ' ', MAX_DIRECTIVE_NAME_LENGTH);

    if (n == MAX_DIRECTIVE_NAME_LENGTH) {
        report_error(eFA_DirectiveTooLong, line, line_no);
        all_ok = false;
    } else {
        unsigned i;
        enum FA_ErrorReason error_reason = eFA_AllOk;
        for (i = 0; directive_generators[i].generator != NULL; i++)
        {
            if (strcmp(name, directive_generators[i].directive_name) == 0) {
                (*address) = directive_generators[i].generator(
                                 line, outpf, listf, *address,
                                 symbol_table, phase, &error_reason);
                all_ok = (error_reason == eFA_AllOk);
                break;
            }
        }
        if (directive_generators[i].generator == NULL) {
            report_error(eFA_Unknown_Directive, line, line_no);
            all_ok = false;
        }
    }
    return all_ok;
}


/**
 * Give a line that contains an instruction, parse
 * the instruction and generate the corresponding code
 */

static bool parse_instruction(
    const char line[FA_LINE_BUFFER_SIZE], int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf,
    FILE* list_outpf,
    symbol_table_type* symbol_table)
{
    bool all_ok = true;
    unsigned n;
    char name[MAX_INSTRUCTION_NAME_LENGTH];

    n = get_word(name, line, ' ', MAX_INSTRUCTION_NAME_LENGTH);

    if (n == MAX_INSTRUCTION_NAME_LENGTH) {
        snprintf(error_message,
                MAX_ERROR_MESSAGE_LENGTH, "%s", "instruction too long");
        all_ok = false;
    } else {
        unsigned i;
        if (phase == eFA_Scan) {
            for (i = 0; instruction_generators[i].generator != NULL; i++)
            {
                char* instr_name = instruction_generators[i].instruction_name;
                if (strcmp(name, instr_name) == 0) {
                    *address += instruction_generators[i].byte_count;
                    break;
                }
            }
        } else {
            enum FA_ErrorReason error_reason = eFA_AllOk;

            for (i = 0; instruction_generators[i].generator != NULL; i++)
            {
                char* instr_name = instruction_generators[i].instruction_name;
                if (strcmp(name, instr_name) == 0) {
                    uint16_t new_address;

                    new_address = instruction_generators[i].generator(
                                      line, outpf, list_outpf,
                                      *address, symbol_table,
                                      &error_reason);

                    if (error_reason != eFA_AllOk) {
                        all_ok = false;
                    } else {
                        uint16_t delta;

                        delta = new_address - (*address);
                        if (delta != instruction_generators[i].byte_count) {
                            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                                     "%s", "internal error");
                            all_ok = false;
                        }
                    }
                    *address = new_address;
                    break;
                }
            }
        }
        if (instruction_generators[i].generator == NULL) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s: %s", "unknown instruction", name);
            all_ok = false;
        }
    }
    return all_ok;
}

/**
 * Read the input file twice.  In the first phase compute the instruction
 * count (how much memory is used) and the location of all labels.
 *
 * In the second phase use this information to generate the code.
 */

static bool assemble(
    FILE* inpf, FILE* outpf, FILE* list_outpf, symbol_table_type* symbol_table)
{
    bool all_ok = true;
    uint16_t address;
    uint32_t line_no;
    char* line = NULL;

    for (enum FA_Phase  phase = eFA_Scan;
         (phase < eFA_NumberOfPhases) && all_ok;
         phase++) {

        rewind(inpf);
        line_no = 1;

        address = 0x0000;
        hex_set_start_address(address);

        line = fgets(line_buffer, FA_LINE_BUFFER_SIZE, inpf);

        while ((line != NULL) && all_ok) {
            remove_remarks(remark_less_line_buffer, line_buffer);
            strip_line(stripped_line_buffer, remark_less_line_buffer);

            if ((list_outpf) && (phase == eFA_Assemble)) {
                fprintf(list_outpf, "%4d %s", line_no, line_buffer);
                if (stripped_line_buffer[0] != '\0') {
                    fprintf(list_outpf, ">\t%s\n", stripped_line_buffer);
                }
            }

            if (is_label(stripped_line_buffer)) {
                if (phase == eFA_Scan) {
                    /* In the first phase we collect label information.  */
                    all_ok = add_label(
                            symbol_table, stripped_line_buffer, address);
                } else {
                    /* In the Assemble phase we double check and use
                     * the collected information.  */
                    all_ok = check_label(
                            symbol_table, stripped_line_buffer, address);
                }
            } else if (is_directive(stripped_line_buffer)) {
                all_ok = parse_directive(
                             stripped_line_buffer, line_no,
                             &address, phase, outpf, list_outpf, symbol_table);
            } else if (is_instruction(stripped_line_buffer)) {
                all_ok = parse_instruction(
                             stripped_line_buffer, line_no,
                             &address, phase, outpf, list_outpf, symbol_table);
            } else {
                /* Empty line, do nothing */
            }

            if (all_ok) {
                /* Get the next line */
                line = fgets(line_buffer, FA_LINE_BUFFER_SIZE, inpf);

                ++line_no;
            } else {
                break;
            }
        }
    }

    if (!all_ok) {
        fprintf(stderr, "Error in line %d\n", line_no);
        fprintf(stderr, "%s\n", line_buffer);
        fprintf(stderr, "%s\n", error_message);
    } else {
        hex_flush(outpf);
        hex_write_end_of_file(outpf);
    }

    return all_ok;
}


static void display_usage(void)
{
    printf("%s",
           "Usage:\n"
           "   fa <sourcefile> [options]\n"
           "     -h             This message\n"
           "     -o <filename>  Output file to use instead of a.hex\n"
           "     -l <filename>  Also produce a listing file\n"
           "     -s <filename>  Also produce a symbol file\n"
          );
}


/**
 *
 */

static void parse_options(
        int argc,
        char** argv,
        char* output_file_name,
        char* listing_file_name,
        char* symbol_file_name)
{
    int c;

    output_file_name[0] = '\0';
    listing_file_name[0] = '\0';
    symbol_file_name[0] = '\0';

    while ((c = getopt(argc, argv, "hl:o:s:")) != EOF) {
        switch (c) {
        case 'h':
            display_usage();
            exit(EXIT_SUCCESS);
            break;
        case 'o':
            strncpy(output_file_name, optarg, FA_MAX_OUTPUT_FILENAME_LENGTH);
            break;
        case 'l':
            strncpy(listing_file_name, optarg, FA_MAX_LISTING_FILENAME_LENGTH);
            break;
        case 's':
            strncpy(symbol_file_name, optarg, FA_MAX_SYMBOL_FILENAME_LENGTH);
            break;
        default:
            break;
        }
    }
}

int main(int argc, char** argv)
{
    FILE* inpf = NULL;
    FILE* outpf = NULL;
    FILE* list_outpf = NULL;

    static symbol_table_type symbol_table;

    if (argc <= 1) {
        fprintf(stderr, "Nothing to assemble\n");
        fprintf(stderr, "Usage: fa <source-file>\n");
    } else {
        static char output_file_name[FA_MAX_OUTPUT_FILENAME_LENGTH + 2];
        static char listing_file_name[FA_MAX_LISTING_FILENAME_LENGTH + 2];
        static char symbol_file_name[FA_MAX_SYMBOL_FILENAME_LENGTH + 2];

        parse_options(
                argc, argv,
                output_file_name, listing_file_name, symbol_file_name);

        /* Parse options puts all non options at the end */
        inpf = fopen(argv[argc-1], "r");

        if (inpf != NULL) {
            bool all_ok = true;

            if (listing_file_name[0] != '\0') {
                list_outpf = fopen(listing_file_name, "w");
                if (list_outpf == NULL) {
                    perror("output fopen:");
                    all_ok = false;
                }
            }

            if (all_ok) {
                if (output_file_name[0] != '\0') {
                    printf("%s\n", output_file_name);
                    outpf = fopen(output_file_name, "w");
                } else {
                    outpf = fopen("./a.hex", "w");
                }
                if (outpf == NULL) {
                    perror("output fopen:");
                    all_ok = false;
                }
            }

            if (all_ok) {
                init_label_table(&symbol_table);
                all_ok = assemble(inpf, outpf, list_outpf, &symbol_table);
                if (all_ok && (symbol_file_name[0] != '\0')) {
                    FILE* sym_outpf = fopen(symbol_file_name, "w");
                    if (sym_outpf == NULL) {
                        perror("Symbol table fopen:");
                    } else {
                        dump_symbol_table(&symbol_table, sym_outpf);
                        fclose(sym_outpf);
                    }
                }
            }

            if (inpf != NULL) { fclose(inpf); }
            if (list_outpf != NULL) { fclose(list_outpf); }
            if (outpf != NULL) { fclose(outpf); }

        } else {
            perror("input fopen:");
        }
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
