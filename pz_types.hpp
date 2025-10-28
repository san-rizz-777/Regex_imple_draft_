#ifndef PZ_TYPES_HPP
#define PZ_TYPES_HPP

/*types*/
#define cut8 const unsigned char
#define ut64 unsigned long long
#define st64 long long
#define ut32 unsigned int
#define st32 int
#define ut16 unsigned short
#define st16 short
#define ut8 unsigned char
#define st8 signed char

/* limits */
#undef UT64_MAX
#undef UT64_GT0
#undef UT64_LT0
#undef UT64_MIN
#undef UT32_MAX
#undef UT32_MIN
#undef UT16_MIN
#undef UT8_MIN
#define ST64_MAX ((st64)0x7FFFFFFFFFFFFFFFULL)
#define ST64_MIN ((st64)(-ST64_MAX - 1))
#define UT64_MAX 0xFFFFFFFFFFFFFFFFULL
#define UT64_GT0 0x8000000000000000ULL
#define UT64_LT0 0x7FFFFFFFFFFFFFFFULL
#define UT64_MIN 0ULL
#define UT64_32U 0xFFFFFFFF00000000ULL
#define UT64_16U 0xFFFFFFFFFFFF0000ULL
#define UT64_8U 0xFFFFFFFFFFFFFF00ULL
#define UT32_MIN 0U
#define UT16_MIN 0U
#define UT32_GT0 0x80000000U
#define UT32_LT0 0x7FFFFFFFU
#define ST32_MAX 0x7FFFFFFF
#define ST32_MIN (-ST32_MAX - 1)
#define UT32_MAX 0xFFFFFFFFU
#define ST16_MAX 0x7FFF
#define ST16_MIN (-ST16_MAX - 1)
#define UT16_GT0 0x8000U
#define UT16_MAX 0xFFFFU
#define ST8_MAX 0x7F
#define ST8_MIN (-ST8_MAX - 1)
#define UT8_GT0 0x80U
#define UT8_MAX 0xFFU
#define UT8_MIN 0x00U
#define ASCII_MIN 32
#define ASCII_MAX 127

#if SSIZE_MAX == ST32_MAX
#define SZT_MAX UT32_MAX
#define SZT_MIN UT32_MIN
#define SSZT_MAX ST32_MAX
#define SSZT_MIN ST32_MIN
#else
#define SZT_MAX UT64_MAX
#define SZT_MIN UT64_MIN
#define SSZT_MAX ST64_MAX
#define SSZT_MIN ST64_MIN
#endif

/* type annotations */
#define PZ_IN        /* do not use, implicit */
#define PZ_OUT       /* parameter is written, not read */
#define PZ_INOUT     /* parameter is read and written */
#define PZ_OWN       /* pointer ownership is transferred */
#define PZ_BORROW    /* pointer ownership is not transferred, it must not be   \
                        freed by the receiver */
#define PZ_NONNULL   /* pointer can not be null */
#define PZ_NULLABLE  /* pointer can be null */
#define PZ_DEPRECATE /* should not be used in new code and should/will be      \
                        removed in the future */

#endif // PZ_TYPES_HPP