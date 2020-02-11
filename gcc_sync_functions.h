#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2016, 2018 ")

#if defined(__MVS__) && !defined(__GCC_SYNC_FUNCTIONS__)
#define __GCC_SYNC_FUNCTIONS__ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <builtins.h>
#include <stdlib.h>

#ifndef GCC_SYNC_VOLATILE
#define GCC_SYNC_VOLATILE
#endif

#if !defined(bool) && !defined(__cplusplus)
#define bool int
#endif

#define __cs_4 __cs1
#define __cs_8 __csg

#define __arg1(x) x

#define SYNC_FETCH_AND_OP_FN(type,type_name,len,name,op,uop) \ 
static inline type __sync_fetch_and_##name##_##type_name(GCC_SYNC_VOLATILE type *ptr, type value) { \
  type old_value = *ptr; \
  type  new_value; \
  do {new_value = uop(old_value op value);} \
  while (__cs_##len(&old_value, (void *)ptr, &new_value)); \
  return old_value; \
}
#define SYNC_OP_AND_FETCH_FN(type,type_name,len,name,op,uop) \ 
static inline type __sync_##name##_and_fetch_##type_name(GCC_SYNC_VOLATILE type *ptr, type value) { \
  type old_value = *ptr; \
  type new_value; \
  do {new_value = uop(old_value op value);} \
  while (__cs_##len(&old_value, (void *)ptr, &new_value)); \
  return new_value; \
}
#define SYNC_FNS(type,type_name,len) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,add,+,__arg1) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,sub,-,__arg1) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,and,&,__arg1) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,or,|,__arg1) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,xor,^,__arg1) \
SYNC_FETCH_AND_OP_FN(type,type_name,len,nand,&,~) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,add,+,__arg1) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,sub,-,__arg1) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,and,&,__arg1) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,or,|,__arg1) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,xor,^,__arg1) \
SYNC_OP_AND_FETCH_FN(type,type_name,len,nand,&,~)

SYNC_FNS(int,int,4)
SYNC_FNS(long long,long_long,8)
#ifdef __64BIT__
SYNC_FNS(long,long,8)
#else
SYNC_FNS(long,long,4)
#endif

SYNC_FNS(unsigned int,unsigned_int,4)
SYNC_FNS(unsigned long long,unsigned_long_long,8)
#ifdef __64BIT__
SYNC_FNS(unsigned long,unsigned_long,8)
#else
SYNC_FNS(unsigned long,unsigned_long,4)
#endif

#define SYNC_BOOL_COMPARE_AND_SWAP_FN(type,type_name,len) \
static inline bool __sync_bool_compare_and_swap_##type_name(GCC_SYNC_VOLATILE type *ptr, type oldval, type newval) { \
  return !__cs_##len(&oldval, (void *)ptr, &newval); \
}
#define SYNC_VAL_COMPARE_AND_SWAP_FN(type,type_name,len) \
static inline type __sync_val_compare_and_swap_##type_name(GCC_SYNC_VOLATILE type *ptr, type oldval, type newval) { \
  __cs_##len(&oldval, (void *)ptr, &newval); \
  return oldval; \
}
#define SYNC_CS_FNS(type,type_name,len) \
SYNC_BOOL_COMPARE_AND_SWAP_FN(type,type_name,len) \
SYNC_VAL_COMPARE_AND_SWAP_FN(type,type_name,len)

SYNC_CS_FNS(int,int,4)
SYNC_CS_FNS(long long,long_long,8)
#ifdef __64BIT__
SYNC_CS_FNS(long,long,8)
SYNC_CS_FNS(void *,void_ptr,8)
#else
SYNC_CS_FNS(long,long,4)
SYNC_CS_FNS(void *,void_ptr,4)
#endif

#if 0 /* __asm__ modifier strings do not work in ascii mode c++ for some reason */
/* apparantly __sync_synchronize is a undocumented builtin function.  No telling whether it works. */
static inline void __sync_synchronize_(void) {
  __asm__ volatile ("BCR 15,0");
};
static inline int __sync_lock_test_and_set_int(GCC_SYNC_VOLATILE int *ptr, int value) {
  int result;
  __asm__ volatile (" XR %0,%0\n TS %1\n IPM %0\n SRL %0,28":"=r"(result):"a"(ptr));
  return result;
}
static inline void __sync_lock_release_int(GCC_SYNC_VOLATILE int *ptr) {
  __asm__ volatile (" MVI X'00',%0\n BC 15,0"::"a"(ptr));
}
#else
static inline void __sync_synchronize_(void) {
  int x = 0;
  __sync_fetch_and_add_int(&x, 0);
}
static inline int __sync_lock_test_and_set_int(GCC_SYNC_VOLATILE int *ptr, int value) {
  return __sync_fetch_and_or_int(ptr, -1);
}
static inline void __sync_lock_release_int(GCC_SYNC_VOLATILE int *ptr) {
  __sync_fetch_and_and_int(ptr, 0);
}
#endif

#ifdef __cplusplus
}
#endif

#endif

/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

