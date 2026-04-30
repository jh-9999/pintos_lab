#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* mlfqs 구현을 위한 17.14 고정소수점 헬퍼 함수. */

#define FP_F (1 << 14)

typedef int fp32_t;

static inline fp32_t
fp (const int n) {
	return n * FP_F;
}

static inline int
fp_int_trunc (const fp32_t x) {
	return x / FP_F;
}

static inline int
fp_int_rnd (const fp32_t x) {
	if (x >= 0)
		return (x + FP_F / 2) / FP_F;
	return (x - FP_F / 2) / FP_F;
}

static inline fp32_t
fp_add (const fp32_t x, const fp32_t y) {
	return x + y;
}

static inline fp32_t
fp_sub (const fp32_t x, const fp32_t y) {
	return x - y;
}

static inline fp32_t
fp_add_i (const fp32_t x, const int n) {
	return x + n * FP_F;
}

static inline fp32_t
fp_sub_i (const fp32_t x, const int n) {
	return x - n * FP_F;
}

static inline fp32_t
fp_mul (const fp32_t x, const fp32_t y) {
	return ((int64_t) x) * y / FP_F;
}

static inline fp32_t
fp_mul_i (const fp32_t x, const int n) {
	return x * n;
}

static inline fp32_t
fp_div (const fp32_t x, const fp32_t y) {
	return ((int64_t) x) * FP_F / y;
}

static inline fp32_t
fp_div_i (const fp32_t x, const int n) {
	return x / n;
}

#endif /* threads/fixed-point.h */
