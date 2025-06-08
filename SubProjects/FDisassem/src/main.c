/**
 *
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include "fdisa.h"

#define FDA_MAX_CODE_LENGTH (80)
#define FDA_MAX_FILENAME_LEN (180)

static void display_usage()
{
}

static void parse_options(
        int argc, char** argv,
        char* input_file_name)
{
    char c;
    input_file_name[0] = '\0';

    while ((c = getopt(argc, argv, "hi:o:r:")) != EOF) {
        switch (c) {
            case 'h':
                display_usage();
                exit(EXIT_SUCCESS);
                break;
            default:
                break;

        }
    }
}


int main(int argc, char** argv)
{
    static char input_file_name[FDA_MAX_FILENAME_LEN + 2];
    static char code[FDA_MAX_CODE_LENGTH];

    parse_options(argc, argv, input_file_name);
    for (uint32_t i = 0x0000; i < 0xFFFF; ++i) {
        disassemble((uint16_t)i, code, 0x8000);
        printf("%04x %04x %s\n", 0x8000, (uint16_t)i, code);
    }

    return EXIT_SUCCESS;
}

