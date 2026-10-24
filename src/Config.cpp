#include "Config.h"
#include "Defaults.h"
#include <Preferences.h>

ConfigManager cfg;

static Preferences prefs;

void ConfigManager::begin() {
  prefs.begin("rxvoter", false);
  size_t len = prefs.getBytesLength("cfg");
  memset(&data, 0, sizeof(data));
  if (len > 0 && len <= sizeof(data)) {
    prefs.getBytes("cfg", &data, len);
  }
  if (len >= offsetof(SysConfig, rxPgaGain) && len <= sizeof(data) &&
      data.magic == CONFIG_MAGIC && data.version >= 2 && data.version < CONFIG_VERSION) {
    // Older layouts (ESP32RXVoter v2/v3) are a prefix of this one
    uint32_t oldVersion = data.version;
    if (oldVersion < 3) {
      // Frames are now stamped with exact GPS time and the client/master
      // offsets are built in, so the stored value is only a trim
      data.timingOffsetMs = DEF_TIMING_OFFSET_MS;
      data.rxPgaGain = DEF_RX_PGA_GAIN;
      data.netIface = DEF_NET_IFACE;
      data.dspRmsNoSig = 0.0f; // DSP RSSI not calibrated
      data.dspRmsFull = 0.0f;
      data.dspSqlOpen = DEF_DSP_SQL_OPEN;
      data.dspSqlHyst = DEF_DSP_SQL_HYST;
    }
    if (oldVersion < 4) {
      data.txGainPct = DEF_TX_GAIN_PCT;
      data.pttInvert = DEF_PTT_INVERT;
      data.pttTailMs = DEF_PTT_TAIL_MS;
      data.jitterMs = DEF_JITTER_MS;
    }
    save();
    Serial.printf("[Config] Migrated v%lu config from NVS\r\n", (unsigned long)oldVersion);
  } else if (len != sizeof(data) || data.magic != CONFIG_MAGIC ||
             data.version != CONFIG_VERSION) {
    Serial.println("[Config] No valid config in NVS - loading defaults");
    resetDefaults();
  } else {
    Serial.println("[Config] Loaded from NVS");
  }

  if (prefs.getBytesLength("gpstm") == sizeof(gpsTm)) {
    prefs.getBytes("gpstm", &gpsTm, sizeof(gpsTm));
  } else {
    _gpsTimingDefaults();
  }
}

void ConfigManager::saveGpsTiming() {
  prefs.putBytes("gpstm", &gpsTm, sizeof(gpsTm));
  Serial.println("[Config] GPS timing saved to NVS");
}

void ConfigManager::_gpsTimingDefaults() {
  memset(&gpsTm, 0, sizeof(gpsTm));
  gpsTm.mode = DEF_GPS_TMODE;
  gpsTm.svinMinDurS = DEF_SVIN_MIN_DUR_S;
  gpsTm.svinAccMm = DEF_SVIN_ACC_MM;
  gpsTm.fixedAccMm = DEF_SVIN_ACC_MM;
}

void ConfigManager::save() {
  data.magic = CONFIG_MAGIC;
  data.version = CONFIG_VERSION;
  prefs.putBytes("cfg", &data, sizeof(data));
  Serial.println("[Config] Saved to NVS");
}

void ConfigManager::resetDefaults() {
  memset(&data, 0, sizeof(data));

  // All values come from Defaults.h (site values from Site.h)

  // Voter. DEF_HOST: an IP literal sets hostIP, anything else is a hostname (as the web UI)
  IPAddress hostIp;
  if (hostIp.fromString(DEF_HOST)) {
    data.hostIP = (uint32_t)hostIp;
  } else {
    strlcpy(data.hostname, DEF_HOST, sizeof(data.hostname));
  }
  data.hostPort = DEF_HOST_PORT;
  strlcpy(data.clientPwd, DEF_CLIENT_PWD, sizeof(data.clientPwd));
  strlcpy(data.hostPwd, DEF_HOST_PWD, sizeof(data.hostPwd));
  data.timingOffsetMs = DEF_TIMING_OFFSET_MS;

  // Radio RX
  data.cosMode = DEF_COS_MODE;
  data.cosInvert = DEF_COS_INVERT;
  data.dspSquelchThresh = DEF_DSP_SQUELCH_THRESH;
  data.dspCalib = DEF_DSP_CALIB;
  data.useHwRSSI = DEF_USE_HW_RSSI;
  data.rssiMin = DEF_RSSI_MIN;
  data.rssiMax = DEF_RSSI_MAX;
  data.dspRmsNoSig = 0.0f; // DSP RSSI not calibrated
  data.dspRmsFull = 0.0f;
  data.dspSqlOpen = DEF_DSP_SQL_OPEN;
  data.dspSqlHyst = DEF_DSP_SQL_HYST;
  data.rxPgaGain = DEF_RX_PGA_GAIN;
  data.rxDigitalGainPct = DEF_RX_DIGITAL_GAIN_PCT;
  data.enablePLFilter = DEF_PL_FILTER;
  data.enableDeemp = DEF_DEEMPHASIS;

  // Radio TX
  data.txGainPct = DEF_TX_GAIN_PCT;
  data.pttInvert = DEF_PTT_INVERT;
  data.pttTailMs = DEF_PTT_TAIL_MS;
  data.jitterMs = DEF_JITTER_MS;

  // Network
  data.netIface = DEF_NET_IFACE;
  data.useStaticIP = DEF_USE_STATIC_IP;
  data.staticIP = (uint32_t)IPAddress(DEF_STATIC_IP);
  data.subnetMask = (uint32_t)IPAddress(DEF_SUBNET_MASK);
  data.gateway = (uint32_t)IPAddress(DEF_GATEWAY);
  data.staticDNS = (uint32_t)IPAddress(DEF_STATIC_DNS);
  data.dnsServerIP = 0;
  strlcpy(data.wifiSSID, DEF_WIFI_SSID, sizeof(data.wifiSSID));
  strlcpy(data.wifiPass, DEF_WIFI_PASS, sizeof(data.wifiPass));

  // Syslog
  data.useSyslog = DEF_USE_SYSLOG;
  strlcpy(data.syslogHost, DEF_SYSLOG_HOST, sizeof(data.syslogHost));
  data.syslogPort = DEF_SYSLOG_PORT;

  save();
  _gpsTimingDefaults();
  saveGpsTiming();
  Serial.println("[Config] Reset to defaults");
}
