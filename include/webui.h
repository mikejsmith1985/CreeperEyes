#pragma once
// Starts the CreeperEyes-XXXX Wi-Fi network and the settings page at http://192.168.4.1/
void webuiStart();
extern volatile bool gOtaActive;   // true while a wireless firmware update is being written
