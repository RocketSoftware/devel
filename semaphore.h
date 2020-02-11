
typedef void *sem_t;
#define SEM_FAILED ((sem_t *)-1)

#define _XOPEN_SOURCE_EXTENDED 1
#define _OPEN_SYS_TIMED_EXT 1
#include <time.h>

#ifdef DEFINE_SEMAPHORE_FUNCTIONS
#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2016, 2019 ")

#define _OPEN_SYS_FILE_EXT 1
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/* why won't these initialize to non-zero? */
int disable_sem_trace = 0; 
int sem_trace_fd = 0;

static int sem_trace_prefix(char *buffer, int length)
{
  struct timeval tv_s, *tv=&tv_s; /* time_t tv_sec; long tv_usec; */
  int gtod = gettimeofday(tv, NULL);

  time_t tt = tv->tv_sec;
  struct tm result_s, *result = &result_s;
#if MVS
  long time_zone_offset_in_seconds = 
    (long)(((long long ***)0)[0x10/4 /*FLCCVT*/][0x148/4 /*CVTEXT2*/][0x38/8 /*CVTLDTO*/] /
           4096000000LL);
  tt += time_zone_offset_in_seconds;
  gmtime_r(&tt, result);
#else
#ifdef __MINGW32__
  struct tm *localtime_result = localtime(&tt);
  memcpy(result, localtime_result, sizeof(struct tm));
#else
  localtime_r(&tt, result);
#endif
#endif

  return snprintf(buffer, length, "%04d-%02d-%02d-%02d-%02d-%02d.%06d %10d ",
		  1900+result->tm_year, result->tm_mon+1, result->tm_mday,
		  result->tm_hour, result->tm_min, result->tm_sec, tv->tv_usec,
		  getpid());
}

static void sem_trace(const char *format, ...)
{
  if (!disable_sem_trace && sem_trace_fd == 0) {
    char *sem_trace_from_env = getenv("SEM_TRACE");
    if (sem_trace_from_env == NULL || sem_trace_from_env[0] == 0) {
      disable_sem_trace = 1;
    } else {
      sem_trace_fd = open(sem_trace_from_env, O_WRONLY+O_APPEND+O_CREAT, S_IRUSR+S_IWUSR + S_IRGRP + S_IROTH);
      if (sem_trace_fd == -1) {
	printf("sem_trace: %s %s\n", sem_trace_from_env ? sem_trace_from_env : "NULL", strerror(errno));
	fflush(stdout);
	disable_sem_trace = 1;
      }
    }
  }
  if (!disable_sem_trace) {
    va_list ap;
    char buffer[512];
    int prefix_len = sem_trace_prefix(buffer, sizeof(buffer)-1);
    va_start(ap, format);
    int n = vsnprintf(buffer+prefix_len, sizeof(buffer)-prefix_len-1, format, ap);
    va_end(ap);
    write(sem_trace_fd, buffer, prefix_len+n);
  }
}

char *named_semaphore_filename = NULL;

/*
$HOME/.semaphore
$HOME/.semaphore/name
$HOME/.semaphore/name/encoded_sem_name
$HOME/.semaphore/pid
$HOME/.semaphore/pid/semid

each file contains a semid; all users use hardlinks to this file
  before every rm we check the number of links
there is a directory containing sem_names as the filename
  sem_unlink
each process has a directory containing semid as the filenames
  sem_close, _exit, exec
 */

char *semaphore_base_directory = NULL;
char *semaphore_name_directory = NULL;
char *semaphore_pid_directory = NULL;

static char *check_directory(char **directory_ptr, char *name, int semid);

char *get_semaphore_base_directory(void)
{
  return check_directory(&semaphore_base_directory, NULL, -1);
}

char *get_semaphore_name_directory(void)
{
  return check_directory(&semaphore_name_directory, "name", -1);
}

char *get_semaphore_pid_directory(void)
{
  return check_directory(&semaphore_pid_directory, NULL, 0);
}

static char *check_directory(char **directory_ptr, char *name, int semid)
{
  if (*directory_ptr) {
    /* sem_trace("check_directory: have %s for name=%s, semid=%d\n", *directory_ptr, name ? name : "NULL", semid); */
    return *directory_ptr;
  }
  char dir[1024];
  if (name) {
    snprintf(dir, sizeof(dir), "%s/name", get_semaphore_base_directory());
  } else if (semid != -1) {
    snprintf(dir, sizeof(dir), "%s/%d", get_semaphore_base_directory(), getpid());
  } else {
    char *user = getenv("USER");
    snprintf(dir, sizeof(dir), "/tmp/semaphore_%s", user);
  }
  *directory_ptr = strdup(dir);
  if (*directory_ptr == NULL) {
    return NULL;
  }
  int fail = 0;

  struct stat st;
  memset(&st, 0, sizeof(st));
  int statrc = stat(dir, &st);
  if (statrc != -1) {
    if (!S_ISDIR(st.st_mode)) {
      sem_trace("check_directory: %s exists, but is not a directory\n", *directory_ptr);
      fail = 1;
    }
    if (!(0 == access(dir, R_OK+W_OK+X_OK))) {
      sem_trace("check_directory: %s exists, but we do not have rwx permission to it\n", *directory_ptr);
      fail = 1;
    }
    if (!fail)
      return *directory_ptr;
  }

  if (!fail) {
    int required_permissions = S_IRWXU;
    int debug_permissions = (S_IRGRP+S_IXGRP) + (S_IROTH+S_IXOTH); /* make debugging a bit easier */
    int mkdir_rc = mkdir(*directory_ptr,  required_permissions + debug_permissions);
    sem_trace("check_directory: %s did not exist, and mkdir failed with %s\n", *directory_ptr, strerror(errno));
    if (mkdir_rc == -1 && errno != EEXIST) 
      fail = 1;
  }

  if (fail) {
    free(*directory_ptr);
    *directory_ptr = NULL;
  }
  return *directory_ptr;
}

/* returns the semid or -1 */
int find_semaphore(char *name)
{
  char name_fullname[1024];
  snprintf(name_fullname, sizeof(name_fullname), "%s%s", get_semaphore_name_directory(), name);
  FILE *in = fopen(name_fullname, "rb");
  if (in == NULL) {
    sem_trace("find_semaphore: failed to open %s: %s\n", name_fullname, strerror(errno));
    return -1;
  }
  int semid = -1;
  int n = fscanf(in, "%d\n", &semid);
  fclose(in);
  sem_trace("find_semaphore: read %s, semid=%d\n", name_fullname, semid);
  return semid;
}

int need_to_cleanup_semaphores = 0;

static int unlink_semaphore_internal(const char *fullname);
void cleanup_semaphores(void)
{
  if (!need_to_cleanup_semaphores) {
    return;
  }
  char *pid_fullname = get_semaphore_pid_directory();
  if (pid_fullname == NULL) {
    sem_trace("cleanup_semaphores: no pid directory\n");
    return;
  }
  struct stat st;
  DIR *dir = opendir(pid_fullname);
  if (dir == NULL) {
    sem_trace("cleanup_semaphores: can not open %s, errno=%d (%s)\n", pid_fullname, errno, strerror(errno));
    return;
  }
  sem_trace("cleanup_semaphores: cleaning %s\n", pid_fullname);
  char semid_fullname[1024];
  struct dirent *ent; 
  while (0 != (ent = readdir(dir))) {
    if (0==strcmp(ent->d_name,".") ||  0==strcmp(ent->d_name,".."))
      continue;
    snprintf(semid_fullname, sizeof(semid_fullname), "%s/%s", pid_fullname, ent->d_name);
    unlink_semaphore_internal(semid_fullname);
  }    
  closedir(dir);
  rmdir(pid_fullname);
  char *name_fullname = get_semaphore_name_directory();
  if (name_fullname == NULL) {
    sem_trace("cleanup_semaphores: no name directory\n");
    return;
  }
  dir = opendir(name_fullname);
  if (dir == NULL) {
    sem_trace("cleanup_semaphores: cannot open %s, errno=%d (%s)\n", name_fullname, errno, strerror(errno));
    return;
  }
  int is_empty = 1;
  while (0 != (ent = readdir(dir))) {
    if (0==strcmp(ent->d_name,".") ||  0==strcmp(ent->d_name,".."))
      continue;
    is_empty = 0;
    break;
  }    
  closedir(dir);
  if (is_empty) {
    char *base_name = get_semaphore_base_directory();
    sem_trace("cleanup_semaphores: %s is empty, removing it and %s\n", name_fullname, base_name);
    rmdir(name_fullname);
    rmdir(base_name);
  } else {
    sem_trace("cleanup_semaphores: %s is not empty\n", name_fullname);
  }
}
    
static int open_semaphore(int semid, const char *name, int created)
{
  if (need_to_cleanup_semaphores == 0) {
    need_to_cleanup_semaphores = 1;
    atexit(cleanup_semaphores);
  }
  char name_fullname[1024];
  snprintf(name_fullname, sizeof(name_fullname), "%s%s", get_semaphore_name_directory(), name);
  if (created) {
    FILE *out = fopen(name_fullname, "wb");
    if (out == NULL) {
      sem_trace("open_semaphore: failed to open %s, %s\n", name_fullname, strerror(errno));
      return -1;
    }
    fprintf(out, "%d\n", semid);
    fclose(out);
  }
  char semid_fullname[1024];
  snprintf(semid_fullname, sizeof(semid_fullname), "%s/%d", get_semaphore_pid_directory(), semid);
  int link_rc = link(name_fullname, semid_fullname);
  if (created)
    sem_trace("open_semaphore: wrote %s, linked %s\n", name_fullname, semid_fullname);
  else
    sem_trace("open_semaphore: reused %s, linked %s\n", name_fullname, semid_fullname);
  return link_rc;
}

static int unlink_semaphore(const char *name)
{
  char name_fullname[1024];
  snprintf(name_fullname, sizeof(name_fullname), "%s%s", get_semaphore_name_directory(), name);
  return unlink_semaphore_internal(name_fullname);
}

static int close_semaphore(int semid)
{
  char semid_fullname[1024];
  snprintf(semid_fullname, sizeof(semid_fullname), "%s/%d", get_semaphore_pid_directory(), semid);
  return unlink_semaphore_internal(semid_fullname);
}

static int unlink_semaphore_internal(const char *fullname)
{
  FILE *in = fopen(fullname, "rb");
  if (in == NULL) {
    sem_trace("unlink_semaphore_internal: could not open %s\n", fullname);
    return -1;
  }
  int unlinkrc = unlink(fullname);
  int semid = -1;
  int n = fscanf(in, "%d\n", &semid);
  struct stat st;
  memset(&st, 0, sizeof(st));
  int statrc = fstat(fileno(in), &st);
  fclose(in);
  sem_trace("unlink_semaphore_internal: name=%s, stat rc=%d, unlink rc=%d, link count=%d\n",
	    fullname, statrc, unlinkrc, st.st_nlink);
  if (unlinkrc == 0 && statrc == 0 && st.st_nlink == 0) {
    int ret = semctl(semid,0,IPC_RMID,0);
    sem_trace("sem_close: sem=%d, ret=%d\n", semid, ret);
  }
  return 0;
}

sem_t *sem_open(char *name, int flags, int mode, int value)
{
  /* flags might have O_CREAT O_EXCL */
  /* if flag includes O_CREAT, mode and value must be specified */
  int semid = find_semaphore(name);
  if (semid != -1) {
    if ((flags & O_CREAT) && (flags & O_EXCL)) {
      sem_trace("sem_open: error: name=%s already exists\n", name);
      errno = EEXIST;
      return (sem_t *)-1;
    } else {
      open_semaphore(semid, name, 0);
      return (sem_t *)semid;
    }
  }
  if (!(flags & O_CREAT)) {
    sem_trace("sem_open: error: name=%s does not exist\n", name);
    errno = ENOENT;
    return (sem_t *)-1;
  } 
  int key = 0;
  int nsems = 1;
  do {
    key++;
    semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | mode);
  } while (semid == -1 && errno == EEXIST);
  if (semid == -1) {
    sem_trace("sem_open: semget returned %s\n", strerror(errno));
  } else {
    if (value > SEM_MX_VAL) {
      sem_trace("sem_open: initial value is %d, but maximum allowed value is %d.  reducing the initial value\n",
		value, SEM_MX_VAL);
      value = SEM_MX_VAL;
    }
    union semun {
      int val;
      struct semid_ds *buf;
      unsigned short *array;
    } arg;
    arg.val = value;
    if (-1 == semctl(semid,0,SETVAL,arg)) {
      sem_trace("sem_open: semctl returned %s\n", strerror(errno));
    }
    open_semaphore(semid, name, 1);
    if (1) {
      int value = semctl(semid,0,GETVAL,0);
      sem_trace("sem_open: value = %d\n", value);
    }
  }
  sem_trace("sem_open: name=%s, flags=%X, mode=%X, value=%d, sem=%d\n", name, flags, mode, value, semid);
  return (sem_t *)semid;
}

int sem_close(sem_t *sem)
{
  return close_semaphore((int)sem);
}

int sem_unlink(const char *name)
{
  return unlink_semaphore(name);
}

int sem_getvalue(sem_t *sem, int *value_ptr)
{
  int value = semctl((int)sem,0,GETVAL,0);
  sem_trace("sem_get_value: sem=%d, value=%d\n", sem, value);
  if (value < 0) return value;
  *value_ptr = value;
  return 0;
}

int sem_wait(sem_t *sem)
{
  struct sembuf sb = {.sem_num=0, .sem_op=-1, .sem_flg=0};
  sem_trace("sem_wait: before: sem=%d\n", sem);
  if (1) {
    int value = semctl((int)sem,0,GETVAL,0);
    sem_trace("sem_wait: value = %d\n", value);
  }
  int ret = semop((int)sem, &sb, 1);
  sem_trace("sem_wait: after: sem=%d, ret=%d\n", sem, ret);
  return ret;
}

int sem_trywait(sem_t *sem)
{
  struct sembuf sb = {.sem_num=0, .sem_op=-1, .sem_flg=IPC_NOWAIT};
  sem_trace("sem_trywait: before: sem=%d\n", sem);
  int ret = semop((int)sem, &sb, 1);
  sem_trace("sem_trywait: after: sem=%d, ret=%d\n", sem, ret);
  return ret;
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
  struct timeval now;
  if (gettimeofday(&now, NULL) < 0) {
    int saved_errno = errno;
    sem_trace("sem_timedwait: gettimeofday failed %s\n", strerror(errno));
    errno = saved_errno;
    return -1;
  }
  struct timespec rel_timeout = {.tv_sec=abs_timeout->tv_sec-now.tv_sec, .tv_nsec=abs_timeout->tv_nsec-1000*now.tv_usec};
  rel_timeout.tv_sec += (rel_timeout.tv_nsec / 1000000000);
  rel_timeout.tv_nsec %= 1000000000;
  sem_trace("sem_timedwait: before: sem=%d, sec=%d, nsec=%d\n", sem, rel_timeout.tv_sec, rel_timeout.tv_nsec);

  struct sembuf sb = {.sem_num=0, .sem_op=-1, .sem_flg=0};
  int ret = __semop_timed((int)sem, &sb, 1, &rel_timeout);
  sem_trace("sem_timedwait: after: sem=%d\n", sem);
  return ret;
}

int sem_post(sem_t *sem)
{
  struct sembuf sb = {.sem_num=0, .sem_op=1, .sem_flg=0};
  sem_trace("sem_post: before: sem=%d\n", sem);
  int ret = semop((int)sem, &sb, 1);
  sem_trace("sem_post: after: sem=%d, ret=%d\n", sem, ret);
  return ret;
}

#else

int enable_sem_trace;
char sem_trace_fd;

sem_t *sem_open(char *name, int flags, int mode, int value);
int sem_close(sem_t *sem);
int sem_unlink(const char *name);
int sem_getvalue(sem_t *sem, int *value_ptr);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);

#endif

/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

