#include "../shared/interfaces.hpp"
#include <cassert>
#include <algorithm>
#include <limits>
#include <iostream>

struct RangeValidator {
    static bool ValidateAndSort(std::vector<MemoryRun>& runs, uint64_t& totalBytes, uint64_t& highestEnd) {
        if (runs.empty()) {
            return false;
        }
        totalBytes = 0;
        highestEnd = 0;

        for (const auto& r : runs) {
            if (r.length == 0) {
                return false;
            }
            if (r.base > std::numeric_limits<uint64_t>::max() - r.length) {
                return false;
            }
            if (totalBytes > std::numeric_limits<uint64_t>::max() - r.length) {
                return false;
            }
            totalBytes += r.length;
            uint64_t end = r.base + r.length;
            if (end > highestEnd) {
                highestEnd = end;
            }
        }

        std::sort(runs.begin(), runs.end(), [](const MemoryRun& a, const MemoryRun& b) {
            return a.base < b.base;
        });

        for (size_t i = 1; i < runs.size(); ++i) {
            uint64_t prevEnd = runs[i - 1].base + runs[i - 1].length;
            if (prevEnd > runs[i].base) {
                return false;
            }
        }

        return true;
    }
};

int main() {
    uint64_t total = 0;
    uint64_t highest = 0;

    // 1. Standard valid disjoint ranges
    std::vector<MemoryRun> v1 = {{0, 0x1000, 0x9000}, {1, 0x100000, 0x200000}};
    assert(RangeValidator::ValidateAndSort(v1, total, highest));
    assert(total == 0x9000 + 0x200000);
    assert(highest == 0x100000 + 0x200000);

    // 2. Out-of-order sorting
    std::vector<MemoryRun> v2 = {{1, 0x100000, 0x1000}, {0, 0x1000, 0x1000}};
    assert(RangeValidator::ValidateAndSort(v2, total, highest));
    assert(v2[0].base == 0x1000 && v2[1].base == 0x100000);

    // 3. Reject zero-length range
    std::vector<MemoryRun> v3 = {{0, 0x1000, 0}};
    assert(!RangeValidator::ValidateAndSort(v3, total, highest));

    // 4. Reject overlapping ranges
    std::vector<MemoryRun> v4 = {{0, 0x1000, 0x5000}, {1, 0x4000, 0x2000}};
    assert(!RangeValidator::ValidateAndSort(v4, total, highest));

    // 5. Reject range containment (Run B inside Run A)
    std::vector<MemoryRun> v5 = {{0, 0x1000, 0x10000}, {1, 0x2000, 0x1000}};
    assert(!RangeValidator::ValidateAndSort(v5, total, highest));

    // 6. Reject UINT64 overflow on base + length
    std::vector<MemoryRun> v6 = {{0, std::numeric_limits<uint64_t>::max() - 0x100, 0x200}};
    assert(!RangeValidator::ValidateAndSort(v6, total, highest));

    // 7. Reject totalBytes accumulator overflow
    std::vector<MemoryRun> v7 = {
        {0, 0, 0x8000000000000000ULL},
        {1, 0x9000000000000000ULL, 0x8000000000000000ULL}
    };
    assert(!RangeValidator::ValidateAndSort(v7, total, highest));

    // 8. Reject empty vector
    std::vector<MemoryRun> v8 = {};
    assert(!RangeValidator::ValidateAndSort(v8, total, highest));

    // 9. Scalability: >64 runs test (128 disjoint runs)
    std::vector<MemoryRun> v9;
    for (uint32_t i = 0; i < 128; ++i) {
        v9.push_back({i, static_cast<uint64_t>(i) * 0x10000000ULL, 0x1000000ULL});
    }
    assert(RangeValidator::ValidateAndSort(v9, total, highest));
    assert(v9.size() == 128);

    // 10. Large memory scales: 4 GiB+, 64 GiB+, 128 GiB+, 256 GiB+
    uint64_t g256 = 256ULL * 1024ULL * 1024ULL * 1024ULL; // 256 GiB
    std::vector<MemoryRun> v10 = {
        {0, 0x1000, 0x9F000},
        {1, 0x100000, g256}
    };
    assert(RangeValidator::ValidateAndSort(v10, total, highest));
    assert(highest == 0x100000 + g256);

    std::cout << "[PASS] Range algebra and 64-bit integer boundary tests passed successfully.\n";
    return 0;
}
