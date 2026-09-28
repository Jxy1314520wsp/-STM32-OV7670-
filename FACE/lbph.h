#ifndef __LBPH_H
#define __LBPH_H

#include <stdint.h>

/* ============================================================================
 * lbph.h - Local Binary Pattern Histograms (LBPH) face descriptor
 *   OpenCV-LBPH style: 4x4 cells x 59 uniform-LBP bins = 944 dims.
 * ==========================================================================*/

#define LBPH_FACE_SIZE   32
#define LBPH_CELLS_X     4
#define LBPH_CELLS_Y     4
#define LBPH_BINS        59
#define LBPH_CELLS       (LBPH_CELLS_X * LBPH_CELLS_Y)
#define LBPH_HIST_SIZE   (LBPH_CELLS * LBPH_BINS)   /* 944 */
#define LBPH_NORM_SUM    4096U

/* Must be called once before use. */
void LBPH_Init(void);

/* Build a normalized histogram from a 32x32 grayscale face. */
void LBPH_Histogram(const uint8_t *face32, uint16_t *hist);

/* Chi-square distance between two normalized histograms. */
uint32_t LBPH_ChiSquare(const uint16_t *a, const uint16_t *b);

/* Average n accumulated uint32 histograms into a uint16 histogram. */
void LBPH_Average(const uint32_t *sum, uint32_t n, uint16_t *out);

#endif /* __LBPH_H */
