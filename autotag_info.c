#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2016, 2018 ")

#ifdef autotag_main
#include "encode_standard_files.c"
#endif

/*
xlc -qlanglvl=extc99 -qascii -q64 -qnocse -qfloat=ieee -qgonum -Dautotag_main=main -o ~/autotag_info autotag_info.c

#cd ~/Rnew; ~/autotag_info . > autotag_info.out
 */


#define _POSIX_SOURCE
#ifndef _OPEN_SYS_FILE_EXT
#define _OPEN_SYS_FILE_EXT
#endif
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static unsigned char IBM_1047_to_ISO8859_1[256] = {
  0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F, 0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x9D, 0x0A, 0x08, 0x87, 0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x17, 0x1B, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
  0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
  0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5, 0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
  0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF, 0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
  0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5, 0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
  0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
  0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,
  0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,
  0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0x5B, 0xDE, 0xAE,
  0xAC, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC, 0xBD, 0xBE, 0xDD, 0xA8, 0xAF, 0x5D, 0xB4, 0xD7,
  0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,
  0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,
  0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F};

/* bad, whitespace, english, international */
#define classify(ch) \
  (ch==0x09||ch==0x0A||ch==0x0B||ch==0x0C||ch==0x0D||ch==20||ch==0x85)?1: \
  (ch<=0x1F||ch==0xA0)?0: \
  (ch<=0x7E)?2: \
  3

/*
  Note that files containing only the characters:
  ASCII  @KLMNOPZ[\]^_`aklmnoyz{|}~
  EBCDIC  .<(+|&!$*);^-/,%_>?`:#@'= 
  cannot be classified.  Empty files included.
*/

static char *autotag_binary_extensions[] =
  {"tar", "tgz", "bz2", "gz", "zip", "xz", "pdf", "elc",
   "pyc", "pbm", "png", "gif", "jpg", "o", "so", "a", "mo", "gmo",
   "rds", "dbf", "flate", "SYD", "syd", "dta", "sav", "xpt", 0};

static char *autotag_text_extensions[] =
  {"el", "c", "C", "c++", "cxx", "cpp", "h", "hpp", "f", "in", "po", "map", "py", "pl", "awk", "texi", "m4", "sh", "xml", 
   "txt", "svg", "html", "css", "js", 0};

static int autotag_check_file_extension(char *ext, char **extensions)
{
  char *check;
  for (int i=0; 0 != (check = extensions[i]); i++) {
    if (0 == strcmp(ext, check))
      return 1;
  }
  return 0;
}

int autotag_analyze_byte_limit = 0;
int autotag_analyze_small_file_limit = 16;
int autotag_verbose_level = 0;

/* , int bad_character_limit, FILE *verbose_out, int *recommended_txtflag, int *recommended_ccsid */
int autotag_analyze_file(char *name, struct stat *st, int fd, FILE *verbose_out)
{
  struct stat st_s;
  if (st == NULL) {
    if (-1 == stat(name, &st_s)) {
      return -1;
    }
    st = &st_s;
  }
  if (!S_ISREG(st->st_mode)) {
    return -1;
  }
  int need_to_close_fd = fd < 0;
  fd = need_to_close_fd ? open(name, O_RDONLY) : fd;
  if (need_to_close_fd && fd < 0) {
    printf("Unable to open %s, errno=%d (%s)\n", name, errno, strerror(errno));
    return -1;
  }
  unsigned long long a_counts[4] = {0};
  unsigned long long e_counts[4] = {0};
  unsigned long long utf8_errors = 0;
  unsigned long long utf8_counts[8] = {0};
  unsigned long long byte_count = 0;
  int nc = 0;
  int utf16sp = 0;
  int ch, ech;
  unsigned char buffer[4096];
  while (1) {
    int limit = sizeof(buffer);
    if (autotag_analyze_byte_limit > 0) {
      if (byte_count >= autotag_analyze_byte_limit) break;
      int remain = autotag_analyze_byte_limit - byte_count;
      if (remain < limit) limit = remain;
    }
    int count;
    while (-1 == (count = read(fd, buffer, limit)) && errno == EINTR) {}
    if (count <= 0) break;
    byte_count += count;
    for (int index = 0; index < count; index++) {
      ch = buffer[index];
      int n = 0;
      for (unsigned char nch = ch; nch & 0x80; nch = nch << 1) {
        n++;
      }
      int a_c = classify(ch);
      a_counts[a_c]++;
      int e_c = classify(IBM_1047_to_ISO8859_1[ch]);
      e_counts[e_c]++;
      utf8_counts[n]++;
      /* if (a_c == 0 || a_c == 3) printf("%02X\n", ch); */
      if (n > 4) {
        utf8_errors++;
        utf16sp = 0;
        nc = 0;
      } else if (nc == 0 && n == 0) {
      } else if (nc == 0 && n > 1) {
        nc = n-1;
        if (ch == 0xED) utf16sp = 1;
      } else if (nc > 0 && n == 1) {
        if (utf16sp && (ch & 0x020)) utf8_errors++;
        nc--;
        utf16sp = 0;
      } else {
        utf8_errors++;
      }
    }
  }

  unsigned long long utf8_nonzero_counts = 0;
  for (int i=1; i<8; i++) utf8_nonzero_counts += utf8_counts[i];
  unsigned long long size = st->st_size;

  int is_utf8 = (utf8_errors == 0);
  double utf8_good_fraction = (size == 0) ? -1.0 : (size - a_counts[0])/(double)size;
  double utf8_bad_fraction = (size == 0) ? 2.0 : a_counts[0]/(double)size;

  int is_iso8859_english = a_counts[3] == 0; /* same as (0 == utf8_nonzero_counts) */
  double iso8859_english_good_fraction = (size == 0) ? -1.0 : (a_counts[1]+a_counts[2])/(double)size;
  double iso8859_english_bad_fraction = (size == 0) ? 2.0 : (a_counts[0]+a_counts[3])/(double)size;
  double iso8859_good_fraction = (size == 0) ? -1.0 : (a_counts[1]+a_counts[2]+a_counts[3])/(double)size;
  double iso8859_bad_fraction = (size == 0) ? 2.0 : a_counts[0]/(double)size;
  
  int is_ebcdic_english = e_counts[3] == 0;
  double ebcdic_english_good_fraction = (size == 0) ? -1.0 : (e_counts[1]+e_counts[2])/(double)size;
  double ebcdic_english_bad_fraction = (size == 0) ? 2.0 : (e_counts[0]+e_counts[3])/(double)size;
  double ebcdic_good_fraction = (size == 0) ? -1.0 : (e_counts[1]+e_counts[2]+e_counts[3])/(double)size;
  double ebcdic_bad_fraction = (size == 0) ? 2.0 : e_counts[0]/(double)size;

  char *dot = strrchr(name, '.');
  char *slash_after_dot = dot ? strchr(dot, '/') : NULL;
  if (slash_after_dot) dot = NULL;
  char *slash = strrchr(name, '/');
  char *ext = dot ? dot+1 : slash ? slash+1 : name;
  if (ext[0]==0) ext = "EMPTY";
  
  int has_binary_extension = autotag_check_file_extension(ext, autotag_binary_extensions);
  int has_text_extension = autotag_check_file_extension(ext, autotag_text_extensions);

  int recommended_txtflag = -1;
  int recommended_ccsid = 0;
  if (size == 0) {
    if (has_binary_extension) {
      recommended_txtflag = 0; recommended_ccsid = FT_BINARY;
    }
    if (has_text_extension) {
      recommended_txtflag = 1; recommended_ccsid = 819;
    }
  } else if ((is_utf8 || iso8859_english_bad_fraction < 0.001) && ebcdic_english_bad_fraction > 0.04) {
    /* UTF-8 1208,  ISO8859-1 819,  ASCII 367 */
    if (is_iso8859_english) {
      recommended_txtflag = 1; recommended_ccsid = 367;
    } else if (is_utf8) {
      recommended_txtflag = 0; recommended_ccsid = 1208;
    } else {
      recommended_txtflag = 1; recommended_ccsid = 819;
    }
  } else if (iso8859_bad_fraction < 0.001 && ebcdic_english_bad_fraction > 0.30) {
    recommended_txtflag = 1; recommended_ccsid = 819;
  } else if (ebcdic_bad_fraction < 0.001 && iso8859_english_bad_fraction > 0.30) {
    recommended_txtflag = 1; recommended_ccsid = 1047;
  } else if (ebcdic_english_bad_fraction > 0.04 && iso8859_english_bad_fraction > 0.04) {
    recommended_txtflag = 0; recommended_ccsid = FT_BINARY;
  }
  int name_contains_blank = 0 != strchr(name, ' ');

  if (verbose_out && autotag_verbose_level &&
      (autotag_verbose_level >= 2) || (!name_contains_blank && (size > 0) && (recommended_ccsid == -1))) {
    fprintf(verbose_out, "%s %s %lld %d %.6f %.6f  %.6f %.6f  %s\n",
	    name, ext, size, is_utf8 && !is_iso8859_english,
	    iso8859_english_bad_fraction, ebcdic_english_bad_fraction,
	    iso8859_bad_fraction, ebcdic_bad_fraction,
	    (recommended_ccsid == 0) ? "none" :
	    (recommended_ccsid == FT_BINARY) ? "BINARY" :
	    (recommended_ccsid == 367) ? "ASCII" :
	    (recommended_ccsid == 819) ? "ISO8859-1" :
	    (recommended_ccsid == 1208) ? "UTF-8" :
	    (recommended_ccsid == 1047) ? "IBM-1047" :
	    "other");
  }
  /* autotag_skip_tagged_files */

  /* char *name, struct stat *st, FILE *verbose_out, int *recommended_txtflag, int *recommended_ccsid */
  if (need_to_close_fd)
    close(fd);
  else
    lseek(fd, 0, SEEK_SET);
  return recommended_ccsid;
}

int autotag_skip_tagged_files = 0;
int autotag_test = 0;
int autotag_recursive = 0;

int autotag_check_file(char *file)
{
  int result = 0;
  struct stat st;
  errno = 0;
  if (-1 == stat(file, &st)) {
    printf("stat of %s returned %d (%s)\n", file, errno, strerror(errno));
    return -1;
  }
  if (S_ISDIR(st.st_mode)) {
    if (!autotag_recursive) return 0;
    DIR *dir = opendir(file);
    char fullname[1024];
    if (dir == NULL) {
      printf("Unable to upen the directory %s, errno=%d (%s)\n", file, errno, strerror(errno));
      return -1;
    }
    struct dirent *ent; 
    while (0 != (ent = readdir(dir))) {
      if (0!=strcmp(ent->d_name,".") && 0!=strcmp(ent->d_name,"..")) {
	snprintf(fullname, sizeof(fullname), "%s/%s", file, ent->d_name);
	autotag_check_file(fullname);
      }
    }    
    closedir(dir);
    return result;
  }
  if (!S_ISREG(st.st_mode)) {
    return 0;
  }
  autotag_analyze_file(file, &st, -1, stdout);
}

#ifdef autotag_main
int autotag_main(int argc, char **argv)
{
  int ret = 0;

  encode_standard_files();
  autotag_verbose_level = 1;
  autotag_recursive = 1;
  for (int i=1; i<argc; i++) {
    if (0==strcmp(argv[i], "-v") || 0==strcmp(argv[i], "--verbose"))
      autotag_verbose_level = 2;
    else if (0==strcmp(argv[i], "-f") || 0==strcmp(argv[i], "--file"))
      autotag_check_file(argv[++i]);
    else if (0==strcmp(argv[i], "-R") || 0==strcmp(argv[i], "--recursive"))
      autotag_recursive = 1;
    else if (0==strcmp(argv[i], "-t") || 0==strcmp(argv[i], "--test"))
      autotag_test = 1;
    else if (0==strcmp(argv[i], "-s") || 0==strcmp(argv[i], "--skip-tagged-files"))
      autotag_skip_tagged_files = 1;
    else if (0==strcmp(argv[i], "-h") || 0==strcmp(argv[i], "--help"))
      printf("Usage: autotag options files\n"
             "  Classify and tag files based on their contents\n"
             "Options\n"
	     "-R --recursive\n"
             "-s --skip-tagged-files         (analyze and tag only files without tags)\n"
             "-t --test                      (does not make any changes)\n"
	     "-f --file              file    (-f is useful if a file argument begins with '-')\n"
	     "-v --verbose\n");
    else {
      int cr = autotag_check_file(argv[i]);
    }
  }
  return ret;
}
#endif

/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

