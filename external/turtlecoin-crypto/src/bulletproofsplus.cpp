// Copyright (c) 2020, Brandon Lehmann <brandonlehmann@gmail.com>
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Adapted from Python code by Sarang Noether found at
// https://github.com/SarangNoether/skunkworks/tree/pybullet-plus

#include "bulletproofsplus.h"

static const crypto_scalar_t BULLETPROOFS_PLUS_DOMAIN_0 = {
    0x3c, 0x2d, 0x2d, 0x20, 0x69, 0x62, 0x75, 0x72, 0x6e, 0x6d, 0x79, 0x63, 0x64, 0x40, 0x74, 0x75,
    0x72, 0x74, 0x6c, 0x65, 0x63, 0x6f, 0x69, 0x6e, 0x2e, 0x64, 0x65, 0x76, 0x20, 0x2d, 0x2d, 0x3e};

static const auto BULLETPROOFS_PLUS_DOMAIN_1 = TurtleCoinCrypto::hash_to_point(BULLETPROOFS_PLUS_DOMAIN_0);

static const auto BULLETPROOFS_PLUS_DOMAIN_2 = TurtleCoinCrypto::hash_to_point(BULLETPROOFS_PLUS_DOMAIN_1);

/**
 * Generates the general bulletproof exponents up through the given count
 * to aid in the speed of proving and verifying, the exponents are cached
 * and if more are requested, then they are generated on demand; otherwise,
 * if less are requested, we supply a slice of the cached entries thus
 * avoiding doing a whole bunch of hashing each generation and verification
 * @param count
 * @return
 */
static std::tuple<crypto_point_vector_t, crypto_point_vector_t> generate_exponents(size_t count)
{
    static crypto_point_vector_t L_cached, R_cached;

    if (count == L_cached.size() && count == R_cached.size())
        return {L_cached, R_cached};

    if (count < L_cached.size())
        return {L_cached.slice(0, count), R_cached.slice(0, count)};

    struct
    {
        crypto_point_t base_point;
        uint64_t index = 0;
    } buffer;

    for (size_t i = L_cached.size(); i < count; ++i)
    {
        buffer.index = i;

        buffer.base_point = BULLETPROOFS_PLUS_DOMAIN_1;

        L_cached.append(TurtleCoinCrypto::G);
        R_cached.append(TurtleCoinCrypto::H);

        // L_cached.append(TurtleCoinCrypto::hash_to_point(&buffer, sizeof(buffer)));

        buffer.base_point = BULLETPROOFS_PLUS_DOMAIN_2;

        // R_cached.append(TurtleCoinCrypto::hash_to_point(&buffer, sizeof(buffer)));
    }

    return {L_cached, R_cached};
}

namespace TurtleCoinCrypto::RangeProofs::BulletproofsPlus
{
    /**
     * Helps to calculate an inner product round
     */
    struct InnerProductRound
    {
        InnerProductRound(
            crypto_point_vector_t Gi,
            crypto_point_vector_t Hi,
            crypto_scalar_vector_t a,
            crypto_scalar_vector_t b,
            const crypto_scalar_t alpha,
            const crypto_scalar_t y,
            const TurtleCoinCrypto::crypto_scalar_transcript_t &tr):
            Gi(std::move(Gi)), Hi(std::move(Hi)), a(std::move(a)), b(std::move(b)), alpha(alpha), y(y), tr(tr)
        {
        }

        std::tuple<
            crypto_point_t,
            crypto_point_t,
            crypto_scalar_t,
            crypto_scalar_t,
            crypto_scalar_t,
            std::vector<crypto_point_t>,
            std::vector<crypto_point_t>>
            compute()
        {
            if (done)
                return {A, B, r1, s1, d1, L.points, R.points};

            auto n = Gi.size();

            while (n > 1)
            {
                n /= 2;

                const auto a1 = a.slice(0, n), a2 = a.slice(n, a.size());

                const auto b1 = b.slice(0, n), b2 = b.slice(n, b.size());

                const auto G1 = Gi.slice(0, n), G2 = Gi.slice(n, Gi.size());

                const auto H1 = Hi.slice(0, n), H2 = Hi.slice(n, Hi.size());

                const auto dL = TurtleCoinCrypto::random_scalar(), dR = TurtleCoinCrypto::random_scalar();

                const auto cL = InnerProductRound::wip(a1, b2, y);

                const auto cR = InnerProductRound::wip(a2 * y.pow(n), b1, y);

                const auto ypow = y.pow(n), yinvpow = y.invert().pow(n);

                L.append(
                    TurtleCoinCrypto::INV_EIGHT
                    * ((a1 * yinvpow).inner_product(G2) + b2.inner_product(H1) + (cL * TurtleCoinCrypto::H) + (dL * TurtleCoinCrypto::G)));

                R.append(
                    TurtleCoinCrypto::INV_EIGHT
                    * ((a2 * ypow).inner_product(G1) + b1.inner_product(H2) + (cR * TurtleCoinCrypto::H) + (dR * TurtleCoinCrypto::G)));

                tr.update(L.back());

                tr.update(R.back());

                const auto x = tr.challenge();

                if (x == TurtleCoinCrypto::ZERO)
                    throw std::runtime_error("x cannot be zero");

                Gi = (G1 * x.invert()) + (G2 * x * yinvpow);

                Hi = (H1 * x) + (H2 * x.invert());

                a = (a1 * x) + (a2 * ypow * x.invert());

                b = (b1 * x.invert()) + (b2 * x);

                alpha = (dL * x.squared()) + alpha + (dR * x.invert().squared());
            }

        try_again:
            const auto r = TurtleCoinCrypto::random_scalar(), s = TurtleCoinCrypto::random_scalar(), d = TurtleCoinCrypto::random_scalar(),
                       eta = TurtleCoinCrypto::random_scalar();

            A = TurtleCoinCrypto::INV_EIGHT
                * ((r * Gi[0]) + (s * Hi[0]) + (((r * y * b[0]) + (s * y * a[0])) * TurtleCoinCrypto::H) + (d * TurtleCoinCrypto::G));

            B = TurtleCoinCrypto::INV_EIGHT * (((r * y * s) * TurtleCoinCrypto::H) + (eta * TurtleCoinCrypto::G));

            tr.update(A);

            tr.update(B);

            const auto x = tr.challenge();

            if (x == TurtleCoinCrypto::ZERO)
                goto try_again;

            r1 = r + (a[0] * x);

            s1 = s + (b[0] * x);

            d1 = eta + (d * x) + (alpha * x.squared());

            done = true;

            return {A, B, r1, s1, d1, L.points, R.points};
        }

      private:
        static crypto_scalar_t
            wip(const crypto_scalar_vector_t &a, const crypto_scalar_vector_t &b, const crypto_scalar_t &y)
        {
            if (a.size() != b.size())
                throw std::invalid_argument("weighted inner product vectors must be of the same size");

            auto r = TurtleCoinCrypto::ZERO;

            for (size_t i = 0; i < a.size(); ++i)
                r += a[i] * y.pow(i + 1) * b[i];

            return r;
        }

        bool done = false;
        TurtleCoinCrypto::crypto_scalar_transcript_t tr;
        crypto_point_vector_t Gi, Hi, L, R;
        crypto_point_t A, B;
        crypto_scalar_vector_t a, b;
        crypto_scalar_t alpha, y, r1, s1, d1;
    };

    std::tuple<crypto_bulletproof_plus_t, std::vector<crypto_pedersen_commitment_t>> prove(
        const std::vector<uint64_t> &amounts,
        const std::vector<crypto_blinding_factor_t> &blinding_factors,
        size_t N)
    {
        if (N == 0)
            throw std::range_error("N must be at least 1-bit");

        if (N > 64)
            throw std::range_error("N must not exceed 64-bits");

        if (amounts.size() != blinding_factors.size())
            throw std::runtime_error("amounts and gamma must be the same size");

        if (amounts.empty())
            throw std::runtime_error("amounts is empty");

        for (const auto &blinding_factor : blinding_factors)
            if (!blinding_factor.check())
                throw std::runtime_error("invalid gamma input");


        const auto M = amounts.size();

        N = TurtleCoinCrypto::pow2_round(N);

        const auto MN = M * N;

        const auto [Gi, Hi] = generate_exponents(MN);

        const auto one_MN = crypto_scalar_vector_t(MN, TurtleCoinCrypto::ONE);

        crypto_point_vector_t V;

        crypto_scalar_vector_t aL, aR;

        for (size_t i = 0; i < M; ++i)
        {
            V.append(TurtleCoinCrypto::RingCT::generate_pedersen_commitment(blinding_factors[i], amounts[i]));

            aL.extend(crypto_scalar_t(amounts[i]).to_bits(N));
        }

        aR = aL - one_MN;

    try_again:
        crypto_scalar_transcript_t tr(BULLETPROOFS_PLUS_DOMAIN_0);

        const auto alpha = TurtleCoinCrypto::random_scalar();

        tr.update(V.points);

        const auto A = TurtleCoinCrypto::INV_EIGHT * (aL.inner_product(Gi) + aR.inner_product(Hi) + (alpha * G));

        tr.update(A);

        const auto y = tr.challenge();

        if (y == TurtleCoinCrypto::ZERO)
            goto try_again;

        tr.update(y);

        const auto z = tr.challenge();

        if (z == TurtleCoinCrypto::ZERO)
            goto try_again;

        crypto_scalar_vector_t d;

        for (size_t j = 0; j < M; ++j)
            for (size_t i = 0; i < N; ++i)
                d.append(z.pow(2 * (j + 1)) * TurtleCoinCrypto::TWO.pow(i));

        const auto aL1 = aL - (one_MN * z);

        const auto yexp = crypto_scalar_vector_t(y.pow_expand(MN, true, false));

        const auto aR1 = aR + (d * yexp) + (one_MN * z);

        auto alpha1 = alpha;

        for (size_t j = 0; j < M; ++j)
        {
            const auto ypow = y.pow(MN + 1);

            alpha1 += z.pow(2 * (j + 1)) * blinding_factors[j] * ypow;
        }

        try
        {
            const auto [A1, B, r1, s1, d1, L, R] = InnerProductRound(Gi, Hi, aL1, aR1, alpha1, y, tr).compute();

            return {crypto_bulletproof_plus_t(A, A1, B, r1, s1, d1, L, R), V.points};
        }
        catch (...)
        {
            goto try_again;
        }
    }

    bool verify(
        const std::vector<crypto_bulletproof_plus_t> &proofs,
        const std::vector<std::vector<crypto_pedersen_commitment_t>> &commitments,
        size_t N)
    {
        if (N == 0)
            throw std::range_error("N must be at least 1-bit");

        if (N > 64)
            throw std::range_error("N must not exceed 64-bits");

        if (proofs.size() != commitments.size())
            return false;

        N = TurtleCoinCrypto::pow2_round(N);

        size_t max_M = 0;

        for (const auto &proof : proofs)
            max_M = std::max(max_M, proof.L.size());

        const size_t max_MN = size_t(TurtleCoinCrypto::TWO.pow(max_M).to_uint64_t());

        const auto [Gi, Hi] = generate_exponents(max_MN);

        auto G_scalar = TurtleCoinCrypto::ZERO, H_scalar = TurtleCoinCrypto::ZERO;

        crypto_scalar_vector_t Gi_scalars(max_MN, TurtleCoinCrypto::ZERO), Hi_scalars(max_MN, TurtleCoinCrypto::ZERO);

        crypto_scalar_vector_t scalars;

        crypto_point_vector_t points;

        for (size_t ii = 0; ii < proofs.size(); ++ii)
        {
            const auto proof = proofs[ii];

            if (commitments[ii].empty())
                return false;

            if (proof.L.empty())
                return false;

            if (proof.L.size() != proof.R.size())
                return false;

            crypto_scalar_transcript_t tr(BULLETPROOFS_PLUS_DOMAIN_0);

            const auto M = size_t(TurtleCoinCrypto::TWO.pow(proof.L.size()).to_uint64_t()) / N;

            const auto MN = M * N;

            const auto one_MN = crypto_scalar_vector_t(MN, TurtleCoinCrypto::ONE);

            const auto weight = TurtleCoinCrypto::random_scalar();

            tr.update(commitments[ii]);

            tr.update(proof.A);

            const auto y = tr.challenge();

            if (y == TurtleCoinCrypto::ZERO)
                return false;

            tr.update(y);

            const auto z = tr.challenge();

            if (z == TurtleCoinCrypto::ZERO)
                return false;

            crypto_scalar_vector_t d, challenges;

            for (size_t j = 0; j < M; ++j)
                for (size_t i = 0; i < N; ++i)
                    d.append(z.pow(2 * (j + 1)) * TurtleCoinCrypto::TWO.pow(i));

            for (size_t j = 0; j < proof.L.size(); ++j)
            {
                tr.update(proof.L[j]);

                tr.update(proof.R[j]);

                const auto challenge = tr.challenge();

                if (challenge == TurtleCoinCrypto::ZERO)
                    return false;

                challenges.append(challenge);
            }

            const auto challenges_inv = challenges.invert();

            tr.update(proof.A1);

            tr.update(proof.B);

            const auto x = tr.challenge();

            if (x == TurtleCoinCrypto::ZERO)
                return false;

            for (size_t i = 0; i < MN; ++i)
            {
                auto index = i;

                auto g = proof.r1 * x * y.invert().pow(i);

                auto h = proof.s1 * x;

                for (size_t j = proof.L.size(); j-- > 0;)
                {
                    auto J = challenges.size() - j - 1;

                    const size_t base_power = size_t(TurtleCoinCrypto::TWO.pow(j).to_uint64_t());

                    if (index / base_power == 0)
                    {
                        g *= challenges_inv[J];

                        h *= challenges[J];
                    }
                    else
                    {
                        g *= challenges[J];

                        h *= challenges_inv[J];

                        index -= base_power;
                    }
                }

                Gi_scalars[i] += weight * (g + (x.squared() * z));

                Hi_scalars[i] += weight * (h - (x.squared() * (d[i] * y.pow(MN - i) + z)));
            }

            for (size_t j = 0; j < M; ++j)
            {
                scalars.append(weight * (x.squared().negate() * z.pow(2 * (j + 1)) * y.pow(MN + 1)));

                points.append(commitments[ii][j]);
            }

            H_scalar += weight
                        * ((proof.r1 * y * proof.s1)
                           + (x.squared()
                              * ((y.pow(MN + 1) * z * one_MN.inner_product(d))
                                 + ((z.squared() - z) * one_MN.inner_product(y.pow_expand(MN, false, false))))));

            G_scalar += weight * proof.d1;

            scalars.append(weight * x.negate());

            points.append(TurtleCoinCrypto::EIGHT * proof.A1);

            scalars.append(weight.negate());

            points.append(TurtleCoinCrypto::EIGHT * proof.B);

            scalars.append(weight * x.squared().negate());

            points.append(TurtleCoinCrypto::EIGHT * proof.A);

            for (size_t j = 0; j < proof.L.size(); ++j)
            {
                scalars.append(challenges[j].squared() * weight * x.squared().negate());

                points.append(TurtleCoinCrypto::EIGHT * proof.L[j]);

                scalars.append(challenges_inv[j].squared() * weight * x.squared().negate());

                points.append(TurtleCoinCrypto::EIGHT * proof.R[j]);
            }
        }

        scalars.append(G_scalar);

        points.append(TurtleCoinCrypto::G);

        scalars.append(H_scalar);

        points.append(TurtleCoinCrypto::H);

        for (size_t i = 0; i < max_MN; ++i)
        {
            scalars.append(Gi_scalars[i]);

            points.append(Gi[i]);

            scalars.append(Hi_scalars[i]);

            points.append(Hi[i]);
        }

        return scalars.inner_product(points) == TurtleCoinCrypto::Z;
    }

    bool verify(
        const crypto_bulletproof_plus_t &proof,
        const std::vector<crypto_pedersen_commitment_t> &commitments,
        size_t N)
    {
        return verify(
            std::vector<crypto_bulletproof_plus_t>(1, proof),
            std::vector<std::vector<crypto_pedersen_commitment_t>>(1, commitments),
            N);
    }
} // namespace TurtleCoinCrypto::RangeProofs::BulletproofsPlus
