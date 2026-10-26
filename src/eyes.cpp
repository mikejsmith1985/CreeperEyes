// Maps the 'eyes' flash partition (eyedata/eyes.bin) and builds kEyeStyles from its header.
#include "eyestyle.h"
#include <esp_partition.h>
#if __has_include(<spi_flash_mmap.h>)
#include <spi_flash_mmap.h>        // ESP-IDF 5
#else
#include <esp_spi_flash.h>         // ESP-IDF 4 (Arduino core 2.x)
#endif

EyeStyle kEyeStyles[MAX_EYE_STYLES];
uint8_t  kEyeStyleCount = 0;
static char names[MAX_EYE_STYLES][24];

struct __attribute__((packed)) BlobEntry {
  char     name[24];
  uint32_t sclera; uint16_t sW, sH;
  uint32_t iris;   uint16_t iW, iH;
  uint32_t upper, lower, polar; uint16_t pW, pH;
  int16_t  irisMin, irisMax;
  uint8_t  pad[4];
};
static_assert(sizeof(BlobEntry) == 64, "entry size");

bool eyesLoad() {
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "eyes");
  if (!p) { Serial.println("[eyes] no 'eyes' partition -- flash with the installer or pio upload"); return false; }
  const void *map; spi_flash_mmap_handle_t h;
  if (esp_partition_mmap(p, 0, p->size, SPI_FLASH_MMAP_DATA, &map, &h) != ESP_OK) {
    Serial.println("[eyes] mmap failed"); return false;
  }
  const uint8_t *b = (const uint8_t *)map;
  if (memcmp(b, "CEYE", 4) != 0) { Serial.println("[eyes] eye graphics not flashed (partition blank)"); return false; }
  uint16_t count = b[6] | (b[7] << 8);
  if (count > MAX_EYE_STYLES) count = MAX_EYE_STYLES;
  for (uint16_t i = 0; i < count; i++) {
    const BlobEntry *e = (const BlobEntry *)(b + 8 + i * 64);
    memcpy(names[i], e->name, 23); names[i][23] = 0;
    EyeStyle &s = kEyeStyles[i];
    s.name = names[i];
    s.sclera = (const uint16_t *)(b + e->sclera); s.scleraW = e->sW; s.scleraH = e->sH;
    s.iris   = (const uint16_t *)(b + e->iris);   s.irisMapW = e->iW; s.irisMapH = e->iH;
    s.upper  = b + e->upper; s.lower = b + e->lower;
    s.polar  = (const uint16_t *)(b + e->polar);  s.irisW = e->pW; s.irisH = e->pH;
    s.irisMin = e->irisMin; s.irisMax = e->irisMax;
  }
  kEyeStyleCount = count;
  Serial.printf("[eyes] %u eye styles loaded from flash\n", count);
  return true;
}

int eyeStyleFind(const char *name) {
  for (int i = 0; i < kEyeStyleCount; i++) if (!strcmp(kEyeStyles[i].name, name)) return i;
  return -1;
}
