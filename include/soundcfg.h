// soundcfg.h -- user-editable sound buttons and which tracks the monster uses for each
// behaviour ("roles"). Saved in flash, edited from the phone page (/api/sounds).
#pragma once
#include <Arduino.h>

enum SoundRole : uint8_t { R_WAKE, R_GROWL, R_LEAN, R_ROAR, R_SNORE, R_HMM, R_CHUCKLE, R_THUNDER, R_COUNT };
#define SND_MAX_BUTTONS 16
#define SND_MAX_PER_ROLE 8

struct SoundButton { char name[20]; uint16_t track; };
struct SoundCfg {
  uint8_t     nButtons;
  SoundButton buttons[SND_MAX_BUTTONS];
  uint8_t     nRole[R_COUNT];
  uint16_t    role[R_COUNT][SND_MAX_PER_ROLE];
};

void     soundCfgLoad();
uint16_t soundForRole(SoundRole r);        // random track from that role's list, 0 = stay silent
bool     soundRoleIsDefault(SoundRole r);  // still the factory list (then missing files fall back)
String   soundCfgJson(uint16_t filesOnCard);
bool     soundCfgFromJson(const String &json, String &err);   // validates, applies and saves
void     soundCfgReset();
