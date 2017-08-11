// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ROCRAND_MRG32K3A_H_
#define ROCRAND_MRG32K3A_H_

#ifndef FQUALIFIERS
#define FQUALIFIERS __forceinline__ __device__
#endif // FQUALIFIERS_

#include "rocrand_common.h"

// Thomas Bradley, Parallelisation Techniques for Random Number Generators
// https://www.nag.co.uk/IndustryArticles/gpu_gems_article.pdf

#define ROCRAND_MRG32K3A_POW32 4294967296
#define ROCRAND_MRG32K3A_M1 4294967087
#define ROCRAND_MRG32K3A_M1C 209
#define ROCRAND_MRG32K3A_M2 4294944443
#define ROCRAND_MRG32K3A_M2C 22853
#define ROCRAND_MRG32K3A_A12 1403580
#define ROCRAND_MRG32K3A_A13 (4294967087 -  810728)
#define ROCRAND_MRG32K3A_A13N 810728
#define ROCRAND_MRG32K3A_A21 527612
#define ROCRAND_MRG32K3A_A23 (4294944443 - 1370589)
#define ROCRAND_MRG32K3A_A23N 1370589

#define ROCRAND_MRG32K3A_DEFAULT_SEED 0x12345ULL

namespace rocrand_device {

class mrg32k3a_engine
{
public:
    struct mrg32k3a_state
    {
        unsigned long long g1[3];
        unsigned long long g2[3];

        #ifndef ROCRAND_DETAIL_MRG32K3A_BM_NOT_IN_STATE
        // The Box–Muller transform requires two inputs to convert uniformly
        // distributed real values [0; 1] to normally distributed real values
        // (with mean = 0, and stddev = 1). Often user wants only one
        // normally distributed number, to save performance and random
        // numbers the 2nd value is saved for future requests.
        unsigned int boxmuller_float_state; // is there a float in boxmuller_float
        unsigned int boxmuller_double_state; // is there a double in boxmuller_double
        float boxmuller_float; // normally distributed float
        double boxmuller_double; // normally distributed double
        #endif

        FQUALIFIERS
        ~mrg32k3a_state() { }
    };

    FQUALIFIERS
    mrg32k3a_engine()
    {
        this->seed(ROCRAND_MRG32K3A_DEFAULT_SEED, 0, 0);
    }

    /// Initializes the internal state of the PRNG using
    /// seed value \p seed, goes to \p subsequence -th subsequence,
    /// and skips \p offset random numbers.
    ///
    /// New seed value should not be zero. If \p seed_value is equal
    /// zero, value \p ROCRAND_MRG32K3A_DEFAULT_SEED is used instead.
    ///
    /// A subsequence is 2^67 numbers long.
    FQUALIFIERS
    mrg32k3a_engine(const unsigned long long seed,
                    const unsigned long long subsequence,
                    const unsigned long long offset)
    {
        this->seed(seed, subsequence, offset);
    }

    FQUALIFIERS
    ~mrg32k3a_engine() { }

    /// Reinitializes the internal state of the PRNG using new
    /// seed value \p seed_value, skips \p subsequence subsequences
    /// and \p offset random numbers.
    ///
    /// New seed value should not be zero. If \p seed_value is equal
    /// zero, value \p ROCRAND_MRG32K3A_DEFAULT_SEED is used instead.
    ///
    /// A subsequence is 2^67 numbers long.
    FQUALIFIERS
    void seed(unsigned long long seed_value,
              const unsigned long long subsequence,
              const unsigned long long offset)
    {
        if(seed_value == 0)
        {
            seed_value = ROCRAND_MRG32K3A_DEFAULT_SEED;
        }
        unsigned int x = (unsigned int) seed_value ^ 0x55555555UL;
        unsigned int y = (unsigned int) ((seed_value >> 32) ^ 0xAAAAAAAAUL);
        m_state.g1[0] = mod_mul_m1(x, seed_value);
        m_state.g1[1] = mod_mul_m1(y, seed_value);
        m_state.g1[2] = mod_mul_m1(x, seed_value);
        m_state.g2[0] = mod_mul_m2(y, seed_value);
        m_state.g2[1] = mod_mul_m2(x, seed_value);
        m_state.g2[2] = mod_mul_m2(y, seed_value);
        this->restart(subsequence, offset);
    }

    /// Advances the internal state to skip \p offset numbers.
    FQUALIFIERS
    void discard(unsigned long long offset)
    {
        this->discard_impl(offset);
    }

    /// Advances the internal state to skip \p subsequence subsequences.
    /// A subsequence is 2^67 numbers long.
    FQUALIFIERS
    void discard_subsequence(unsigned long long subsequence)
    {
        this->discard_subsequence_impl(subsequence);
    }

    /// Advances the internal state to skip \p sequence sequences.
    /// A sequence is 2^127 numbers long.
    FQUALIFIERS
    void discard_sequence(unsigned long long sequence)
    {
        this->discard_sequence_impl(sequence);
    }

    FQUALIFIERS
    void restart(const unsigned long long subsequence,
                 const unsigned long long offset)
    {
        #ifndef ROCRAND_DETAIL_MRG32K3A_BM_NOT_IN_STATE
        m_state.boxmuller_float_state = 0;
        m_state.boxmuller_double_state = 0;
        #endif
        this->discard_subsequence_impl(subsequence);
        this->discard_impl(offset);
    }

    FQUALIFIERS
    unsigned long long operator()()
    {
        return this->next();
    }

    FQUALIFIERS
    unsigned long long next()
    {
        unsigned long long p;

        p = ROCRAND_MRG32K3A_A12 * m_state.g1[1] + ROCRAND_MRG32K3A_A13N
            * (ROCRAND_MRG32K3A_M1 - m_state.g1[0]);
        p = mod_m1(p);

        m_state.g1[0] = m_state.g1[1]; m_state.g1[1] = m_state.g1[2]; m_state.g1[2] = p;

        p = ROCRAND_MRG32K3A_A21 * m_state.g2[2] + ROCRAND_MRG32K3A_A23N
            * (ROCRAND_MRG32K3A_M2 - m_state.g2[0]);
        p = mod_m2(p);

        m_state.g2[0] = m_state.g2[1]; m_state.g2[1] = m_state.g2[2]; m_state.g2[2] = p;

        p = m_state.g1[2] - m_state.g2[2];
        if (m_state.g1[2] <= m_state.g2[2])
            p += ROCRAND_MRG32K3A_M1;  // 0 < p <= M1

        return p;
    }

protected:
    // Advances the internal state to skip \p offset numbers.
    // DOES NOT CALCULATE NEW ULONGLONG
    FQUALIFIERS
    void discard_impl(unsigned long long offset)
    {
        discard_state(offset);
    }

    // DOES NOT CALCULATE NEW ULONGLONG
    FQUALIFIERS
    void discard_subsequence_impl(unsigned long long subsequence)
    {
        unsigned long long A1p67[9] =
        {
            82758667, 1871391091, 4127413238,
            3672831523, 69195019, 1871391091,
            3672091415, 3528743235, 69195019
        };
        unsigned long long A2p67[9] =
        {
            1511326704, 3759209742, 1610795712,
            4292754251, 1511326704, 3889917532,
            3859662829, 4292754251, 3708466080
        };

        while(subsequence > 0) {
            if (subsequence % 2 == 1) {
                mod_mat_vec(A1p67, m_state.g1, ROCRAND_MRG32K3A_M1);
                mod_mat_vec(A2p67, m_state.g2, ROCRAND_MRG32K3A_M2);
            }
            subsequence = subsequence / 2;

            mod_mat_sq(A1p67, ROCRAND_MRG32K3A_M1);
            mod_mat_sq(A2p67, ROCRAND_MRG32K3A_M2);
        }
    }

    // DOES NOT CALCULATE NEW ULONGLONG
    FQUALIFIERS
    void discard_sequence_impl(unsigned long long sequence)
    {
        unsigned long long A1p127[9] =
        {
            2427906178, 3580155704, 949770784,
            226153695,  1230515664, 3580155704,
            1988835001, 986791581,  1230515664
        };
        unsigned long long A2p127[9] =
        {
            1464411153, 277697599,  1610723613,
            32183930,   1464411153, 1022607788,
            2824425944, 32183930,   2093834863
        };

        while(sequence > 0) {
            if (sequence % 2 == 1) {
                mod_mat_vec(A1p127, m_state.g1, ROCRAND_MRG32K3A_M1);
                mod_mat_vec(A2p127, m_state.g2, ROCRAND_MRG32K3A_M2);
            }
            sequence = sequence / 2;

            mod_mat_sq(A1p127, ROCRAND_MRG32K3A_M1);
            mod_mat_sq(A2p127, ROCRAND_MRG32K3A_M2);
        }
    }

    // Advances the internal state by offset times.
    // DOES NOT CALCULATE NEW ULONGLONG
    FQUALIFIERS
    void discard_state(unsigned long long offset)
    {
        unsigned long long A1[9] =
        {
            0,                    1,   0,
            0,                    0,   1,
            ROCRAND_MRG32K3A_A13, ROCRAND_MRG32K3A_A12, 0
        };
        unsigned long long A2[9] =
        {
            0,                    1, 0,
            0,                    0, 1,
            ROCRAND_MRG32K3A_A23, 0, ROCRAND_MRG32K3A_A21
        };

        while(offset > 0) {
            if (offset % 2 == 1) {
                mod_mat_vec(A1, m_state.g1, ROCRAND_MRG32K3A_M1);
                mod_mat_vec(A2, m_state.g2, ROCRAND_MRG32K3A_M2);
            }
            offset = offset / 2;

            mod_mat_sq(A1, ROCRAND_MRG32K3A_M1);
            mod_mat_sq(A2, ROCRAND_MRG32K3A_M2);
        }
    }

    // Advances the internal state to the next state
    // DOES NOT CALCULATE NEW ULONGLONG
    FQUALIFIERS
    void discard_state()
    {
        discard_state(1);
    }

private:
    inline __device__ __host__
    void mod_mat_vec(unsigned long long * A,
                     unsigned long long * s,
                     unsigned long long m)
    {
        unsigned long long x[3];
        for (size_t i = 0; i < 3; ++i) {
            x[i] = 0;
            for (size_t j = 0; j < 3; j++)
                x[i] = (A[i + 3 * j] * s[j] + x[i]) % m;
        }
        for (size_t i = 0; i < 3; ++i)
            s[i] = x[i];
    }

    inline __device__ __host__
    void mod_mat_sq(unsigned long long * A,
                    unsigned long long m)
    {
        unsigned long long x[9];
        unsigned long long a;
        for (size_t i = 0; i < 3; i++) {
            for (size_t j = 0; j < 3; j++) {
                a = 0;
                for (size_t k = 0; k < 3; k++) {
                    a += (A[i + 3 * k] * A[k + 3 * j]) % m;
                }
                x[i + 3 * j] = a % m;
            }
        }
        for (size_t i = 0; i < 3; i++) {
            A[i + 3 * 0] = x[i + 3 * 0];
            A[i + 3 * 1] = x[i + 3 * 1];
            A[i + 3 * 2] = x[i + 3 * 2];
        }
    }

    inline __host__ __device__
    unsigned long long mod_mul_m1(unsigned int i,
                                  unsigned long long j)
    {
        long long hi, lo, temp1, temp2;

        hi = i / 131072;
        lo = i - (hi * 131072);
        temp1 = mod_m1(hi * j) * 131072;
        temp2 = mod_m1(lo * j);
        lo = mod_m1(temp1 + temp2);

        if (lo < 0)
            lo += ROCRAND_MRG32K3A_M1;
        return lo;
    }

    inline __host__ __device__
    unsigned long long mod_m1(unsigned long long i)
    {
        unsigned long long p;
        p = (i & (ROCRAND_MRG32K3A_POW32 - 1)) + (i >> 32)
            * ROCRAND_MRG32K3A_M1C;
        if (p >= ROCRAND_MRG32K3A_M1)
            p -= ROCRAND_MRG32K3A_M1;

        return p;
    }

    inline __host__ __device__
    unsigned long long mod_mul_m2(unsigned int i,
                                  unsigned long long j)
    {
        long long hi, lo, temp1, temp2;

        hi = i / 131072;
        lo = i - (hi * 131072);
        temp1 = mod_m2(hi * j) * 131072;
        temp2 = mod_m2(lo * j);
        lo = mod_m2(temp1 + temp2);

        if (lo < 0)
            lo += ROCRAND_MRG32K3A_M2;
        return lo;
    }

    inline __host__ __device__
    unsigned long long mod_m2(unsigned long long i)
    {
        unsigned long long p;
        p = (i & (ROCRAND_MRG32K3A_POW32 - 1)) + (i >> 32)
            * ROCRAND_MRG32K3A_M2C;
        p = (p & (ROCRAND_MRG32K3A_POW32 - 1)) + (p >> 32)
            * ROCRAND_MRG32K3A_M2C;
        if (p >= ROCRAND_MRG32K3A_M2)
            p -= ROCRAND_MRG32K3A_M2;

        return p;
    }

protected:
    // State
    mrg32k3a_state m_state;

    #ifndef ROCRAND_DETAIL_MRG32K3A_BM_NOT_IN_STATE
    friend class detail::engine_boxmuller_helper<mrg32k3a_engine>;
    #endif

}; // mrg32k3a_engine class

} // end namespace rocrand_device

/** \addtogroup device
 *
 *  @{
 */

/// \cond ROCRAND_KERNEL_DOCS_TYPEDEFS
typedef rocrand_device::mrg32k3a_engine rocrand_state_mrg32k3a;
/// \endcond

/**
 * \brief Initialize MRG32K3A state.
 *
 * Initialize MRG32K3A state in \p state with the given \p seed, \p subsequence,
 * and \p offset.
 *
 * \param seed - Value to use as a seed
 * \param subsequence - Subsequence to start at
 * \param offset - Absolute offset into sequence
 * \param state - Pointer to state to initialize
 */
FQUALIFIERS
void rocrand_init(const unsigned long long seed,
                  const unsigned long long subsequence,
                  const unsigned long long offset,
                  rocrand_state_mrg32k3a * state)
{
    *state = rocrand_state_mrg32k3a(seed, subsequence, offset);
}

/**
 * \brief Return pseudorandom value (64-bit) from MRG32K3A generator.
 *
 * Return pseudorandom value (64-bit) from the MRG32K3A generator in \p state,
 * increment position of generator by one.
 *
 * \param state - Pointer to state to update
 *
 * \return pseudorandom value (64-bit) as an unsigned long long
 */
FQUALIFIERS
unsigned long long rocrand(rocrand_state_mrg32k3a * state)
{
    return state->next();
}

/**
 * \brief Update MRG32K3A state to skip ahead by \p offset elements.
 *
 * Update the MRG32K3A state in \p state to skip ahead by \p offset elements.
 *
 * \param offset - Number of elements to skip
 * \param state - Pointer to state to update
 */
FQUALIFIERS
void skipahead(unsigned long long offset, rocrand_state_mrg32k3a * state)
{
    return state->discard(offset);
}

/**
 * \brief Update MRG32K3A state to skip ahead by \p subsequence subsequences.
 *
 * Update the MRG32K3A state in \p state to skip ahead by \p subsequence subsequences.
 * Each subsequence is 2^67 numbers long.
 *
 * \param subsequence - Number of subsequences to skip
 * \param state - Pointer to state to update
 */
FQUALIFIERS
void skipahead_subsequence(unsigned long long subsequence, rocrand_state_mrg32k3a * state)
{
    return state->discard_subsequence(subsequence);
}

/**
 * \brief Update MRG32K3A state to skip ahead by \p sequence sequences.
 *
 * Update the MRG32K3A state in \p state to skip ahead by \p sequence sequences.
 * Each sequence is 2^127 numbers long.
 *
 * \param sequence - Number of sequences to skip
 * \param state - Pointer to state to update
 */
FQUALIFIERS
void skipahead_sequence(unsigned long long sequence, rocrand_state_mrg32k3a * state)
{
    return state->discard_sequence(sequence);
}

#endif // ROCRAND_MRG32K3A_H_

/** @} */ // end of group device