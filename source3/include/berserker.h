#ifndef __BERSERKER_H__
#define __BERSERKER_H__

#define _PWD_H_	/* disabilita l'inclusione del file pwd.h sotto android */
#define _GRP_H_

#include <unistd.h>
#include <sys/types.h>

struct passwd {
  char* pw_name;
  char* pw_passwd;
  uid_t pw_uid;
  gid_t pw_gid;
#ifdef __LP64__
  char* pw_gecos;
#endif
  char* pw_dir;
  char* pw_shell;
};

struct group {
  char* gr_name;
  char* gr_passwd;
  gid_t gr_gid;
  char** gr_mem;
};

#ifndef BERSERKER_USE_ORIG
#define getpwnam berserker_getpwnam
struct passwd *berserker_getpwnam(const char* __name);
#define getpwuid berserker_getpwuid
struct passwd *berserker_getpwuid(uid_t __uid);
#define setpwent berserker_setpwent
void berserker_setpwent(void);
#define getpwent berserker_getpwent
struct passwd *berserker_getpwent(void);
#define endpwent berserker_endpwent
void berserker_endpwent(void);
#define getgrnam berserker_getgrnam
struct group *berserker_getgrnam(const char* __name);
#define getgrgid berserker_getgrgid
struct group *berserker_getgrgid(gid_t __gid);
#endif

/* fix per defines mancanti di utmp.ut_type */
#ifndef DEAD_PROCESS
#define DEAD_PROCESS  8
#endif

#endif
