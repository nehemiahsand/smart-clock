#include "persistent_store.h"
#include <string.h>

namespace {
constexpr uint32_t MAGIC = 0x53434c4b;
constexpr uint16_t VERSION = 1;
struct Blob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  PersistedData data;
  uint32_t crc;
};

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
  if (preferences_.getBytesLength("state") == 0) {
    data = PersistedData{};
    lastError_[0] = '\0';
    return true;
  }
  if (preferences_.getBytesLength("state") != sizeof(Blob)) {
    setError(lastError_, "stored state size mismatch");
    data = PersistedData{};
    return false;
  }
  Blob blob{};
  if (preferences_.getBytes("state", &blob, sizeof(blob)) != sizeof(blob)) {
    setError(lastError_, "stored state read failed");
    data = PersistedData{};
    return false;
  }
  const uint32_t expected =
      crc32(reinterpret_cast<const uint8_t*>(&blob), offsetof(Blob, crc));
  if (blob.magic != MAGIC || blob.version != VERSION ||
      blob.size != sizeof(PersistedData) || blob.crc != expected) {
    setError(lastError_, "stored state validation failed");
    data = PersistedData{};
    return false;
  }
  data = blob.data;
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
