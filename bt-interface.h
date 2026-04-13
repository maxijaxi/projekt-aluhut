#pragma once
#include <Arduino.h>

static const size_t BT_MAX_PAYLOAD_SIZE = 256;

enum class BtIoStatus : uint8_t
{
    Ok = 0,
    InvalidArgument,
    Timeout,
    NotAvailable,
    BufferTooSmall,
    UnknownError
};

class IBtIo
{
public:
    virtual ~IBtIo() = default;

    virtual BtIoStatus send(const uint8_t *data,
                            size_t length,
                            size_t *sentBytes = nullptr) = 0;

    virtual BtIoStatus receive(uint8_t *outBuffer,
                               size_t maxLength,
                               size_t *receivedBytes,
                               uint32_t timeoutMs = 0) = 0;

    virtual size_t available() const = 0;
};
