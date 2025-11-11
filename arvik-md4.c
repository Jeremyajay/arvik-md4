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
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <md4.h>

// Provided arvik header file
#include "arvik.h"

// Prototypes
void print_help(void);
void create_archive(const char *archive_name, int verbose, char **files, int file_count);
void extract_archive(const char *archive_name, int verbose);
void list_archive(const char *archive_name, int verbose);
void validate_archive(const char *archive_name, int verbose);
static void format_mode(mode_t mode, char *buf);


int main(int argc, char *argv[]) {
  int opt = -1;
  int verbose = 0;
  char *filename = NULL;
  var_action_t action = ACTION_NONE;

  while ((opt = getopt(argc, argv, ARVIK_OPTIONS)) != -1) {
    switch (opt) {
    case 'x':
    case 'c':
    case 't':
    case 'V':
        if (action != ACTION_NONE) {
            fprintf(stderr, "Only one action (-c, -x, -t, -V) allowed.\n");
            exit(INVALID_CMD_OPTION);
        }
        if (opt == 'x') action = ACTION_EXTRACT;
        else if (opt == 'c') action = ACTION_CREATE;
        else if (opt == 't') action = ACTION_TOC;
        else if (opt == 'V') action = ACTION_VALIDATE;
        break;

    case 'f':
        if (optarg == NULL) {
            fprintf(stderr, "Option -f requires a filename.\n");
            exit(INVALID_CMD_OPTION);
        }
        filename = optarg;
        break;

    case 'v':
        verbose = 1;
        break;

    case 'h':
        print_help();
        exit(EXIT_SUCCESS);

    default:
        fprintf(stderr, "Invalid option.\n");
        exit(INVALID_CMD_OPTION);
    }
  }


  for (int i = optind; i < argc; ++i) {
    if (argv[i] != NULL && argv[i][0] == '-') {
        fprintf(stderr, "Invalid combination of options / stray option '%s'.\n", argv[i]);
        exit(INVALID_CMD_OPTION);
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


static void format_mode(mode_t mode, char *buf) {
  buf[0] = (mode & S_IRUSR) ? 'r' : '-';
  buf[1] = (mode & S_IWUSR) ? 'w' : '-';
  buf[2] = (mode & S_IXUSR) ? 'x' : '-';
  buf[3] = (mode & S_IRGRP) ? 'r' : '-';
  buf[4] = (mode & S_IWGRP) ? 'w' : '-';
  buf[5] = (mode & S_IXGRP) ? 'x' : '-';
  buf[6] = (mode & S_IROTH) ? 'r' : '-';
  buf[7] = (mode & S_IWOTH) ? 'w' : '-';
  buf[8] = (mode & S_IXOTH) ? 'x' : '-';
  buf[9] = '\0';
}


void create_archive(const char *archive_name, int verbose,
                    char **files, int file_count)
{
    int afd = STDOUT_FILENO;
    struct stat sb;
    arvik_header_t header;
    arvik_footer_t footer;
    unsigned char header_md4[MD4_DIGEST_LENGTH];
    unsigned char data_md4[MD4_DIGEST_LENGTH];
    MD4_CTX ctx_header;
    MD4_CTX ctx_data;
    char buf[4096];
    char fbuf[64];
    ssize_t n;
    int i, j, fd;
    int fname_len, len, copy;
    int file_exists;
    mode_t old_umask;
    off_t remaining;

    /* open archive */
    if (archive_name != NULL) {
        file_exists = (access(archive_name, F_OK) == 0);
        old_umask = umask(0);
        afd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        umask(old_umask);
        if (afd < 0) { perror("open archive"); exit(CREATE_FAIL); }
        if (!file_exists) fchmod(afd, 0664);
    }

    /* tag */
    if (write(afd, ARVIK_TAG, strlen(ARVIK_TAG)) != (ssize_t)strlen(ARVIK_TAG)) {
        perror("write tag"); exit(CREATE_FAIL);
    }

    if (verbose)
        fprintf(stderr, "Creating archive %s\n",
                archive_name ? archive_name : "(stdout)");

    for (i = 0; i < file_count; i++) {

        fd = open(files[i], O_RDONLY);
        if (fd < 0) { perror(files[i]); exit(CREATE_FAIL); }

        if (fstat(fd, &sb) < 0) {
            perror("fstat"); close(fd); exit(CREATE_FAIL);
        }

        memset(&header, ' ', sizeof(header));

        /* name */
        fname_len = strlen(files[i]);
        if (fname_len >= ARVIK_NAME_LEN - 1) {
            memcpy(header.arvik_name, files[i], ARVIK_NAME_LEN - 1);
            header.arvik_name[ARVIK_NAME_LEN - 1] = ARVIK_NAME_TERM;
        } else {
            memcpy(header.arvik_name, files[i], fname_len);
            header.arvik_name[fname_len] = ARVIK_NAME_TERM;
        }

        /* date */
        snprintf(fbuf, sizeof(fbuf), "%ld", (long)sb.st_mtime);
        len = strlen(fbuf);
        copy = (len < ARVIK_DATE_LEN - 1) ? len : (ARVIK_DATE_LEN - 1);
        memcpy(header.arvik_date, fbuf, copy);
        if (copy < ARVIK_DATE_LEN - 1)
            memset(header.arvik_date + copy, ' ', (ARVIK_DATE_LEN - 1) - copy);
        header.arvik_date[ARVIK_DATE_LEN - 1] = ' ';

        /* uid */
        snprintf(fbuf, sizeof(fbuf), "%ld", (long)sb.st_uid);
        len = strlen(fbuf);
        copy = (len < ARVIK_UID_LEN - 1) ? len : (ARVIK_UID_LEN - 1);
        memcpy(header.arvik_uid, fbuf, copy);
        if (copy < ARVIK_UID_LEN - 1)
            memset(header.arvik_uid + copy, ' ', (ARVIK_UID_LEN - 1) - copy);
        header.arvik_uid[ARVIK_UID_LEN - 1] = ' ';

        /* gid */
        snprintf(fbuf, sizeof(fbuf), "%ld", (long)sb.st_gid);
        len = strlen(fbuf);
        copy = (len < ARVIK_GID_LEN - 1) ? len : (ARVIK_GID_LEN - 1);
        memcpy(header.arvik_gid, fbuf, copy);
        if (copy < ARVIK_GID_LEN - 1)
            memset(header.arvik_gid + copy, ' ', (ARVIK_GID_LEN - 1) - copy);
        header.arvik_gid[ARVIK_GID_LEN - 1] = ' ';

        /* mode */
        snprintf(fbuf, sizeof(fbuf), "%o", (unsigned)sb.st_mode);
        len = strlen(fbuf);
        copy = (len < ARVIK_MODE_LEN - 1) ? len : (ARVIK_MODE_LEN - 1);
        memcpy(header.arvik_mode, fbuf, copy);
        if (copy < ARVIK_MODE_LEN - 1)
            memset(header.arvik_mode + copy, ' ', (ARVIK_MODE_LEN - 1) - copy);
        header.arvik_mode[ARVIK_MODE_LEN - 1] = ' ';

        /* size */
        snprintf(fbuf, sizeof(fbuf), "%ld", (long)sb.st_size);
        len = strlen(fbuf);
        copy = (len < ARVIK_SIZE_LEN - 1) ? len : (ARVIK_SIZE_LEN - 1);
        memcpy(header.arvik_size, fbuf, copy);
        if (copy < ARVIK_SIZE_LEN - 1)
            memset(header.arvik_size + copy, ' ', (ARVIK_SIZE_LEN - 1) - copy);
        header.arvik_size[ARVIK_SIZE_LEN - 1] = ' ';

        memcpy(header.arvik_term, ARVIK_TERM, ARVIK_TERM_LEN);

        /* header MD4 */
        MD4Init(&ctx_header);
        MD4Update(&ctx_header, (unsigned char *)&header, sizeof(header));
        MD4Final(header_md4, &ctx_header);

        if (write(afd, &header, sizeof(header)) != sizeof(header)) {
            perror("write header"); close(fd); exit(CREATE_FAIL);
        }

        /* file data + MD4 */
        MD4Init(&ctx_data);
        remaining = sb.st_size;

        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (write(afd, buf, n) != n) {
                perror("write data"); close(fd); exit(CREATE_FAIL);
            }
            MD4Update(&ctx_data, (unsigned char *)buf, n);
            remaining -= n;
        }

        if (n < 0) { perror("read data"); close(fd); exit(CREATE_FAIL); }

        MD4Final(data_md4, &ctx_data);

        /* padding */
        if ((sb.st_size % 2) == 1) {
            char pad = '\n';
            if (write(afd, &pad, 1) != 1) {
                perror("write padding"); close(fd); exit(CREATE_FAIL);
            }
        }

        /* footer */
        for (j = 0; j < MD4_DIGEST_LENGTH; j++) {
	  snprintf(fbuf, sizeof(fbuf), "%02x", header_md4[j]);
	  footer.md4sum_header[j*2] = fbuf[0];
	  footer.md4sum_header[j*2 + 1] = fbuf[1];

	  snprintf(fbuf, sizeof(fbuf), "%02x", data_md4[j]);
	  footer.md4sum_data[j*2] = fbuf[0];
	  footer.md4sum_data[j*2 + 1] = fbuf[1];
        }
        memcpy(footer.arvik_term, ARVIK_TERM, ARVIK_TERM_LEN);

        if (write(afd, &footer, sizeof(footer)) != sizeof(footer)) {
            perror("write footer"); close(fd); exit(CREATE_FAIL);
        }

        if (verbose)
            fprintf(stderr, "a %s\n", files[i]);

        close(fd);
    }

    if (archive_name != NULL)
        close(afd);
}


void extract_archive(const char *archive_name, int verbose)
{
    int afd = STDIN_FILENO;
    arvik_header_t header;
    arvik_footer_t footer;
    char tagbuf[sizeof(ARVIK_TAG)];
    ssize_t n;

    /* open archive */
    if (archive_name != NULL) {
        afd = open(archive_name, O_RDONLY);
        if (afd < 0) { perror("open archive"); exit(EXTRACT_FAIL); }
    }

    /* verify ARVIK_TAG */
    n = read(afd, tagbuf, (ssize_t)strlen(ARVIK_TAG));
    if (n != (ssize_t)strlen(ARVIK_TAG) ||
        memcmp(tagbuf, ARVIK_TAG, strlen(ARVIK_TAG)) != 0) {
        fprintf(stderr, "Invalid archive format (bad tag).\n");
        if (archive_name) close(afd);
        exit(BAD_TAG);
    }

    /* read first header */
    n = read(afd, &header, (ssize_t)sizeof(header));
    while (n == (ssize_t)sizeof(header)) {
        char fname[ARVIK_NAME_LEN + 1];
        char *lt;
        long fsize_long;
        off_t fsize;
        int outfd;
        unsigned char buf[4096];
        ssize_t r;
        size_t chunk;
        mode_t mode;
        struct timespec ts[2];

        /* check header terminator */
        if (memcmp(header.arvik_term, ARVIK_TERM, ARVIK_TERM_LEN) != 0) {
            fprintf(stderr, "Bad header terminator.\n");
            if (archive_name) close(afd);
            exit(READ_FAIL);
        }

        /* extract filename */
        memset(fname, 0, sizeof(fname));
        memcpy(fname, header.arvik_name, ARVIK_NAME_LEN);
        lt = memchr(fname, ARVIK_NAME_TERM, ARVIK_NAME_LEN);
        if (lt) *lt = '\0';

        /* parse file size */
        errno = 0;
        fsize_long = strtol(header.arvik_size, NULL, 10);
        if (errno != 0 || fsize_long < 0) {
            fprintf(stderr, "Invalid file size for '%s'.\n", fname);
            if (archive_name) close(afd);
            exit(READ_FAIL);
        }
        fsize = (off_t)fsize_long;

        /* open output file */
        outfd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) { perror(fname); if (archive_name) close(afd); exit(EXTRACT_FAIL); }

        /* copy data */
        while (fsize > 0) {
            chunk = (fsize > (off_t)sizeof(buf)) ? sizeof(buf) : (size_t)fsize;
            r = read(afd, buf, (ssize_t)chunk);
            if (r <= 0) { perror("read data"); close(outfd); if (archive_name) close(afd); exit(READ_FAIL); }
            if (write(outfd, buf, r) != r) { perror("write output"); close(outfd); if (archive_name) close(afd); exit(EXTRACT_FAIL); }
            fsize -= r;
        }

        /* consume padding if odd */
        if (fsize_long % 2 == 1) {
            char pad;
            r = read(afd, &pad, 1);
            if (r != 1) { perror("read padding"); close(outfd); if (archive_name) close(afd); exit(READ_FAIL); }
        }

        /* read footer */
        r = read(afd, &footer, (ssize_t)sizeof(footer));
        if (r != (ssize_t)sizeof(footer)) {
            fprintf(stderr, "Missing footer for '%s'.\n", fname);
            close(outfd);
            if (archive_name) close(afd);
            exit(READ_FAIL);
        }

        /* restore mode */
        mode = (mode_t)strtol(header.arvik_mode, NULL, 8);
        fchmod(outfd, mode);

        /* restore timestamps */
        ts[0].tv_sec = (time_t)strtol(header.arvik_date, NULL, 10);
        ts[0].tv_nsec = 0;
        ts[1].tv_sec = ts[0].tv_sec;
        ts[1].tv_nsec = 0;
        futimens(outfd, ts);

        if (verbose) fprintf(stderr, "x %s\n", fname);

        close(outfd);

        /* read next header */
        n = read(afd, &header, (ssize_t)sizeof(header));
    }

    if (n < 0) { perror("read header"); if (archive_name) close(afd); exit(READ_FAIL); }

    if (archive_name) close(afd);
}



void list_archive(const char *archive_name, int verbose) {
  int afd = STDIN_FILENO;
  arvik_header_t header;
  arvik_footer_t footer;
  char tagbuf[sizeof(ARVIK_TAG)];
  ssize_t n;
  char fname[ARVIK_NAME_LEN + 1];
  char *lt;
  long fsize;
  time_t t;
  struct tm *tm;
  char tbuf[64];
  off_t skip;
  mode_t mode;
  char mode_str[10];
  uid_t uid;
  gid_t gid;
  struct passwd *pw;
  struct group *gr;

  /* Open archive for reading */
  if (archive_name != NULL) {
    afd = open(archive_name, O_RDONLY);
    if (afd < 0) {
      perror("open archive");
      exit(TOC_FAIL);
    }
  }

  /* Read/Verify ARVIK_TAG */
  n = read(afd, tagbuf, strlen(ARVIK_TAG));
  if (n != (ssize_t)strlen(ARVIK_TAG) ||
      memcmp(tagbuf, ARVIK_TAG, strlen(ARVIK_TAG)) != 0) {
    fprintf(stderr, "Invalid archive format.\n");
    exit(BAD_TAG);
  }

  /* Read first header */
  n = read(afd, &header, sizeof(header));

  while (n == sizeof(header)) {
    memset(fname, 0, sizeof(fname));
    strncpy(fname, header.arvik_name, ARVIK_NAME_LEN);
    lt = strchr(fname, ARVIK_NAME_TERM);
    if (lt) *lt = '\0';

    fsize = strtol(header.arvik_size, NULL, 10);

    if (!verbose) {
      printf("%s\n", fname);
    } else {
      /* Long listing format with detailed info */
      mode = strtol(header.arvik_mode, NULL, 8);
      uid = strtol(header.arvik_uid, NULL, 10);
      gid = strtol(header.arvik_gid, NULL, 10);
      t = strtol(header.arvik_date, NULL, 10);
      tm = localtime(&t);
      strftime(tbuf, sizeof(tbuf), "%b %e %R %Y", tm);
      
      format_mode(mode, mode_str);
      
      pw = getpwuid(uid);
      gr = getgrgid(gid);
      
      printf("file name: %s\n", fname);
      printf("    mode:       %s\n", mode_str);
      printf("    uid:         %8ld  %s\n", (long)uid, pw ? pw->pw_name : "");
      printf("    gid:         %8ld  %s\n", (long)gid, gr ? gr->gr_name : "");
      printf("    size:        %8ld  bytes\n", fsize);
      printf("    mtime:      %s\n", tbuf);
      
      /* Read footer to get MD4 values */
      skip = fsize + (fsize % 2 == 1 ? 1 : 0);
      if (lseek(afd, skip, SEEK_CUR) < 0) {
        perror("lseek");
        exit(TOC_FAIL);
      }
      
      if (read(afd, &footer, sizeof(footer)) != sizeof(footer)) {
        if (archive_name) close(afd);
        return;
      }
      
      printf("    header md4: %.32s\n", footer.md4sum_header);
      printf("    data md4:   %.32s\n", footer.md4sum_data);
      
      /* Read next header */
      n = read(afd, &header, sizeof(header));
      continue;
    }

    /* Skip file data and padding */
    skip = fsize + (fsize % 2 == 1 ? 1 : 0);
    if (lseek(afd, skip, SEEK_CUR) < 0) {
      perror("lseek");
      exit(TOC_FAIL);
    }

    /* Read footer */
    if (read(afd, &footer, sizeof(footer)) != sizeof(footer)) {
      /* Reached end of file, not an error */
      if (archive_name) close(afd);
      return;
    }

    /* Read next header */
    n = read(afd, &header, sizeof(header));
  }

  if (n < 0) {
    perror("read");
    exit(TOC_FAIL);
  }

  if (archive_name) close(afd);
}


void validate_archive(const char *archive_name, int verbose) {
  int afd = STDIN_FILENO;
  arvik_header_t header;
  arvik_footer_t footer;
  char tagbuf[sizeof(ARVIK_TAG)];
  char buf[4096];
  char h_hex[33], d_hex[33];
  ssize_t n;
  int ok_header, ok_data;
  unsigned char md4_header[MD4_DIGEST_LENGTH];
  unsigned char md4_data[MD4_DIGEST_LENGTH];
  MD4_CTX ctx_header;
  MD4_CTX ctx_data;
  char fname[ARVIK_NAME_LEN + 1];
  char *lt;
  long fsize;
  long remaining;
  int i;
  ssize_t max;
  ssize_t chunk;

  /* Open archive */
  if (archive_name != NULL) {
    afd = open(archive_name, O_RDONLY);
    if (afd < 0) {
      perror("open archive");
      exit(MD4_ERROR);
    }
  }

  /* Verify ARVIK_TAG */
  n = read(afd, tagbuf, strlen(ARVIK_TAG));
  if (n != (ssize_t)strlen(ARVIK_TAG) ||
      memcmp(tagbuf, ARVIK_TAG, strlen(ARVIK_TAG)) != 0) {
    fprintf(stderr, "Invalid archive tag.\n");
    exit(BAD_TAG);
  }

  /* Read first header */
  n = read(afd, &header, sizeof(header));

  while (n == sizeof(header)) {
    fsize = strtol(header.arvik_size, NULL, 10);

    /* Compute header md4 */
    MD4Init(&ctx_header);
    MD4Update(&ctx_header, (const unsigned char *)&header, sizeof(header));
    MD4Final(md4_header, &ctx_header);

    /* Compute data md4 */
    MD4Init(&ctx_data);
    remaining = fsize;
    while (remaining > 0) {
      max = (ssize_t)sizeof(buf);
      chunk = (remaining > max) ? max : remaining;
      n = read(afd, buf, chunk);
      if (n <= 0) {
        perror("read data");
        exit(READ_FAIL);
      }
      MD4Update(&ctx_data, (const unsigned char *)buf, n);
      remaining -= n;
    }
    MD4Final(md4_data, &ctx_data);

    /* Skip padding byte if file size was odd */
    if (fsize % 2 == 1) {
      char pad;
      if (read(afd, &pad, 1) != 1) {
        perror("read padding");
        exit(READ_FAIL);
      }
    }

    /* Read footer */
    if (read(afd, &footer, sizeof(footer)) != sizeof(footer)) {
      fprintf(stderr, "Missing footer.\n");
      exit(READ_FAIL);
    }

    /* Convert md4 buffers into hex strings */
    memset(h_hex, 0, sizeof(h_hex));
    memset(d_hex, 0, sizeof(d_hex));
    for (i = 0; i < MD4_DIGEST_LENGTH; i++) {
      sprintf(h_hex + i*2, "%02x", md4_header[i]);
      sprintf(d_hex + i*2, "%02x", md4_data[i]);
    }

    ok_header = memcmp(h_hex, footer.md4sum_header, 32) == 0;
    ok_data = memcmp(d_hex, footer.md4sum_data, 32) == 0;

    /* Clean filename */
    memset(fname, 0, sizeof(fname));
    strncpy(fname, header.arvik_name, ARVIK_NAME_LEN);
    lt = strchr(fname, ARVIK_NAME_TERM);
    if (lt) *lt = '\0';

    if (verbose) {
      printf("%s: header=%s, data=%s\n",
             fname,
             ok_header ? "OK" : "BAD",
             ok_data ? "OK" : "BAD");
    }

    if (!ok_header || !ok_data) {
      fprintf(stderr, "Validation failed for %s\n", fname);
      exit(MD4_ERROR);
    }

    /* Read next header */
    n = read(afd, &header, sizeof(header));
  }

  if (n < 0) {
    perror("read header");
    exit(READ_FAIL);
  }

  if (archive_name) close(afd);
}

