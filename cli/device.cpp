#include "phylaram.hpp"
#include <algorithm>

DeviceSession::DeviceSession()
{
    ioBuffer_.resize(sizeof(PHYLA_READ_RESULT) + PHYLA_MAX_TRANSFER);
}

DeviceSession::~DeviceSession()
{
    Close();
}

bool DeviceSession::Open()
{
    handle_.Reset(CreateFileW(PHYLA_USER_DEVICE_NAME,
                              GENERIC_READ,
                              0,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr));
    if (!handle_) {
        lastError_ = GetLastError();
        return false;
    }
    return true;
}

void DeviceSession::Close()
{
    handle_.Reset();
}

bool DeviceSession::Query(uint64_t& highestEnd, uint64_t& totalBytes, std::vector<MemoryRun>& runs)
{
    highestEnd = 0;
    totalBytes = 0;
    runs.clear();

    if (!handle_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    PHYLA_QUERY_INFO info{};
    DWORD returned = 0;

    if (!DeviceIoControl(handle_.Get(), IOCTL_PHYLA_QUERY_INFO, nullptr, 0,
                         &info, sizeof(info), &returned, nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    if (returned != sizeof(info) || info.Version != PHYLA_PROTOCOL_VERSION) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    if (info.RunCount == 0 || info.RunCount > 1024 * 1024) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    std::vector<PHYLA_MEMORY_RUN> raw(info.RunCount);
    DWORD expectedBytes = static_cast<DWORD>(raw.size() * sizeof(raw[0]));

    if (!DeviceIoControl(handle_.Get(), IOCTL_PHYLA_GET_RANGES, nullptr, 0,
                         raw.data(), expectedBytes, &returned, nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    if (returned != expectedBytes) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    runs.reserve(raw.size());
    for (uint32_t i = 0; i < raw.size(); ++i) {
        if (raw[i].NumberOfBytes == 0 ||
            raw[i].BaseAddress > UINT64_MAX - raw[i].NumberOfBytes) {
            lastError_ = ERROR_INVALID_DATA;
            return false;
        }
        runs.push_back({i, raw[i].BaseAddress, raw[i].NumberOfBytes});
    }

    std::sort(runs.begin(), runs.end(), [](const MemoryRun& a, const MemoryRun& b) {
        return a.base < b.base;
    });

    for (size_t i = 1; i < runs.size(); ++i) {
        uint64_t prevEnd = runs[i - 1].base + runs[i - 1].length;
        if (prevEnd > runs[i].base) {
            lastError_ = ERROR_INVALID_DATA;
            return false;
        }
    }

    highestEnd = info.HighestPhysicalEnd;
    totalBytes = info.TotalBytes;
    return true;
}

bool DeviceSession::QueryHints(KernelHints& hints)
{
    hints = KernelHints{};

    if (!handle_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    PHYLA_KERNEL_HINTS raw{};
    DWORD returned = 0;

    if (!DeviceIoControl(handle_.Get(), IOCTL_PHYLA_QUERY_HINTS, nullptr, 0,
                         &raw, sizeof(raw), &returned, nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    if (returned != sizeof(raw) || raw.Version != PHYLA_PROTOCOL_VERSION) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    hints.available = true;
    hints.hypervisorPresent = (raw.HypervisorPresent != 0);
    hints.majorVersion = raw.MajorVersion;
    hints.minorVersion = raw.MinorVersion;
    hints.buildNumber = raw.BuildNumber;
    hints.numberOfProcessors = raw.NumberOfProcessors;
    hints.directoryTableBase = raw.DirectoryTableBase;
    hints.kpcrAddress = raw.KpcrAddress;
    hints.kernelBase = raw.KernelBase;
    hints.kernelSize = raw.KernelSize;

    return true;
}

bool DeviceSession::Read(uint32_t runIndex, uint64_t offset, uint32_t length, ReadResult& result)
{
    if (!handle_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    if (length == 0 || length > PHYLA_MAX_TRANSFER) {
        lastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    PHYLA_READ_REQUEST request{};
    request.RunIndex = runIndex;
    request.Length = length;
    request.OffsetWithinRun = offset;

    size_t neededCapacity = sizeof(PHYLA_READ_RESULT) + length;
    if (ioBuffer_.size() < neededCapacity) {
        ioBuffer_.resize(neededCapacity);
    }

    DWORD returned = 0;
    if (!DeviceIoControl(handle_.Get(), IOCTL_PHYLA_READ_RUN,
                         &request, sizeof(request),
                         ioBuffer_.data(), static_cast<DWORD>(neededCapacity),
                         &returned, nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    if (returned < sizeof(PHYLA_READ_RESULT)) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    auto* header = reinterpret_cast<const PHYLA_READ_RESULT*>(ioBuffer_.data());
    if (header->Version != PHYLA_PROTOCOL_VERSION ||
        header->HeaderSize != sizeof(PHYLA_READ_RESULT) ||
        header->BytesCopied > length ||
        returned != sizeof(PHYLA_READ_RESULT) + header->BytesCopied) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    result.physicalAddress = header->PhysicalAddress;
    result.requested = header->RequestedLength;
    result.copied = header->BytesCopied;
    result.copyStatus = header->CopyStatus;
    result.data.assign(ioBuffer_.begin() + sizeof(PHYLA_READ_RESULT),
                       ioBuffer_.begin() + sizeof(PHYLA_READ_RESULT) + header->BytesCopied);

    return true;
}

bool DeviceSession::End(bool& topologyChanged)
{
    topologyChanged = false;

    if (!handle_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    PHYLA_END_RESULT endResult{};
    DWORD returned = 0;

    if (!DeviceIoControl(handle_.Get(), IOCTL_PHYLA_END_SESSION, nullptr, 0,
                         &endResult, sizeof(endResult), &returned, nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    if (returned != sizeof(endResult) || endResult.Version != PHYLA_PROTOCOL_VERSION) {
        lastError_ = ERROR_INVALID_DATA;
        return false;
    }

    topologyChanged = (endResult.TopologyChanged != 0);
    return true;
}
