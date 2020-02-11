#ifndef alloca
#define alloca(x) __alloca(x)
#ifdef __cplusplus
  extern "builtin"
#else
  #pragma linkage(__alloca,builtin)
#endif
void *__alloca(unsigned int x);
#endif

/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

