#include "persistent_store.h"
#include <string.h>

namespace {
constexpr uint32_t MAGIC = 0x53434c4b;
constexpr uint16_t VERSION = 2;

struct PersistedDataV1 {
  uint32_t nextId;
  Schedule::Alarm alarms[Config::MAX_ALARMS];
  Schedule::Timer timers[Config::MAX_TIMERS];
};

template <typename Data>
struct StoredBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  Data data;
  uint32_t crc;
};
using Blob = StoredBlob<PersistedData>;
using BlobV1 = StoredBlob<PersistedDataV1>;

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320UL & (0U - (crc & 1U)));
  }
  return ~crc;
}

void setError(char* destination, const char* message) {
  strncpy(destination, message, 63);
  destination[63] = '\0';
}
}

bool PersistentStore::begin() {
  if (!preferences_.begin("smart-clock", false)) {
    setError(lastError_, "Preferences begin failed");
    return false;
  }
  return true;
}

bool PersistentStore::load(PersistedData& data) {
  const size_t storedSize = preferences_.getBytesLength("state");
  if (storedSize == 0) {
    data = PersistedData{};
    lastError_[0] = '\0';
    return true;
  }

  constexpr size_t MAX_BLOB_SIZE =
      sizeof(Blob) > sizeof(BlobV1) ? sizeof(Blob) : sizeof(BlobV1);
  if (storedSize > MAX_BLOB_SIZE) {
    setError(lastError_, "stored state size mismatch");
    data = PersistedData{};
    return false;
  }
  uint8_t raw[MAX_BLOB_SIZE]{};
  if (preferences_.getBytes("state", raw, storedSize) != storedSize) {
    setError(lastError_, "stored state read failed");
    data = PersistedData{};
    return false;
  }
  uint16_t storedVersion = 0;
  memcpy(&storedVersion, raw + offsetof(Blob, version), sizeof(storedVersion));

  if (storedVersion == VERSION && storedSize == sizeof(Blob)) {
    Blob blob{};
    memcpy(&blob, raw, sizeof(blob));
    const uint32_t expected =
        crc32(reinterpret_cast<const uint8_t*>(&blob), offsetof(Blob, crc));
    if (blob.magic != MAGIC || blob.version != VERSION ||
        blob.size != sizeof(PersistedData) || blob.crc != expected) {
      setError(lastError_, "stored state validation failed");
      data = PersistedData{};
      return false;
    }
    data = blob.data;
  } else if (storedVersion == 1 && storedSize == sizeof(BlobV1)) {
    BlobV1 blob{};
    memcpy(&blob, raw, sizeof(blob));
    const uint32_t expected =
        crc32(reinterpret_cast<const uint8_t*>(&blob), offsetof(BlobV1, crc));
    if (blob.magic != MAGIC || blob.version != 1 ||
        blob.size != sizeof(PersistedDataV1) || blob.crc != expected) {
      setError(lastError_, "stored v1 state validation failed");
      data = PersistedData{};
      return false;
    }
    data = PersistedData{};
    data.nextId = blob.data.nextId;
    memcpy(data.alarms, blob.data.alarms, sizeof(data.alarms));
    memcpy(data.timers, blob.data.timers, sizeof(data.timers));
    data.sharpAlarmGateEnabled = false;
    if (!save(data)) return false;
  } else {
    setError(lastError_, "stored state size mismatch");
    data = PersistedData{};
    return false;
  }

  if (data.nextId == 0) data.nextId = 1;
  lastError_[0] = '\0';
  return true;
}

bool PersistentStore::save(const PersistedData& data) {
  Blob blob{};
  blob.magic = MAGIC;
  blob.version = VERSION;
  blob.size = sizeof(PersistedData);
  blob.data = data;
  blob.crc = crc32(reinterpret_cast<const uint8_t*>(&blob), offsetof(Blob, crc));
  if (preferences_.putBytes("state", &blob, sizeof(blob)) != sizeof(blob)) {
    setError(lastError_, "stored state write failed");
    return false;
  }
  lastError_[0] = '\0';
  return true;
}
