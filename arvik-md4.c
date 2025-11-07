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
#include <md4.h>

// Provided arvik header file
#include "arvik.h"

// Prototypes
void print_help(void);
void create_archive(const char *archive_name, int verbose, char **files, int file_count);
void extract_archive(const char *archive_name, int verbose);
void list_archive(const char *archive_name, int verbose);
void validate_archive(const char *archive_name, int verbose);


int main(int argc, char *argv[]) {
  int opt = -1;
  int verbose = 0;
  char *filename = NULL;
  var_action_t action = ACTION_NONE;

  while ((opt = getopt(argc, argv, ARVIK_OPTIONS)) != -1) {
    switch (opt) {
    case 'x':
      if (action != ACTION_NONE) exit(INVALID_CMD_OPTION);
      action = ACTION_EXTRACT;
      break;
    case 'c':
      if (action != ACTION_NONE) exit(INVALID_CMD_OPTION);
      action = ACTION_CREATE;
      break;
    case 't':
      if (action != ACTION_NONE) exit(INVALID_CMD_OPTION);
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
      if (action != ACTION_NONE) exit(INVALID_CMD_OPTION);
      action = ACTION_VALIDATE;
      break;
    default:
      exit(INVALID_CMD_OPTION);
      break;
    }
  }

  switch (action) {
  case ACTION_CREATE:
    create_archive(filename, verbose, &argv[optind], argc - optind);
    break;
  case ACTION_EXTRACT:
    extract_archive(filename, verbose);
    break;
  case ACTION_TOC:
    list_archive(filename, verbose);
    break;
  case ACTION_VALIDATE:
    validate_archive(filename, verbose);
    break;
  case ACTION_NONE:
    fprintf(stderr, "No action specified.\n");
    exit(NO_ACTION_GIVEN);
  }
  
  return EXIT_SUCCESS;
}


void print_help(void) {
  printf("Usage: arvik-md4 -[cxtvVf:h] archive-file file...\n");
  printf("        -c           create a new archive file\n");
  printf("        -x           extract members from an existing archive file\n");
  printf("        -t           show the table of contents of archive file\n");
  printf("        -f filename  name of archive file to use\n");
  printf("        -V           Validate the md4 values for the header and data\n");
  printf("        -v           verbose output\n");
  printf("        -h           show help text\n");
}


void create_archive(const char *archive_name, int verbose, char **files, int file_count) {
  int afd = STDOUT_FILENO;
  ssize_t written;
  struct stat sb;
  arvik_header_t header;
  
  // Open the archive file, or default to stdout
  if (archive_name != NULL) {
    afd = open(archive_name,
	       O_WRONLY | O_CREAT | O_TRUNC,
	       S_IRUSR | S_IWUSR);
    if (afd < 0) {
      perror("open archive");
      exit(EXIT_FAILURE);
    }
  }

  written = write(afd, ARVIK_TAG, strlen(ARVIK_TAG));
  if (written != (ssize_t)strlen(ARVIK_TAG)) {
    perror("write tag");
    exit(EXIT_FAILURE);
  }

  if (verbose) {
    fprintf(stderr, "Creating archive: %s\n",
	    archive_name ? archive_name : "(stdout)");
  }

  for (int i = 0; i < file_count; i++) {
    const char *fname = files[i];
    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
      perror(fname);
      close(afd);
      exit(EXIT_FAILURE);
    }

    if (fstat(fd, &sb) < 0) {
      perror("fstat");
      close(fd);
      close(afd);
      exit(EXIT_FAILURE);
    }

    memset(&header, 0, sizeof(header));


    if (verbose) {
      fprintf(stderr, "Adding file: %s (%ld bytes)\n",
	      fname, (long)sb.st_size);
    }

    close(fd);
  }

  if (archive_name != NULL) {
    close(afd);
  }
}


void extract_archive(const char *archive_name, int verbose) {
  (void)archive_name;
  (void)verbose;
}


void list_archive(const char *archive_name, int verbose) {
  (void)archive_name;
  (void)verbose;
}


void validate_archive(const char *archive_name, int verbose) {
  (void)archive_name;
  (void)verbose;
}
