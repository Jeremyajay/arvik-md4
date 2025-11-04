// Jeremy Cuthbert
// CS333 - Jesse Chaney
// arvik-md4.c

// The purpose of this file is to read and write files. Specifically,
// we will be reading multiple files and writing them into one
// singular archive file.

#include <stdio.h>
#include <stdlib.h>
#include "arvik.h"

// Prototypes
void print_help(void);

int main(int argc, char *argv[]) {
  int opt = -1;
  int extract_flag = 0, create_flag = 0, toc_flag = 0, validate_flag = 0;
  int verbose = 0;
  char *filename = NULL;
  

  while ((opt = getopt(argc, argv, ARVIK_OPTIONS)) != -1) {
    switch (opt) {
    case 'x':
      extract_flag = 1;
      break;
    case 'c':
      create_flag = 1;
      break;
    case 't':
      toc_flag = 1;
      break;
    case 'f':
      filename = optarg;
      break;
    case 'h':
      print_help();
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'V':
      validate_flag = 1;
      break;
    default:
      exit(INVALID_CMD_OPTION);
      break;
    }
  }

  
  
  return EXIT_SUCCESS;
}
