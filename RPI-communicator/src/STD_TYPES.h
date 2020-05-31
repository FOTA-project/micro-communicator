#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

typedef uint8_t       u8;
typedef uint16_t      u16;
typedef uint32_t      u32;

typedef int8_t        s8;
typedef int16_t       s16;
typedef int32_t       s32;

typedef float         f32 ;
typedef double        f64 ;
typedef long double   f96 ;


typedef u8            ERROR_STATUS;

#define NULL          ((void *)0)

#define status_Ok     1U
#define status_NOk    0U

#define OK            1
#define NOT_OK        0

#define STD_ERROR_NOK				      1
#define STD_ERROR_Ok				         2
#define STD_TYPES_ERROR_NULL_POINTER	3
#define NULL ((void *)0)

#define  Error_s   	unsigned char

#endif /* STD_TYPES_H */

