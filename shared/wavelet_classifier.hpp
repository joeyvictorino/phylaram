#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace phylaram {

// 5-element quantized eigen-space S from Haar DWT Level-1
// S = {-√2, -1/√2, 0, +1/√2, +√2} mapped to tokens 0, 1, 2, 3, 4
constexpr int WAVELET_NUM_STATES = 5;
constexpr uint8_t TOKEN_NEG_FULL = 0;  // -√2
constexpr uint8_t TOKEN_NEG_HALF = 1;  // -1/√2
constexpr uint8_t TOKEN_ZERO     = 2;  //  0 (identity)
constexpr uint8_t TOKEN_POS_HALF = 3;  // +1/√2
constexpr uint8_t TOKEN_POS_FULL = 4;  // +√2

enum class MemoryRegionCategory {
    ZeroPool,               // >98% identity tokens (zeroed / unallocated pool)
    StructuralExecutable,   // Format-determined (PE headers, page tables, compiled code)
    ConstrainedData,        // Soft structure (ASCII text, structured records, tables)
    HighEntropyPayload      // High transition energy (compressed, encrypted, or shellcode)
};

struct WaveletEntropyMetrics {
    uint64_t totalBytesAnalyzed = 0;
    float identityDensity = 0.0f;       // % of identity (token 2) transitions
    float transitionEnergy = 0.0f;      // Mean |token - 2|
    float energyVariance = 0.0f;        // Variance of transition energy
    float bigramEntropy = 0.0f;         // Shannon entropy of transition bigrams
    float predictionConfidence = 0.0f;  // Markov bigram structural predictability
    uint64_t orbitHash = 0xcbf29ce484222325ULL; // SGH5 / FNV-1a geometric orbit hash
    MemoryRegionCategory category = MemoryRegionCategory::ZeroPool;
    std::string categoryName;
};

// Fast inline 8-token Level-1 Haar DWT transition extractor
// Takes two consecutive bytes and generates the 8 transition tokens for the 4 bit-pairs
inline void ExtractWaveletTokens(uint8_t a, uint8_t b, uint8_t outTokens[8]) {
    for (int i = 0; i < 4; ++i) {
        int ec = (a >> (7 - 2 * i)) & 1;
        int oc = (a >> (6 - 2 * i)) & 1;
        int en = (b >> (7 - 2 * i)) & 1;
        int on = (b >> (6 - 2 * i)) & 1;
        // cA transition (sum) and cD transition (diff)
        outTokens[i]     = static_cast<uint8_t>((ec + oc) - (en + on) + 2);
        outTokens[4 + i] = static_cast<uint8_t>((ec - oc) - (en - on) + 2);
    }
}

// Compute comprehensive Wavelet Entropy & Geometry Metrics across a memory buffer
inline WaveletEntropyMetrics AnalyzeWaveletEntropy(const uint8_t* data, size_t size, size_t maxSample = 65536) {
    WaveletEntropyMetrics metrics;
    metrics.totalBytesAnalyzed = size;

    if (data == nullptr || size < 2) {
        metrics.category = MemoryRegionCategory::ZeroPool;
        metrics.categoryName = "Zero / Unallocated Pool";
        return metrics;
    }

    size_t sampleLen = (size < maxSample) ? size : maxSample;
    uint32_t tokenCounts[WAVELET_NUM_STATES] = { 0 };
    uint64_t totalTokens = 0;
    float energySum = 0.0f;
    float energySqSum = 0.0f;
    uint64_t orbit = 0xcbf29ce484222325ULL;

    uint32_t bigram[WAVELET_NUM_STATES][WAVELET_NUM_STATES] = { { 0 } };
    uint8_t prevTokens[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };

    // Bigram predictor confidence tracking
    float confidenceSum = 0.0f;
    uint32_t confidenceSamples = 0;

    for (size_t i = 0; i + 1 < sampleLen; ++i) {
        uint8_t tokens[8];
        ExtractWaveletTokens(data[i], data[i + 1], tokens);

        for (int j = 0; j < 8; ++j) {
            uint8_t tok = tokens[j];
            if (tok >= WAVELET_NUM_STATES) tok = 2; // clamp bounds

            tokenCounts[tok]++;
            totalTokens++;

            float e = std::fabs(static_cast<float>(tok) - 2.0f);
            energySum += e;
            energySqSum += e * e;

            // SGH5 geometric orbit hash accumulation
            orbit ^= static_cast<uint64_t>(tok);
            orbit *= 0x100000001b3ULL;

            if (i > 0) {
                bigram[prevTokens[j]][tok]++;
            }
            prevTokens[j] = tok;
        }

        // Predictor feed
        if (i >= 16) {
            float frameConf = 0.0f;
            for (int j = 0; j < 8; ++j) {
                uint32_t rowTotal = 0;
                uint32_t best = 0;
                for (int s = 0; s < WAVELET_NUM_STATES; ++s) {
                    uint32_t c = bigram[prevTokens[j]][s];
                    rowTotal += c;
                    if (c > best) best = c;
                }
                if (rowTotal > 0) {
                    frameConf += static_cast<float>(best) / static_cast<float>(rowTotal);
                }
            }
            confidenceSum += (frameConf / 8.0f);
            confidenceSamples++;
        }
    }

    if (totalTokens > 0) {
        metrics.identityDensity = static_cast<float>(tokenCounts[2]) / static_cast<float>(totalTokens);
        metrics.transitionEnergy = energySum / static_cast<float>(totalTokens);
        float meanEnergy = metrics.transitionEnergy;
        metrics.energyVariance = (energySqSum / static_cast<float>(totalTokens)) - (meanEnergy * meanEnergy);
    }
    metrics.orbitHash = orbit;

    if (confidenceSamples > 0) {
        metrics.predictionConfidence = confidenceSum / static_cast<float>(confidenceSamples);
    }

    // Shannon Bigram Entropy
    float entropy = 0.0f;
    uint64_t bgTotal = 0;
    for (int r = 0; r < WAVELET_NUM_STATES; ++r) {
        for (int c = 0; c < WAVELET_NUM_STATES; ++c) {
            bgTotal += bigram[r][c];
        }
    }
    if (bgTotal > 0) {
        for (int r = 0; r < WAVELET_NUM_STATES; ++r) {
            for (int c = 0; c < WAVELET_NUM_STATES; ++c) {
                if (bigram[r][c] > 0) {
                    float p = static_cast<float>(bigram[r][c]) / static_cast<float>(bgTotal);
                    entropy -= p * std::log2(p);
                }
            }
        }
    }
    metrics.bigramEntropy = entropy;

    // Classification Decision Tree
    if (metrics.identityDensity >= 0.98f) {
        metrics.category = MemoryRegionCategory::ZeroPool;
        metrics.categoryName = "Zero / Unallocated Pool";
    } else if (metrics.transitionEnergy >= 0.72f && metrics.identityDensity <= 0.40f) {
        metrics.category = MemoryRegionCategory::HighEntropyPayload;
        metrics.categoryName = "High-Entropy / Encrypted / Shellcode";
    } else if (metrics.predictionConfidence > 0.60f || metrics.identityDensity >= 0.50f) {
        metrics.category = MemoryRegionCategory::StructuralExecutable;
        metrics.categoryName = "Structural Code / Page Tables / Kernel Headers";
    } else {
        metrics.category = MemoryRegionCategory::ConstrainedData;
        metrics.categoryName = "Constrained Structured Data / Strings";
    }

    return metrics;
}

} // namespace phylaram
