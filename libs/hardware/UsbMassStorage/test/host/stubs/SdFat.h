#pragma once
#include <cstddef>
#include <cstdint>
typedef uint32_t Sector_t;
class FsBlockDeviceInterface {
 public:
  virtual ~FsBlockDeviceInterface() = default;
  virtual Sector_t sectorCount() = 0;
  virtual bool readSector(Sector_t, uint8_t*) { return true; }
  virtual bool readSectors(Sector_t, uint8_t*, size_t) { return true; }
  virtual bool writeSector(Sector_t, const uint8_t*) { return true; }
  virtual bool writeSectors(Sector_t, const uint8_t*, size_t) { return true; }
  virtual bool syncDevice() { return true; }
};
