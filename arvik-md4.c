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
  struct stat sb;
  arvik_header_t header;
  arvik_footer_t footer;
  unsigned char header_md4[MD4_DIGEST_LENGTH];
  unsigned char data_md4[MD4_DIGEST_LENGTH];
  char buf[4096];
  ssize_t n;
  int uid_len, gid_len;
  MD4_CTX ctx, ctx_header;
  
  // Open the archive file
  if (archive_name != NULL) {
    afd = open(archive_name,
	       O_WRONLY | O_CREAT | O_TRUNC,
	       S_IRUSR | S_IWUSR);
    if (afd < 0) {
      perror("open archive");
      exit(EXIT_FAILURE);
    }
  }

  // Write ARVIK_TAG
  if (write(afd, ARVIK_TAG, strlen(ARVIK_TAG)) != (ssize_t)strlen(ARVIK_TAG)) {
      perror("write tag");
      close(afd);
      exit(EXIT_FAILURE);
  }

  if (verbose) {
    fprintf(stderr, "Creating archive %s\n", archive_name ? archive_name : "(stdout)");
  }

  // Process each file
  for (int i = 0; i < file_count; i++) {
    char tmp[32];
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

    // Fill header
    memset(&header, 0, sizeof(header));
    snprintf(header.arvik_name, sizeof(header.arvik_name), "%s<", fname);
    snprintf(header.arvik_date, sizeof(header.arvik_date), "%13ld", (long)sb.st_mtime);

    snprintf(tmp, sizeof(tmp), "%ld", (long)sb.st_uid);
    uid_len = strlen(tmp);
    if (uid_len > 5) memcpy(header.arvik_uid, tmp + (uid_len - 5), 5);
    else {
      memset(header.arvik_uid, ' ', 5 - uid_len);
      memcpy(header.arvik_uid + (5 - uid_len), tmp, uid_len);
    }

    snprintf(tmp, sizeof(tmp), "%ld", (long)sb.st_gid);
    gid_len = strlen(tmp);
    if (gid_len > 5) memcpy(header.arvik_gid, tmp + (gid_len - 5), 5);
    else {
      memset(header.arvik_gid, ' ', 5 - gid_len);
      memcpy(header.arvik_gid + (5 - gid_len), tmp, gid_len);
    }

    snprintf(header.arvik_mode, sizeof(header.arvik_mode), "%7o", (unsigned)sb.st_mode);
    snprintf(header.arvik_size, sizeof(header.arvik_size), "%11ld", (long)sb.st_size);
    memcpy(header.arvik_term, ARVIK_TERM, ARVIK_TERM_LEN);

    // Compute md4 of header
    MD4Init(&ctx_header);
    MD4Update(&ctx_header, (const unsigned char *)&header, sizeof(header));
    MD4Final(header_md4, &ctx_header);

    // Write header
    if (write(afd, &header, sizeof(header)) != sizeof(header)) {
      perror("write header");
      close(fd);
      close(afd);
      exit(EXIT_FAILURE);
    }

    // Compute md4 of file data while writing to archive
    MD4Init(&ctx);
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      if (write(afd, buf, n) != n) {
	perror("write file data");
	close(fd);
	close(afd);
	exit(EXIT_FAILURE);
      }
      MD4Update(&ctx, (const unsigned char *)buf, n);
    }
    if (n < 0) {
      perror("read file data");
      close(fd);
      close(afd);
      exit(EXIT_FAILURE);
    }
    MD4Final(data_md4, &ctx);

    // Fill footer
    for (int j = 0; j < MD4_DIGEST_LENGTH; j++) {
      snprintf(footer.md4sum_header + j*2, 3, "%02x", header_md4[j]);
      snprintf(footer.md4sum_data + j*2, 3, "%02x", data_md4[j]);
    }
    memcpy(footer.arvik_term, ARVIK_TERM, ARVIK_TERM_LEN);

    // Write footer
    if (write(afd, &footer, sizeof(footer)) != sizeof(footer)) {
      perror("write footer");
      close(fd);
      close(afd);
      exit(EXIT_FAILURE);
    }

    if (verbose) {
      fprintf(stderr, "Added file: %s (%ld bytes)\n", fname, (long)sb.st_size);
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
