#ifndef __FIXED_POINT_H__
#define __FIXED_POINT_H__

#define F (1 << 14) /** the f in the pq representation*/

/** to pq pseudo float */
#define int_to_pq(value) ((value)*F)

/** to int from pq value */
#define pq_to_int(pq) ((pq) / F)

/** round to the nearest integer */
#define round_pq(pq) ((pq) < 0 ? (pq)-F / 2 : (pq) + F / 2)

/** Multiplication of two pq numbers */
#define pq_mul(pq1, pq2) (((int64_t)pq1) * (pq2) / F)

/** Division of two pq numbers */
#define pq_div(pq1, pq2) (((int64_t)pq1) * F / (pq2))

typedef int pq1714;

#endif

