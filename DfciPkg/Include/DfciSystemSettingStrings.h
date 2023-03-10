/** @file
DfciSystemSettingStrings.h

These are the setting strings.

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DFCI_SETTING_STRINGS_H__
#define __DFCI_SETTING_STRINGS_H__

//
// DFCI Setting Type
//
#define DFCI_STR_SETTING_TYPE_ENABLE             "ENABLE/DISABLE TYPE"
#define DFCI_STR_SETTING_TYPE_SECUREBOOTKEYENUM  "SECURE BOOT KEY ENUM TYPE"
#define DFCI_STR_SETTING_TYPE_PASSWORD           "PASSWORD TYPE"
#define DFCI_STR_SETTING_TYPE_USBPORTENUM        "USB PORT STATE TYPE"
#define DFCI_STR_SETTING_TYPE_STRING             "STRING TYPE"
#define DFCI_STR_SETTING_TYPE_BINARY             "BINARY TYPE"
#define DFCI_STR_SETTING_TYPE_CERT               "CERT TYPE"

//
// Enable/Disable
//
#define DFCI_STR_ENABLED   "Enabled"
#define DFCI_STR_DISABLED  "Disabled"

//
// Secure Boot Key
//
#define DFCI_STR_SECURE_BOOT_KEY_MS_ONLY       "MsOnly"
#define DFCI_STR_SECURE_BOOT_KEY_MS_3RD_PARTY  "MsPlus3rdParty"
#define DFCI_STR_SECURE_BOOT_KEY_NONE          "None"
#define DFCI_STR_SECURE_BOOT_KEY_CUSTOM        "Custom"

//
// System Password
//
#define DFCI_STR_SYSTEM_PASSWORD_SET      "System Password Set"
#define DFCI_STR_SYSTEM_PASSWORD_NOT_SET  "No System Password"

//
// USB Port State
//
#define DFCI_STR_USB_PORT_ENABLED        "UsbPortEnabled"
#define DFCI_STR_USB_PORT_HW_DISABLED    "UsbPortHwDisabled"
#define DFCI_STR_USB_PORT_DATA_DISABLED  "UsbPortDataDisabled"
#define DFCI_STR_USB_PORT_AUTHENTICATED  "UsbPortAuthenticated"

//
// Misc
//
#define DFCI_STR_INCONSISTENT        "Inconsistent"
#define DFCI_STR_UNKNOWN             "Unknown"
#define DFCI_STR_UNSUPPORTED_VALUE   "UnsupportedValue"
#define DFCI_STR_CERT_NOT_AVAILABLE  "No Cert information available"


#endif // __DFCI_SETTING_STRINGS_H__
