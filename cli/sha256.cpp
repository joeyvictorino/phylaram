#include "phylaram.hpp"
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")

Sha256::Sha256(Sha256&& other) noexcept
    : alg_(other.alg_), hash_(other.hash_), initialized_(other.initialized_)
{
    other.alg_ = nullptr;
    other.hash_ = nullptr;
    other.initialized_ = false;
}

Sha256& Sha256::operator=(Sha256&& other) noexcept
{
    if (this != &other) {
        Reset();
        alg_ = other.alg_;
        hash_ = other.hash_;
        initialized_ = other.initialized_;
        other.alg_ = nullptr;
        other.hash_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void Sha256::Reset()
{
    if (hash_) {
        BCryptDestroyHash(hash_);
        hash_ = nullptr;
    }
    if (alg_) {
        BCryptCloseAlgorithmProvider(alg_, 0);
        alg_ = nullptr;
    }
    initialized_ = false;
}

bool Sha256::Initialize()
{
    Reset();

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return false;
    }

    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    alg_ = alg;
    hash_ = hash;
    initialized_ = true;
    return true;
}

bool Sha256::Update(const uint8_t* data, size_t length)
{
    if (!initialized_ || (!data && length != 0)) {
        return false;
    }

    while (length != 0) {
        ULONG chunk = static_cast<ULONG>(std::min<size_t>(length, 16u * 1024u * 1024u));
        if (BCryptHashData(hash_, const_cast<PUCHAR>(data), chunk, 0) < 0) {
            return false;
        }
        data += chunk;
        length -= chunk;
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
        size_t chunk = static_cast<size_t>(std::min<uint64_t>(length, zeros.size()));
        if (!Update(zeros.data(), chunk)) {
            return false;
        }
        length -= chunk;
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
    if (BCryptFinishHash(hash_, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        return false;
    }

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : digest) {
        ss << std::setw(2) << static_cast<unsigned int>(b);
    }
    hex = ss.str();

    Reset();
    return true;
}
