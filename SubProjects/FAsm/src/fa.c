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
#define FA_MAX_LABEL_SIZE 32
#define MAX_DIRECTIVE_NAME_LENGTH 32
#define MAX_INSTRUCTION_NAME_LENGTH 32
#define MAX_NUMBER_OF_LABELS 1024
#define MAX_ERROR_MESSAGE_LENGTH 1024
#define FA_MAX_OUTPUT_FILENAME_LENGTH (5*80)
#define FA_MAX_LISTING_FILENAME_LENGTH (5*80)

/* --------------------------------------------------------------------*/
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
    eFA_NumberOfErrors
};
/* --------------------------------------------------------------------*/

typedef struct label_location {
    char label[FA_MAX_LABEL_SIZE];
    uint16_t location;
} label_location_t;

/**
 * Lookup table to match labels to the corresponding
 * memory addresses
 */
typedef struct label_table {
    uint16_t number;
    label_location_t labels[MAX_NUMBER_OF_LABELS];
} label_table_type;

/**
 * TODO
 */
typedef uint16_t instruction_callback_type(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    FILE* listf,
    uint16_t address, label_table_type* table);

typedef uint16_t directive_callback_type(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf,
    uint16_t address, label_table_type* table,
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


static unsigned get_word(
        char word[FA_MAX_WORD_SIZE],
        const char line[FA_LINE_BUFFER_SIZE], char seperator, int max_len);

static void init_label_table(label_table_type* lt);
static bool find_label(label_table_type* lt,
        const char* label, uint16_t *address);
static bool add_label(label_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE], uint16_t address);
static bool check_label(label_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE], uint16_t address);

static uint16_t nop_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, FILE* listf,
    uint16_t address, label_table_type* table);
static uint16_t halt_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, FILE* listf,
    uint16_t address, label_table_type* table);
static uint16_t ldl_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, FILE* listf,
    uint16_t address, label_table_type* table);
static uint16_t lt_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, FILE* listf,
    uint16_t address, label_table_type* table);
static uint16_t bif_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, FILE* listf,
    uint16_t address, label_table_type* table);

static void emit_code(
        FILE* outpf, FILE* listf, uint16_t address, uint16_t instruction);

static bool is_label(const char line[FA_LINE_BUFFER_SIZE]);
static bool is_directive(const char line[FA_LINE_BUFFER_SIZE]);
static bool is_instruction(const char line[FA_LINE_BUFFER_SIZE]);
static void remove_remarks(
        char remark_less_line[FA_LINE_BUFFER_SIZE],
        const char line[FA_LINE_BUFFER_SIZE]);
static void strip_line(
        char stripped_line[FA_LINE_BUFFER_SIZE],
        const char line[FA_LINE_BUFFER_SIZE]);
static bool parse_instruction(
    const char line[FA_LINE_BUFFER_SIZE], int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf,
    FILE* list_outpf,
    label_table_type* label_table);

static void assemble(
        FILE* inpf, FILE* outpf, FILE* list_outpf,
        label_table_type* label_table);

/* --------------------------------------------------------------------*/

static char line_buffer[FA_LINE_BUFFER_SIZE];
static char remark_less_line_buffer[FA_LINE_BUFFER_SIZE];
static char stripped_line_buffer[FA_LINE_BUFFER_SIZE];
static char error_message[MAX_ERROR_MESSAGE_LENGTH];

/* --------------------------------------------------------------------*/


/**
 * Error message table based on FA_ErrorReason
 */

static char* error_reason_text [] = {
    "All is OK",
    "Unknown directive",
    "Syntax Error",
    "Directive is too long",

    "INTERNAL ERROR"
};


// TODO Should take an FA_ErrorReason
static void report_error(enum FA_ErrorReason reason, char* line, int line_no)
{
    snprintf(error_message,
            MAX_ERROR_MESSAGE_LENGTH, "%s", error_reason_text[reason]);
}

/**
 * TODO
 */

static uint16_t get_stack_id(char c)
{
    uint16_t id;
    switch (c) {
    case 'D':
        id = 0x0;
        break;
    case 'C':
        id = 0x1;
        break;
    case 'R':
        id = 0x2;
        break;
    case 'T':
        id = 0x3;
        break;
    default:
        /* Error, unknown stack */
        id = 0xF;
        break;
    }
    return id;
}

static bool read_number(const char *line, uint16_t* value)
{
    // TODO Error check
    *value = strtol(line, NULL, 16);
    return true;
}

static unsigned skip_word(const char line[FA_LINE_BUFFER_SIZE], unsigned n)
{
    unsigned i;
    for (i = n; (line[i] != '\0') && !isspace(line[i]); ++i);
    return i;
}

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
    const char line[FA_LINE_BUFFER_SIZE], char seperator, int max_len)
{
    unsigned i;
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

static void init_label_table(label_table_type* lt)
{
    lt->number = 0;
}

/*
 * Lookup a given label in the look-up table.
 *
 * Returns true if the label was found.
 * The corresponding location is returned in `address`
 */
static bool find_label(
        label_table_type* lt, const char* label, uint16_t *address)
{
    int i;
    int n = lt->number;
    bool found = false;

    for (i = 0; i < n; ++i) {
        if (strcmp(label, lt->labels[i].label) == 0) {
            found = true;
            *address = lt->labels[i].location;
            break;
        }
    }

    return found;
}

/**
 * Add a new label to the label table
 *
 */

static bool add_label(
        label_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE], uint16_t address)
{
    bool is_ok = true;
    unsigned i;
    int n = lt->number;

    for (i = 0; (line[i] != ':') && (i < FA_MAX_LABEL_SIZE); ++i) {
        lt->labels[n].label[i] = line[i];
        lt->labels[n].location = address;
    }

    if (line[i] == ':') {
        lt->labels[n].label[i] = '\0';
        if (lt->number < (MAX_NUMBER_OF_LABELS - 3)) {
            lt->number++;
            // TODO check for duplicate labels
        } else {
            /* Error Table is full */
            is_ok = false;
        }
    } else {
        /* Label too long, or no label found */
        is_ok = false;
    }

    return is_ok;
}


static bool check_label(
        label_table_type* lt,
        const char line[FA_LINE_BUFFER_SIZE], uint16_t address)
{
    bool is_ok = true;
    char label[FA_MAX_LABEL_SIZE];
    unsigned i;

    i = get_word(label, line, ':', FA_MAX_LABEL_SIZE);

    if (i == FA_MAX_LABEL_SIZE) {
        /* Label too long, or no label found */
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "Label too long");
        is_ok = false;
    } else {
        if (line[i] == ':') {
            uint16_t ist_address;
            if (find_label(lt, label, &ist_address)) {
                if (ist_address == address) {
                    /* OK */
                } else {
                    snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                             "%s", "Address computation error");
                    is_ok = false;
                }
            } else {
                /* Label can't be found, but it should be there */
                snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                         "%s", "Can't find label location");
                is_ok = false;
            }
        } else {
            /* No label found on line */
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                     "%s", "No label specified");
            is_ok = false;
        }
    }

    return is_ok;
}

/* --------------------------------------------------------------------*/

static void emit_code(
        FILE* outpf, FILE* listf, uint16_t address, uint16_t instruction)
{
    fprintf(outpf, "%04X %04X\n", address, instruction);
    if (listf) {
        fprintf(listf, ">\t%04X %04X\n", address, instruction);
    }
}

/* Generators */

static uint16_t nop_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    emit_code(outpf, listf, address, 0x8000);
    return address + 2;
}

static uint16_t halt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    emit_code(outpf, listf, address, 0x8200);
    return address + 2;
}

/**
 * LDL d 0x00000
 * LDL r 0x00000
 * LDL c 0x00000
 * LDL t 0x00000
 */
static uint16_t ldl_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    uint16_t new_address = address;
    unsigned i = 0;
    i = skip_word(line, i);
    i = skip_spaces(line, i);

    if (line[i] == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "no destination or value");
    } else {
        uint16_t value;
        char destination = line[i];
        uint16_t bits = get_stack_id(destination);
        if (bits == 0x7) {
            snprintf(error_message,
                    MAX_ERROR_MESSAGE_LENGTH, "%s", "illegal destination");
        } else {
            i = skip_word(line, i);
            i = skip_spaces(line, i);
            if (line[i] != '\0') {
                if (read_number(&(line[i]), &value)) {
                    uint16_t instruction = 0xC000;
                    instruction |= value;
                    instruction |= (bits << 10);
                    emit_code(outpf, listf, address, instruction);
                    new_address += 2;
                } else {
                    snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                             "%s", "syntax error in number");
                }
            }
        }
    }
    return new_address;
}

static uint16_t lt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    emit_code(outpf, listf, address, 0xe290);
    return address + 2;
}

static uint16_t gt_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    emit_code(outpf, listf, address, 0xe310);
    return address + 2;
}

static uint16_t add_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
{
    emit_code(outpf, listf, address, 0xe010);
    return address + 2;
}

static uint16_t dup_generator(
    const char line[FA_LINE_BUFFER_SIZE],
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
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
    FILE* outpf, FILE* listf, uint16_t address, label_table_type* table)
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
        if (find_label(table, &(line[i]), &location)) {
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
    {"ADD",  add_generator,  2},
    {"DUP",  dup_generator,  2},
    {"NOP",  nop_generator,  2},
    {"HALT", halt_generator, 2},
    {"LDL",  ldl_generator,  2},
    {"LT",   lt_generator,   2},
    {"GT",   gt_generator,   2},
    {"BIF",  bif_generator,  2},
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
    FILE* outpf, uint16_t address, label_table_type* table,
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
                uint16_t pair = 0U;
                unsigned m = 0U;
                for (++i ; (line[i] != '\0') && (line[i] != '"'); ++i) {
                    if (m == 0) {
                        pair = line[i];
                        ++m;
                    } else {
                        pair = (pair << 8) | (line[i]);
                        fprintf(outpf, "%04x %04x\n",
                                address + bytes_needed - 1,
                                pair);
                        m = 0;
                    }
                    bytes_needed++;
                }
                if (m == 1) {
                    pair = (pair << 8);
                    fprintf(outpf, "%04x %04x\n",
                            address + bytes_needed - 1,
                            pair);
                    bytes_needed++;
                }
            } else {
                for (++i ; (line[i] != '\0') && (line[i] != '"'); ++i) {
                    bytes_needed++;
                }
                if (bytes_needed & 1) {
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

static uint16_t dot_b_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, uint16_t address,
    label_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    return 0;
}

static uint16_t dot_w_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, uint16_t address,
    label_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    return 0;
}

static uint16_t dot_l_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, uint16_t address,
    label_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    return 0;
}

static uint16_t dot_align_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, uint16_t address,
    label_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    return 0;
}

static uint16_t dot_code_generator(
    const char line[FA_LINE_BUFFER_SIZE], FILE* outpf, uint16_t address,
    label_table_type* table,
    enum FA_Phase  phase, enum FA_ErrorReason* error_reason)
{
    return 0;
}


static directive_generator_type directive_generators[] = {
    {".STR",   dot_str_generator},
    {".B",     dot_b_generator},
    {".W",     dot_w_generator},
    {".L",     dot_l_generator},
    {".ALIGN", dot_align_generator},
    {".CODE",  dot_code_generator},
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
 * Remove leading and trailing spaces from the line
 * Make everything except strings upper-case.
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
    FILE* outpf,
    label_table_type* label_table)
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
        for (i = 0; directive_generators[i].generator != NULL; i++)
        {
            enum FA_ErrorReason error_reason;
            if (strcmp(name, directive_generators[i].directive_name) == 0) {
                (*address) = directive_generators[i].generator(
                                 line, outpf, *address,
                                 label_table, phase, &error_reason);
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
 *
 *
 */

static bool parse_instruction(
    const char line[FA_LINE_BUFFER_SIZE], int line_no,
    uint16_t* address, enum FA_Phase  phase,
    FILE* outpf,
    FILE* list_outpf,
    label_table_type* label_table)
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
            for (i = 0; instruction_generators[i].generator != NULL; i++)
            {
                char* instr_name = instruction_generators[i].instruction_name;
                if (strcmp(name, instr_name) == 0) {
                    uint16_t new_address;
                    uint16_t delta;
                    new_address = instruction_generators[i].generator(
                                      line, outpf, list_outpf,
                                      *address, label_table);
                    delta = new_address - (*address);
                    if (delta != instruction_generators[i].byte_count) {
                        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                                 "%s", "internal error");
                        all_ok = false;
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
 * Read the input file twice.
 * In the first phase compute the instruction count (how much memory is used)
 * and the location of all labels.
 *
 * In the second phase use this information to generate a the code.
 */

static void assemble(
    FILE* inpf, FILE* outpf, FILE* list_outpf, label_table_type* label_table)
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
                printf("label\n");
                if (phase == eFA_Scan) {
                    /* In the first phase we collect label information.  */
                    all_ok = add_label(
                            label_table, stripped_line_buffer, address);
                } else {
                    /* In the Assemble phase we double check and use
                     * the collected information.  */
                    all_ok = check_label(
                            label_table, stripped_line_buffer, address);
                }
            } else if (is_directive(stripped_line_buffer)) {
                all_ok = parse_directive(
                             stripped_line_buffer, line_no,
                             &address, phase, outpf, label_table);
            } else if (is_instruction(stripped_line_buffer)) {
                all_ok = parse_instruction(
                             stripped_line_buffer, line_no,
                             &address, phase, outpf, list_outpf, label_table);
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
        fprintf(stderr, "%s\n", error_message);
        fprintf(stderr, "%s\n", line_buffer);
    }
}


static void display_usage(void)
{
    printf("%s",
           "Usage:\n"
           "   fa <sourcefile> [options]\n"
           "     -h             This message\n"
           "     -o <filename>  Output file to use instead of a.hex\n"
           "     -l <filename>  Also produce a listing file\n"
          );
}


/**
 *
 */

static void parse_options(
        int argc,
        char** argv,
        char* output_file_name,
        char* listing_file_name)
{
    int c;

    output_file_name[0] = '\0';
    listing_file_name[0] = '\0';

    while ((c = getopt(argc, argv, "hl:o:")) != EOF) {
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

    static label_table_type label_table;

    if (argc <= 1) {
        fprintf(stderr, "Nothing to assemble\n");
        fprintf(stderr, "Usage: fa <source-file>\n");
    } else {
        static char output_file_name[FA_MAX_OUTPUT_FILENAME_LENGTH + 2];
        static char listing_file_name[FA_MAX_LISTING_FILENAME_LENGTH + 2];

        parse_options(argc, argv, output_file_name, listing_file_name);

        /* Parse options puts all non options at the end */
        inpf = fopen(argv[argc-1], "r");

        if (inpf != NULL) {
            bool all_ok = true;

            if (listing_file_name[0] != '\0') {
                printf("%s\n", listing_file_name);
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
                init_label_table(&label_table);
                assemble(inpf, outpf, list_outpf, &label_table);
            }

            if (inpf != NULL) {
                fclose(inpf);
            }

            if (list_outpf != NULL) {
                fclose(list_outpf);
            }

            if (outpf != NULL) {
                fclose(outpf);
            }

        } else {
            perror("input fopen:");
        }
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
