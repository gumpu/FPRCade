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

#define WORD_BUFFER_SIZE 1024
#define LINE_BUFFER_SIZE 1024
#define MAX_LABEL_SIZE 32
#define MAX_INSTRUCTION_NAME_LENGTH 32
#define MAX_NUMBER_OF_LABELS 1024
#define MAX_ERROR_MESSAGE_LENGTH 1024
#define FA_MAX_OUTPUT_FILENAME_LENGTH 80

/* --------------------------------------------------------------------*/

typedef struct label_location {
    char label[MAX_LABEL_SIZE];
    uint16_t location;
} label_location_t;

/**
 * Lookup table to match labels to the corresponding
 * memory addresses
 */
typedef struct label_table {
    uint16_t number;
    label_location_t labels[MAX_NUMBER_OF_LABELS];
} label_table_t;

typedef uint16_t (*callback_t)(
        char* line, FILE* outpf, uint16_t address,
        label_table_t* table);

/*
 *
 */
typedef struct instruction_generator {
    char* instruction_name;
    callback_t generator;
    uint16_t byte_count;
} instruction_generator_t;


/* --------------------------------------------------------------------*/

static int get_word(char* word, char* line, char seperator, int max_len);
static void init_label_table(label_table_t* lt);
static bool find_label(label_table_t* lt,  char* label, uint16_t *address);
static bool add_label(label_table_t* lt, char* line, uint16_t address);
static bool check_label(label_table_t* lt, char* line, uint16_t address);
static uint16_t nop_generator(
    char* line, FILE* outpf, uint16_t address, label_table_t* table);
static uint16_t halt_generator(
    char* line, FILE* outpf, uint16_t address, label_table_t* table);
static uint16_t ldl_generator(
    char* line, FILE* outpf, uint16_t address, label_table_t* table);
static uint16_t lt_generator(
    char* line, FILE* outpf, uint16_t address, label_table_t* table);
static uint16_t bif_generator(
    char* line, FILE* outpf, uint16_t address, label_table_t* table);
static bool is_label(const char* line);
static bool is_directive(char* line);
static bool is_instruction(char* line);
static char* remove_remarks(char *line);
static char* strip_line(char *line);
static bool parse_instruction(
    char *line, int line_no,
    uint16_t* address, uint16_t fase,
    FILE* outpf,
    label_table_t* label_table);
static void assemble(
    FILE* inpf, FILE* outpf, label_table_t* label_table);

/* --------------------------------------------------------------------*/

static char line_buffer[LINE_BUFFER_SIZE];
static char error_message[MAX_ERROR_MESSAGE_LENGTH];

/* --------------------------------------------------------------------*/

/**
 *
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

static bool read_number(char *line, uint16_t* value)
{
    // TODO Error check
    *value = strtol(line, NULL, 16);
    return true;
}

static char* skip_word(char *line)
{
    char *c;
    for (c = line; (*c != '\0') && !isspace(*c); c++);
    return c;
}

static char* skip_spaces(char *line)
{
    char *c;
    c = line;
    if (*c != '\0') {
        for (; isspace(*c); c++);
    } else {
        /* Do not go past the end of a line */
    }
    return c;
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

static int get_word(char* word, char* line, char seperator, int max_len)
{
    int i;
    for (i = 0; i < max_len; ++i) {
        char x = line[i];

        if (x == seperator) {
            word[i] = '\0'; break;
        } else if (x == '\0') {
            word[i] = '\0'; break;
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

static void init_label_table(label_table_t* lt)
{
    lt->number = 0;
}

/*
 * Lookup a given label in the look-up table.
 *
 * Returns true if the label was found.
 * The corresponding location is returned in `address`
 */
static bool find_label(label_table_t* lt,  char* label, uint16_t *address)
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

static bool add_label(label_table_t* lt, char* line, uint16_t address)
{
    bool is_ok = true;
    int i;
    int n = lt->number;

    for (i = 0; (line[i] != ':') && (i < MAX_LABEL_SIZE); ++i) {
        lt->labels[n].label[i] = line[i];
        lt->labels[n].location = address;
    }

    if (line[i] == ':') {
        lt->labels[n].label[i] = '\0';
        if (lt->number < (MAX_NUMBER_OF_LABELS - 3)) {
            lt->number++;
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


static bool check_label(label_table_t* lt, char* line, uint16_t address)
{
    bool is_ok = true;
    char label[MAX_LABEL_SIZE];
    int i;

    i = get_word(label, line, ':', MAX_LABEL_SIZE);

    if (i == MAX_LABEL_SIZE) {
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

/* Generators */

static uint16_t nop_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    fprintf(outpf, "%04x 8000 ; %s\n", address, line);
    return address + 2;
}

static uint16_t halt_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    fprintf(outpf, "%04x 8200 ; %s\n", address, line);
    return address + 2;
}

/*
 * LDL d 0x00000
 * LDL r 0x00000
 * LDL c 0x00000
 * LDL t 0x00000
 */
static uint16_t ldl_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    uint16_t new_address = address;
    char* c = line;
    c = skip_word(c);
    c = skip_spaces(c);
    if (*c == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "no destination or value");
    } else {
        uint16_t value;
        char destination = *c;
        uint16_t bits = get_stack_id(destination);
        if (bits == 0x7) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s", "illegal destination");
        } else {
            c = skip_word(c);
            c = skip_spaces(c);
            if (*c != '\0') {
                if (read_number(c, &value)) {
                    uint16_t instruction = 0xC000;
                    instruction |= value;
                    instruction |= (bits << 10);
                    fprintf(outpf, "%04x %04x ; %s\n",
                            address, instruction, line);
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
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    fprintf(outpf, "%04x e290 ; %s\n", address, line);
    return address + 2;
}

static uint16_t gt_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    fprintf(outpf, "%04x e310 ; %s\n", address, line);
    return address + 2;
}



static uint16_t add_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    fprintf(outpf, "%04x e010 ; %s\n", address, line);
    return address + 2;
}

static uint16_t dup_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    uint16_t new_address = address;
    char* c = line;
    c = skip_word(c);
    c = skip_spaces(c);
    if (*c == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "no destination");
    } else {
        char destination = *c;
        uint16_t bits = get_stack_id(destination);
        if (bits == 0x7) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s", "illegal destination");
        } else {
            uint16_t instruction = 0xB100;
            instruction |= (bits << 6);
            fprintf(outpf, "%04x %04x ; %s\n",
                    address, instruction, line);

            new_address += 2;
        }
    }
    return new_address;
}

static uint16_t bif_generator(
        char* line, FILE* outpf, uint16_t address, label_table_t* table)
{
    uint16_t new_address = address;
    char* c = line;
    c = skip_word(c);
    c = skip_spaces(c);
    if (*c == '\0') {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "missing label");
    } else {
        uint16_t location;
        if (find_label(table, c, &location)) {
            uint16_t instruction;
            int16_t  delta;
            instruction = 0x1000;
            delta = location - new_address;
            instruction |= delta & 0x0FFF;
            fprintf(outpf, "%04x %04x ; %s  (%04x)\n",
                    new_address, instruction, line, location);
            new_address += 2;
        } else {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s", "label not found");
        }
    }
    return new_address;
}

static instruction_generator_t generators[] = {
    {"ADD", add_generator, 2},
    {"DUP", dup_generator, 2},
    {"NOP", nop_generator, 2},
    {"HALT", halt_generator, 2},
    {"LDL", ldl_generator, 2},
    {"LT", lt_generator, 2},
    {"GT", gt_generator, 2},
    {"BIF", bif_generator, 2},
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
 *
 */
static bool is_label(const char* stripped_line)
{
    return strchr(stripped_line, ':');
}

/**
 * Does the given line contain a directive.
 */
static bool is_directive(char* stripped_line)
{
    return stripped_line[0] == '.';
}

static bool is_instruction(char* stripped_line)
{
    return stripped_line[0] != '\0';
}

/**
 * Replace the ';' with a '\0' to remove remarks from lines
 *
 * TODO this fails for
 *
 *      .str ";"
 *
 */
static char* remove_remarks(char *line)
{
    char* c;
    for (c = line; *c != '\0'; c++) {
        if (*c == ';') {
            /* Remove remark by ending the line at the remark marker. This
             * works because remarks always run till the end of a line */
            *c = '\0';
            break;
        }
    }
    return line;
}

/**
 * Remove leading and trailing spaces from the line
 * Make everything except strings upper-case.
 */
static char* strip_line(char *line)
{
    char *src;
    char *dest;
    bool non_space_found = false;
    bool in_string = false;
    dest = line;
    for (src = line; *src != '\0'; ++src) {
        if (!isspace(*src)) { non_space_found = true; }
        if (*src == '"') { in_string = !in_string; }
        if (non_space_found) {
            if (!in_string) {
                *dest = toupper(*src); }
            ++dest;
        }
    }
    *dest = '\0';
    for (--dest; dest != line; --dest) {
        if (isspace(*dest)) { *dest = '\0'; } else { break; }
    }
    return line;
}

/*
 *
 *
 */

static bool parse_instruction(
        char *line, int line_no,
        uint16_t* address, uint16_t fase,
        FILE* outpf,
        label_table_t* label_table)
{
    bool all_ok = true;
    int i;
    char name[MAX_INSTRUCTION_NAME_LENGTH];

    i = get_word(name, line, ' ', MAX_INSTRUCTION_NAME_LENGTH);

    if (i == MAX_INSTRUCTION_NAME_LENGTH) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                 "%s", "instruction too long");
        all_ok = false;
    } else {
        if (fase == 0) {
            for (i = 0; generators[i].generator != NULL; i++)
            {
                if (strcmp(name, generators[i].instruction_name) == 0) {
                    *address += generators[i].byte_count;
                    break;
                }
            }
        } else {
            printf("[ %s ]\n", name);
            for (i = 0; generators[i].generator != NULL; i++)
            {
                if (strcmp(name, generators[i].instruction_name) == 0) {
                    uint16_t new_address;
                    uint16_t delta;
                    new_address = generators[i].generator(
                            line, outpf, *address, label_table);
                    delta = new_address - (*address);
                    if (delta != generators[i].byte_count) {
                        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                                "%s", "internal error");
                        all_ok = false;
                    }
                    *address = new_address;
                    break;
                }
            }
        }
        if (generators[i].generator == NULL) {
            snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH,
                    "%s: %s", "unknown instruction", name);
            all_ok = false;
        }
    }
    return all_ok;
}

/**
 * Read the input file twice.
 * In the first fase compute the instruction count (how much memory is used) and
 * the location of all labels.
 *
 * In the second fase use this information to generate a the code.
 */

static void assemble(
        FILE* inpf, FILE* outpf, label_table_t* label_table)
{
    bool all_ok = true;
    uint16_t address;
    uint32_t line_no;
    uint16_t fase;
    char* line = NULL;

    for (fase = 0; (fase < 2) && all_ok; fase++) {

        rewind(inpf);
        line_no = 1;
        address = 0x0000;
        line = fgets(line_buffer, LINE_BUFFER_SIZE, inpf);

        while ((line != NULL) && all_ok) {
            line = remove_remarks(line);
            line = strip_line(line);

            if (is_label(line)) {
                printf("%s\n", line);
                if (fase == 0) {
                    /* In the first fase we collect label information.  */
                    all_ok = add_label(label_table, line, address);
                } else {
                    /* In the second fase we double check and use
                     * the collected information.  */
                    all_ok = check_label(label_table, line, address);
                }
            } else if (is_directive(line)) {
                // printf("       <-- is directive\n");
            } else if (is_instruction(line)) {
                // printf("       <-- is instruction\n");
                // printf("0x%04x ", address);
                all_ok = parse_instruction(
                        line, line_no,
                        &address, fase, outpf,
                        label_table);
            } else {
                /* Empty line, do nothing */
            }

            if (all_ok) {
                /* Get the next line */
                line = fgets(line_buffer, LINE_BUFFER_SIZE, inpf);

                ++line_no;
            } else {
                break;
            }
        }
    }

    if (!all_ok) {
        fprintf(stderr, "Error in line %d\n", line_no);
        fprintf(stderr, "%s\n", error_message);
        fprintf(stderr, "%s\n", line);
    }
}


static void display_usage(void)
{
    printf("%s",
           "Usage:\n"
           "   fa <sourcefile> [options]\n"
           "     -h             This message\n"
           "     -o <filename>  Output file to use instead of a.hex\n"
          );
}



static void parse_options(int argc, char** argv, char* output_file_name)
{
    int c;
    output_file_name[0] = '\0';
    while ((c = getopt(argc, argv, "ho:")) != EOF) {
        switch (c) {
            case 'h':
                display_usage();
                exit(EXIT_SUCCESS);
                break;
            case 'o':
                strncpy(output_file_name, optarg, FA_MAX_OUTPUT_FILENAME_LENGTH);
                break;
            default:
                break;
        }
    }
}

int main(int argc, char** argv)
{
    FILE* inpf;
    FILE* outpf;
    static label_table_t label_table;
    if (argc > 1) {
        char output_file_name[FA_MAX_OUTPUT_FILENAME_LENGTH];
        parse_options(argc, argv, output_file_name);
        /* Parse options puts all non options at the end */
        inpf = fopen(argv[argc-1], "r");
        if (inpf != NULL) {
            if (output_file_name[0] != '\0') {
                printf("%s\n", output_file_name);
                outpf = fopen(output_file_name, "w");
            } else {
                outpf = fopen("a.hex", "w");
            }
            if (outpf != NULL) {
                init_label_table(&label_table);
                assemble(inpf, outpf, &label_table);
                fclose(outpf);
            } else {
                perror("output fopen:");
            }
            fclose(inpf);
        } else {
            perror("input fopen:");
        }
    } else {
        fprintf(stderr, "Nothing to assemble\n");
        fprintf(stderr, "Usage: fa <source-file>\n");
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
