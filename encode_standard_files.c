#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2016, 2018 ")

/* put this at the top of any that contains a main() function */
/* #include <encode_standard_files.c> */

#ifdef __MVS__
#define _OPEN_SYS_FILE_EXT 1
#ifndef ENCODE_STANDARD_FILES_NO_RUNOPTS
#pragma runopts(filetag(autocvt, autotag))
#endif
#endif

#ifdef __MVS__
#include <stdio.h>
#include <string.h>
#include <env.h>
#include <sys/stat.h>
#include <sys/modes.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#endif

#ifndef ESF_STATIC
#define ESF_STATIC
#endif
       
#ifndef ENCODE_STANDARD_FILES_SET_PCCSID
#define ENCODE_STANDARD_FILES_SET_PCCSID 0
#endif

#if __CHARSET_LIB
# define COMPILE_MODE_CCSID 819
#else
# define COMPILE_MODE_CCSID 1047
#endif

ESF_STATIC void encodeStandardFiles_restore(void);
ESF_STATIC void encodeStandardFiles_setBinary(int fd);
ESF_STATIC void encodeStandardFiles_setIgnore(int fd);
ESF_STATIC void encodeStandardFiles(void);

struct file_encoding_info {
  int new_file;
  int is_binary;
  int is_ignore;
  int stat_error;
  struct stat st;
#ifdef __MVS__
  struct file_tag ft;
  struct f_cnvrt fc;
#endif
} initial_file_encoding[3] = {{0},{0},{0}};

ESF_STATIC void encodeStandardFiles_setBinary(int fd)
{
  if (0<=fd && fd<=2)
    initial_file_encoding[fd].is_binary = 1;
}

ESF_STATIC void encodeStandardFiles_setIgnore(int fd)
{
  if (0<=fd && fd<=2)
    initial_file_encoding[fd].is_ignore = 1;
}

/* I have not found any example that shows that this makes a difference */
#define encodeStandardFiles_cvt_off_before_change 0

ESF_STATIC void determineFileEncodingInfo(int fd, struct file_encoding_info *fei)
{
  int fstatrc = fstat(fd, &fei->st);
  if (0 > fstatrc) {
    fei->stat_error = errno;
  } else {
    fei->new_file = S_ISREG(fei->st.st_mode) && fei->st.st_size == 0;
    fei->ft = fei->st.st_tag;
    fei->fc.cvtcmd = QUERYCVT;
    fcntl(fd, F_CONTROL_CVT, &fei->fc);
  }
}

ESF_STATIC void debugEncodeStandardFiles(FILE *esfd)
{
  for (int fd=0; fd<=2; fd++) {
    struct file_encoding_info *ife = &initial_file_encoding[fd];
    struct file_encoding_info fei_s = {0}, *fei = &fei_s;
    determineFileEncodingInfo(fd, fei);
    char *type =
      S_ISBLK(fei->st.st_mode) ? "blk" :
      S_ISDIR(fei->st.st_mode) ? "dir" :
      S_ISCHR(fei->st.st_mode) ? "chr" :
      S_ISFIFO(fei->st.st_mode) ? "fifo" :
      S_ISREG(fei->st.st_mode) ? "reg" :
      S_ISLNK(fei->st.st_mode) ? "lnk" :
      S_ISSOCK(fei->st.st_mode) ? "sock" :
      S_ISVMEXTL(fei->st.st_mode) ? "mextl" :
      "unknown";
    if (fei->stat_error) {
      fprintf(esfd, "fd=%d, stat() errno=%d\n", fd, fei->stat_error);
    } else {
      fprintf(esfd,
	      "fd=%d, type=%s, dev=%d, inode=%d, "
	      "new_file=%d, is_binary=%d, is_ignore=%d, "
	      "txtflag=%d, ccsid=%d, "
	      "cvtcmd=%d, pccsid=%d, fccsid=%d\n",
	      fd, type, fei->st.st_dev, fei->st.st_ino,
	      fei->new_file, ife->is_binary, ife->is_ignore,
	      fei->ft.ft_txtflag, fei->ft.ft_ccsid,
	      fei->fc.cvtcmd, fei->fc.pccsid, fei->fc.fccsid);
    }
  }
}

ESF_STATIC void encodeStandardFiles(void)
{
#ifdef __MVS__
  setenv("_BPXK_AUTOCVT", "ON", 1);
  fflush(NULL);
  for (int fd=0; fd<=2; fd++) {
    determineFileEncodingInfo(fd, &initial_file_encoding[fd]);
  }
  char *esf_debug = getenv("ENCODESTANDARDFILES_DEBUG");
  FILE *esfd = esf_debug ? fopen(esf_debug, "at") : NULL;
  if (esfd) {fprintf(esfd, "encodeStandardFiles before changes\n"); debugEncodeStandardFiles(esfd);}
  for (int fd=0; fd<=2; fd++) {
    struct file_encoding_info *ife = &initial_file_encoding[fd];
    if (ife->is_ignore)
      continue;
    if (!ife->stat_error) {
      if (ife->new_file) {
	struct file_tag ft;
	if (ife->is_binary) {
	  ft.ft_ccsid = FT_BINARY;
	  ft.ft_txtflag = 0;
	} else {
	  ft.ft_ccsid = COMPILE_MODE_CCSID;
	  ft.ft_txtflag = 1;
	}
	fcntl(fd, F_SETTAG, &ft);
      } else {
	struct f_cnvrt fc;
#if encodeStandardFiles_cvt_off_before_change
	if (!ife->is_binary) {
	  fc.cvtcmd = SETCVTOFF;
	  fcntl(fd, F_CONTROL_CVT, &fc);
	}
#endif
	fc.cvtcmd = ife->is_binary ? SETCVTOFF : SETCVTON;
	fc.fccsid = ife->fc.fccsid ? ife->fc.fccsid : 1047;
#if ENCODE_STANDARD_FILES_SET_PCCSID
	fc.pccsid = COMPILE_MODE_CCSID;
#else
	fc.pccsid = 0;
#endif
	fcntl(fd, F_CONTROL_CVT, &fc);
      }
    }
  }
  atexit(encodeStandardFiles_restore);
  if (esfd) {fprintf(esfd, "encodeStandardFiles after changes\n"); debugEncodeStandardFiles(esfd);}
  if (esfd) fclose(esfd);
#endif
}

ESF_STATIC void encodeStandardFiles_restore(void)
{
#ifdef __MVS__
  char *esf_debug = getenv("ENCODESTANDARDFILES_DEBUG");
  FILE *esfd = esf_debug ? fopen(esf_debug, "at") : NULL;
  if (esfd) {fprintf(esfd, "encodeStandardFiles_restore\n");}
  fflush(NULL);
  for (int fd=0; fd<=2; fd++) {
    struct file_encoding_info *ife = &initial_file_encoding[fd];
    if (ife->is_ignore)
      continue;
    if (!ife->new_file && !ife->is_binary && !ife->stat_error) {
#if encodeStandardFiles_cvt_off_before_change
      if (ife->fc.cvtcmd != SETCVTOFF) {
	struct f_cnvrt fc;
	fc.cvtcmd = SETCVTOFF;
	fcntl(fd, F_CONTROL_CVT, &fc);
      }
#endif
#if !ENCODE_STANDARD_FILES_SET_PCCSID
      ife->fc.pccsid = 0;
#endif
      fcntl(fd, F_CONTROL_CVT, &ife->fc);
      if (esfd) {fprintf(esfd, "encodeStandardFiles_restore: fd=%d cvtcmd=%d pccsid=%d fccsid=%d\n",
			 fd, ife->fc.cvtcmd, ife->fc.pccsid, ife->fc.fccsid);}
    }
  }
  if (esfd) {fclose(esfd);}
#endif
}

#define encode_standard_files encodeStandardFiles
#define encode_standard_files_restore encodeStandardFiles_restore

/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

