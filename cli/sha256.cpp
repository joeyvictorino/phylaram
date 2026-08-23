#include "phylaram.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace {
constexpr size_t kHashChunkBytes = 16u * 1024u * 1024u;
}

Sha256::Sha256(Sha256&& other) noexcept
    : algorithm_(other.algorithm_),
      hash_(other.hash_),
      initialized_(other.initialized_)
{
    other.algorithm_ = nullptr;
    other.hash_ = nullptr;
    other.initialized_ = false;
}

Sha256& Sha256::operator=(Sha256&& other) noexcept
{
    if (this != &other) {
        Reset();
        algorithm_ = other.algorithm_;
        hash_ = other.hash_;
        initialized_ = other.initialized_;

        other.algorithm_ = nullptr;
        other.hash_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void Sha256::Reset()
{
    if (hash_ != nullptr) {
        BCryptDestroyHash(hash_);
        hash_ = nullptr;
    }
    if (algorithm_ != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm_, 0);
        algorithm_ = nullptr;
    }
    initialized_ = false;
}

bool Sha256::Initialize()
{
    Reset();

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0) < 0) {
        return false;
    }

    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    algorithm_ = algorithm;
    hash_ = hash;
    initialized_ = true;
    return true;
}

bool Sha256::Update(const uint8_t* data, size_t length)
{
    if (!initialized_ || (data == nullptr && length != 0)) {
        return false;
    }

    while (length != 0) {
        const size_t chunkSize = std::min(length, kHashChunkBytes);
        const ULONG chunkBytes = static_cast<ULONG>(chunkSize);

        /*
         * SAFETY: BCryptHashData's legacy signature accepts PUCHAR even though
         * it does not retain or mutate the caller's bytes.  data remains valid
         * for chunkBytes for the duration of this synchronous call.
         */
        if (BCryptHashData(
                hash_,
                const_cast<PUCHAR>(data),
                chunkBytes,
                0) < 0) {
            return false;
        }

        data += chunkSize;
        length -= chunkSize;
    }

    return true;
}

bool Sha256::UpdateZeros(uint64_t length)
{
    if (!initialized_) {
        return false;
    }

    static const std::array<uint8_t, 1024 * 1024> zeros{};
    while (length != 0) {
        const size_t chunkSize = static_cast<size_t>(
            std::min<uint64_t>(length, zeros.size()));
        if (!Update(zeros.data(), chunkSize)) {
            return false;
        }
        length -= chunkSize;
    }

    return true;
}

bool Sha256::Finish(std::string& hex)
{
    hex.clear();
    if (!initialized_) {
        return false;
    }

    std::array<uint8_t, 32> digest{};
    if (BCryptFinishHash(
            hash_,
            digest.data(),
            static_cast<ULONG>(digest.size()),
            0) < 0) {
        return false;
    }

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const uint8_t byte : digest) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    hex = stream.str();

    Reset();
    return true;
}
