#include "FLNoise.h"
#include <cmath>
#include <random>
#include <algorithm>
#include "..\MathHelper\FLMathHelper.h"

namespace FireFlame
{
// Perlin Noise Data
static constexpr int NoisePermSize = 256;
static int NoisePerm[2 * NoisePermSize] = 
{
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140,
    36, 103, 30, 69, 142,
    // Remainder of the noise permutation table
    8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62,
    94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174,
    20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77,
    146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55,
    46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76,
    132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100,
    109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147,
    118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28,
    42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101,
    155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232,
    178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12,
    191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31,
    181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66,
    215, 61, 156, 180, 151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194,
    233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6,
    148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32,
    57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74,
    165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60,
    211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25,
    63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135,
    130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226,
    250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59,
    227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2,
    44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19,
    98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251,
    34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249,
    14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115,
    121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72,
    243, 141, 128, 195, 78, 66, 215, 61, 156, 180 
};

void Noise::Permutate(unsigned seed)
{
    for (int i = 0; i < NoisePermSize; i++)
    {
        NoisePerm[i] = i;
    }
    std::shuffle(std::begin(NoisePerm), std::end(NoisePerm), std::default_random_engine(seed));
    for (int i = NoisePermSize; i < NoisePermSize*2; i++)
    {
        NoisePerm[i] = NoisePerm[i - NoisePermSize];
    }
}

inline Noise::Float Grad(int x, int y, int z, Noise::Float dx, Noise::Float dy, Noise::Float dz) {
    int h = NoisePerm[NoisePerm[NoisePerm[x] + y] + z];
    h &= 15;
    Noise::Float u = h < 8 || h == 12 || h == 13 ? dx : dy;
    Noise::Float v = h < 4 || h == 12 || h == 13 ? dy : dz;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

inline Noise::Float NoiseWeight(Noise::Float t) {
    Noise::Float t3 = t * t * t;
    Noise::Float t4 = t3 * t;
    return 6 * t4 * t - 15 * t4 + 10 * t3;
}

Noise::Float Noise::Evaluate(Float x, Float y, Float z) 
{
    // Compute noise cell coordinates and offsets
    int ix = (int)std::floor(x), iy = (int)std::floor(y), iz = (int)std::floor(z);
    Float dx = x - ix, dy = y - iy, dz = z - iz;

    // Compute gradient weights
    ix &= NoisePermSize - 1;
    iy &= NoisePermSize - 1;
    iz &= NoisePermSize - 1;
    Float w000 = Grad(ix, iy, iz, dx, dy, dz);
    Float w100 = Grad(ix + 1, iy, iz, dx - 1, dy, dz);
    Float w010 = Grad(ix, iy + 1, iz, dx, dy - 1, dz);
    Float w110 = Grad(ix + 1, iy + 1, iz, dx - 1, dy - 1, dz);
    Float w001 = Grad(ix, iy, iz + 1, dx, dy, dz - 1);
    Float w101 = Grad(ix + 1, iy, iz + 1, dx - 1, dy, dz - 1);
    Float w011 = Grad(ix, iy + 1, iz + 1, dx, dy - 1, dz - 1);
    Float w111 = Grad(ix + 1, iy + 1, iz + 1, dx - 1, dy - 1, dz - 1);

    // Compute trilinear interpolation of weights
    Float wx = NoiseWeight(dx), wy = NoiseWeight(dy), wz = NoiseWeight(dz);
    Float x00 = MathHelper::Lerp(wx, w000, w100);
    Float x10 = MathHelper::Lerp(wx, w010, w110);
    Float x01 = MathHelper::Lerp(wx, w001, w101);
    Float x11 = MathHelper::Lerp(wx, w011, w111);
    Float y0 = MathHelper::Lerp(wy, x00, x10);
    Float y1 = MathHelper::Lerp(wy, x01, x11);
    return MathHelper::Lerp(wz, y0, y1);
}

Noise::Float Noise::FBm
(
    const Vector3f &p, 
    const Vector3f &dpdx, const Vector3f &dpdy,
    Float omega, int maxOctaves
) 
{
    // Compute number of octaves for antialiased FBm
    Float len2 = std::max(dpdx.LengthSquared(), dpdy.LengthSquared());
    Float n = MathHelper::Clamp(-1 - .5f * MathHelper::Log2(len2), 0, maxOctaves);
    int nInt = (int)std::floor(n);

    // Compute sum of octaves of noise for FBm
    Float sum = 0, lambda = 1, o = 1;
    for (int i = 0; i < nInt; ++i) {
        sum += o * EvaluatePoint(lambda * p);
        lambda *= 1.99f;
        o *= omega;
    }
    Float nPartial = n - nInt;
    sum += o * MathHelper::SmoothStep(.3f, .7f, nPartial) * EvaluatePoint(lambda * p);
    return sum;
}

Noise::Float Noise::FBm
(
    const Vector3f &p,
    Float omega, int maxOctaves
)
{
    Float n = (Float)maxOctaves;
    int nInt = (int)std::floor(n);

    // Compute sum of octaves of noise for FBm
    Float sum = 0, lambda = 1, o = 1;
    for (int i = 0; i < nInt; ++i) {
        sum += o * EvaluatePoint(lambda * p);
        lambda *= 1.99f;
        o *= omega;
    }
    Float nPartial = n - nInt;
    sum += o * MathHelper::SmoothStep(.3f, .7f, nPartial) * EvaluatePoint(lambda * p);
    return sum;
}

Noise::Float Noise::Turbulence
(
    const Vector3f &p, 
    const Vector3f &dpdx, const Vector3f &dpdy,
    Float omega, int maxOctaves
) 
{
    // Compute number of octaves for antialiased FBm
    Float len2 = std::max(dpdx.LengthSquared(), dpdy.LengthSquared());
    Float n = MathHelper::Clamp(-1 - .5f * MathHelper::Log2(len2), 0, maxOctaves);
    int nInt = (int)std::floor(n);

    // Compute sum of octaves of noise for turbulence
    Float sum = 0, lambda = 1, o = 1;
    for (int i = 0; i < nInt; ++i) {
        sum += o * std::abs(EvaluatePoint(lambda * p));
        lambda *= 1.99f;
        o *= omega;
    }

    // Account for contributions of clamped octaves in turbulence
    Float nPartial = n - nInt;
    sum += o * MathHelper::Lerp(MathHelper::SmoothStep(.3f, .7f, nPartial), 0.2f,
        std::abs(EvaluatePoint(lambda * p)));
    for (int i = nInt; i < maxOctaves; ++i) {
        sum += o * 0.2f;
        o *= omega;
    }
    return sum;
}

Noise::Float Noise::Turbulence
(
    const Vector3f &p,
    Float omega, int maxOctaves
)
{
    Float n = (Float)maxOctaves;
    int nInt = (int)std::floor(n);

    // Compute sum of octaves of noise for turbulence
    Float sum = 0, lambda = 1, o = 1;
    for (int i = 0; i < nInt; ++i) {
        sum += o * std::abs(EvaluatePoint(lambda * p));
        lambda *= 1.99f;
        o *= omega;
    }

    // Account for contributions of clamped octaves in turbulence
    Float nPartial = n - nInt;
    sum += o * MathHelper::Lerp(MathHelper::SmoothStep(.3f, .7f, nPartial), 0.2f,
        std::abs(EvaluatePoint(lambda * p)));
    for (int i = nInt; i < maxOctaves; ++i) {
        sum += o * 0.2f;
        o *= omega;
    }
    return sum;
}
}