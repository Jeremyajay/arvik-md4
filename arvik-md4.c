// Jeremy Cuthbert
// CS333 - Jesse Chaney
// arvik-md4.c

// The purpose of this file is to read and write files. Specifically,
// we will be reading multiple files and writing them into one
// singular archive file.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// Provided arvik header file
#include "arvik.h"

// Prototypes
void print_help(void);

int main(int argc, char *argv[]) {
  int opt = -1;
  int verbose = 0;
  char *filename = NULL;
  var_action_t action = ACTION_NONE;

  while ((opt = getopt(argc, argv, ARVIK_OPTIONS)) != -1) {
    switch (opt) {
    case 'x':
      action = ACTION_EXTRACT;
      break;
    case 'c':
      action = ACTION_CREATE;
      break;
    case 't':
      action = ACTION_TOC;
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
      action = ACTION_VALIDATE;
      break;
    default:
      exit(INVALID_CMD_OPTION);
      break;
    }
  }

  
  
  return EXIT_SUCCESS;
}
