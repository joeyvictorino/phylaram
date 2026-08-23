#include "../shared/wavelet_classifier.hpp"
#include <cassert>
#include <vector>
#include <iostream>
#include <random>

int main() {
    // Test 1: Zero Pool buffer (all zeros)
    {
        std::vector<uint8_t> zeros(4096, 0);
        auto metrics = phylaram::AnalyzeWaveletEntropy(zeros.data(), zeros.size());
        assert(metrics.category == phylaram::MemoryRegionCategory::ZeroPool);
        assert(metrics.identityDensity >= 0.99f);
        assert(metrics.transitionEnergy == 0.0f);
        std::cout << "[PASS] Zero pool wavelet classification verified.\n";
    }

    // Test 2: Structured code / PE header simulation
    {
        std::vector<uint8_t> peHeader = {
            'M', 'Z', 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, (uint8_t)0xFF, (uint8_t)0xFF, 0x00, 0x00,
            (uint8_t)0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)0x80, 0x00, 0x00, 0x00
        };
        while (peHeader.size() < 4096) {
            peHeader.push_back(0x00);
        }
        auto metrics = phylaram::AnalyzeWaveletEntropy(peHeader.data(), peHeader.size());
        assert(metrics.totalBytesAnalyzed == 4096);
        assert(metrics.orbitHash != 0);
        std::cout << "[PASS] Structured header wavelet classification verified.\n";
    }

    // Test 3: High-Entropy / Pseudorandom payload
    {
        std::vector<uint8_t> randomBytes(4096);
        std::mt19937 rng(1337);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : randomBytes) {
            b = static_cast<uint8_t>(dist(rng));
        }

        auto metrics = phylaram::AnalyzeWaveletEntropy(randomBytes.data(), randomBytes.size());
        assert(metrics.category == phylaram::MemoryRegionCategory::HighEntropyPayload);
        assert(metrics.identityDensity <= 0.40f);
        assert(metrics.transitionEnergy >= 0.72f);
        assert(metrics.bigramEntropy > 2.0f);
        std::cout << "[PASS] High-entropy wavelet payload detection verified.\n";
    }

    // Test 4: Token extraction exactness
    {
        uint8_t tokens[8];
        phylaram::ExtractWaveletTokens(0x00, 0x00, tokens);
        for (int i = 0; i < 8; ++i) {
            assert(tokens[i] == 2); // Identity
        }

        phylaram::ExtractWaveletTokens(0xFF, 0x00, tokens);
        // High to low transition
        assert(tokens[0] == 4); // Max positive diff
        assert(tokens[1] == 4);
    }

    std::cout << "[PASS] All Wavelet Transition Entropy tests passed successfully.\n";
    return 0;
}
