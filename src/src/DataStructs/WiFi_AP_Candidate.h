#ifndef DATASTRUCTS_WIFI_AP_CANDIDATES_H
#define DATASTRUCTS_WIFI_AP_CANDIDATES_H

#include "../../ESPEasy_common.h"

#include "../DataStructs/MAC_address.h"
struct WiFi_AP_Candidate {
  // Construct from stored credentials
  // @param index  The index of the stored credentials
  // @param ssid_c SSID of the credentials
  // @param pass   Password/key of the credentials
  WiFi_AP_Candidate(uint8_t          index,
                    const String& ssid_c);


  // Construct using index from WiFi scan result
  WiFi_AP_Candidate(uint8_t networkItem);
  #ifdef ESP8266
  #if FEATURE_ESP8266_DIRECT_WIFI_SCAN
  WiFi_AP_Candidate(const bss_info& ap);
  #endif
  #endif


  WiFi_AP_Candidate() = default;
  WiFi_AP_Candidate(const WiFi_AP_Candidate& other) = default;


  // Return true when this one is preferred over 'other'.
  bool               operator<(const WiFi_AP_Candidate& other) const;

  bool               operator==(const WiFi_AP_Candidate& other) const  {
    return bssid_match(other.bssid) && ssid.equals(other.ssid);// && key.equals(other.key);
  }

  WiFi_AP_Candidate& operator=(const WiFi_AP_Candidate& other) = default;

  // Check if the candidate data can be used to actually connect to an AP.
  bool               usable() const;

  // Check if the candidate was recently seen
  bool               expired() const;

  // For quick connection the channel and BSSID are needed
  bool               allowQuickConnect() const { return (channel != 0) && bssid_set(); }

  // Check to see if the BSSID is set
  bool               bssid_set() const  { return !bssid.all_zero(); }

  bool               bssid_match(const uint8_t bssid_c[6]) const { return bssid == bssid_c; }
  bool               bssid_match(const MAC_address& other) const {  return bssid == other; }

  // Create a formatted string
  String             toString(const String& separator = " ") const;

  String             encryption_type() const;

  bool               phy_known() const { return phy_11b || phy_11g || phy_11n; }

  String  ssid;
//  String  key;

  unsigned long last_seen = 0;
  int32_t       rssi     = 0;
  int32_t       channel  = 0;
  MAC_address   bssid;
  uint8_t       index    = 0;     // Index of the matching credentials
  uint8_t       enc_type = 0;     // Encryption used (e.g. WPA2)
  union 
  {
    struct {
      uint8_t isHidden:1; // Hidden SSID
      uint8_t lowPriority:1; // Try as last attempt
      uint8_t isEmergencyFallback:1; 
      uint8_t phy_11b:1; 
      uint8_t phy_11g:1; 
      uint8_t phy_11n:1; 
      uint8_t wps:1; 
      uint8_t unused:1;      
    };
    uint8_t flags = 0;
  };
  
};

#endif // ifndef DATASTRUCTS_WIFI_AP_CANDIDATES_H
