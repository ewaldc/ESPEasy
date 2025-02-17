#ifndef HELPERS__PLUGIN_HELPER_SERIAL_H
#define HELPERS__PLUGIN_HELPER_SERIAL_H


#include "../../ESPEasy_common.h"

#ifdef PLUGIN_USES_SERIAL

#include <ESPeasySerial.h>


struct ESPeasySerialType;

String serialHelper_getSerialTypeLabel(ESPEasySerialPort serType);

void   serialHelper_log_GpioDescription(ESPEasySerialPort typeHint,
                                        int               config_pin1,
                                        int               config_pin2);

String serialHelper_getGpioDescription(ESPEasySerialPort typeHint,
                                       int               config_pin1,
                                       int               config_pin2,
                                       const String    & newline);

void serialHelper_getGpioNames(struct EventStruct *event,
                               bool                rxOptional = false,
                               bool                txOptional = false);

void serialHelper_modbus_getGpioNames(struct EventStruct *event,
                               bool                rxOptional = false,
                               bool                txOptional = false,
                               bool                DE_RE_optional = true);

int8_t            serialHelper_getRxPin(struct EventStruct *event);

int8_t            serialHelper_getTxPin(struct EventStruct *event);

ESPEasySerialPort serialHelper_getSerialType(struct EventStruct *event);

String            serialHelper_getSerialTypeLabel(struct EventStruct *event);

#ifndef DISABLE_SC16IS752_Serial
void              serialHelper_addI2CuartSelectors(int address,
                                                   int channel);
#endif // ifndef DISABLE_SC16IS752_Serial

void serialHelper_webformLoad(struct EventStruct *event);

// These helper functions were made to create a generic interface to setup serial port config.
// See issue #2343 and Pull request https://github.com/letscontrolit/ESPEasy/pull/2352
// For now P020 and P044 have been reverted to make them work again.
void serialHelper_webformLoad(struct EventStruct *event,
                              bool                allowSoftwareSerial);

void serialHelper_webformLoad(ESPEasySerialPort port,
                              int               rxPinDef,
                              int               txPinDef,
                              bool              allowSoftwareSerial);

void serialHelper_webformSave(uint8_t& port,
                              int8_t & rxPin,
                              int8_t & txPin);

void    serialHelper_webformSave(struct EventStruct *event);

bool    serialHelper_isValid_serialconfig(uint8_t serialconfig);

void    serialHelper_serialconfig_webformLoad(struct EventStruct *event,
                                              uint8_t             currentSelection);

uint8_t serialHelper_serialconfig_webformSave();

// Used by some plugins, which used several TaskDevicePluginConfigLong
uint8_t serialHelper_convertOldSerialConfig(uint8_t newLocationConfig);

#endif // ifdef PLUGIN_USES_SERIAL

#endif // ifndef HELPERS__PLUGIN_HELPER_SERIAL_H
