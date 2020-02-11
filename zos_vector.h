
#ifndef __ZOS_VECTOR__
#define __ZOS_VECTOR__ 1

#define CVT (*(unsigned int *)0x10)
#define ECVT (*(unsigned int *)(CVT + 0x8C))
#define PVER_PREL *(unsigned int *)(ECVT + 0x200)
#define have_facilities_list1 (PVER_PREL >= 0xF0F2F0F2)
/* facilities_list1 requires z/OS 2.2 */

#define vector_is_enabled() \
  ((*(unsigned char *)(CVT + 0xF4)) & 0x80 /* CVTVEF	"X'80'" Vector Extension Facility */ )
  
#define have_vector_without_float() \
  (vector_is_enabled() && have_facilities_list1 && ((*(unsigned char *)0xD8) & 0x40 /* bit 129 */))
/* vector without float requires ARCH(11) */
#define have_vector_with_float() \
  (vector_is_enabled() && have_facilities_list1 && ((*(unsigned char *)0xD8) & 0x01 /* bit 135 */))
/* vector with float requires ARCH(12) option and the z/OS V2R3 XL C/C++ compiler */

#endif


/*
© 2016-2018 Rocket Software, Inc. or its affiliates. All Rights Reserved.
ROCKET SOFTWARE, INC.
*/

