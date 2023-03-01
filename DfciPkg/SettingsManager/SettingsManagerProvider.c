/**@file
SettingsManagerProvider.c

Setting Manger Provider manager

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SettingsManager.h"

LIST_ENTRY  mProviderList = INITIALIZE_LIST_HEAD_VARIABLE (mProviderList); // linked list for the providers

static DFCI_AUTHENTICATION_PROTOCOL  *mAuthenticationProtocol = NULL;

#define CERT_STRING_SIZE    (200)
#define CERT_NOT_AVAILABLE  "No Cert information available"

/**
Helper function to return the string describing the type enum
**/
CHAR8 *
ProviderTypeAsAscii (
  DFCI_SETTING_TYPE  Type
  )
{
  switch (Type) {
    case DFCI_SETTING_TYPE_ENABLE:
      return "ENABLE/DISABLE TYPE";

    case DFCI_SETTING_TYPE_SECUREBOOTKEYENUM:
      return "SECURE BOOT KEY ENUM TYPE";

    case DFCI_SETTING_TYPE_PASSWORD:
      return "PASSWORD TYPE";

    case DFCI_SETTING_TYPE_USBPORTENUM:
      return "USB PORT STATE TYPE";

    case DFCI_SETTING_TYPE_STRING:
      return "STRING TYPE";

    case DFCI_SETTING_TYPE_BINARY:
      return "BINARY TYPE";

    case DFCI_SETTING_TYPE_CERT:
      return "CERT TYPE";

    default:
      return "Unknown";
      break;
  }
}

EFI_STATUS
SetIndividualSettings (
  IN DFCI_SETTING_PROVIDER   *Provider,
  IN CONST CHAR8             *Value,
  IN CONST DFCI_AUTH_TOKEN   *AuthToken,
  IN OUT DFCI_SETTING_FLAGS  *Flags
  )
{
  // STRING and CERT types can be set to <NULL>.  The XML parser return NULL when
  // the value is <Value></Value>.  This prints in prettyXml as <Value/>
  // For these types, convert the set value to a null string.
  if ((Provider->Type != DFCI_SETTING_TYPE_STRING) &&
      (Provider->Type != DFCI_SETTING_TYPE_CERT))
  {
    if (Value == NULL) {
      DEBUG ((DEBUG_INFO, "%a - Value is NULL\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  } else {
    if (Value == NULL) {
      Value = "";
    }
  }

  DEBUG ((DEBUG_INFO, "%a - Value is %a\n", __FUNCTION__, Value));

  return SetProviderValueFromAscii (Provider, Value, AuthToken, Flags);
}

/**
Helper function to set a setting based on ASCII input
**/
EFI_STATUS
EFIAPI
SetSettingFromAscii (
  IN DFCI_SETTING_ID_STRING  Id,
  IN CONST CHAR8             *Value,
  IN CONST DFCI_AUTH_TOKEN   *AuthToken,
  IN OUT DFCI_SETTING_FLAGS  *Flags
  )
{
  DFCI_SETTING_PROVIDER   *Provider = NULL; // need provider to get type
  DFCI_GROUP_LIST_ENTRY   *Group;
  LIST_ENTRY              *Link;
  DFCI_MEMBER_LIST_ENTRY  *Member;
  EFI_STATUS              ReturnStatus;
  EFI_STATUS              Status;

  if (Id == NULL) {
    DEBUG ((DEBUG_INFO, "%a - Id is NULL\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a - Id is %a\n", __FUNCTION__, Id));

  if (AuthToken == NULL) {
    DEBUG ((DEBUG_INFO, "%a - AuthToken is NULL\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a - AuthToken is 0x%X\n", __FUNCTION__, *AuthToken));

  Provider = FindProviderById (Id);
  if (Provider != NULL) {
    // Set the setting (may be first member of a group)
    return SetIndividualSettings (Provider, Value, AuthToken, Flags);
  } else {
    Group = FindGroup (Id);
    if (NULL == Group) {
      DEBUG ((DEBUG_INFO, "%a - Provider for Id (%a) not found in system\n", __FUNCTION__, Id));
      return EFI_NOT_FOUND;
    }

    ReturnStatus = EFI_SUCCESS;
    EFI_LIST_FOR_EACH (Link, &Group->MemberHead) {
      Member   = MEMBER_LIST_ENTRY_FROM_MEMBER_LINK (Link);
      Provider =  FindProviderById (Member->Id);
      ASSERT (Provider != NULL);
      if (Provider == NULL) {
        return EFI_NOT_FOUND;
      }

      DEBUG ((DEBUG_INFO, "Processing Group Setting member %a\n", Provider->Id));
      Status = SetIndividualSettings (Provider, Value, AuthToken, Flags);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a - Error %r settings %a\n", __FUNCTION__, Status, Provider->Id));
        ReturnStatus = Status;
      }
    }
    return ReturnStatus;
  }
}

/**
Helper function to set a setting based on ASCII input
**/
EFI_STATUS
EFIAPI
SetProviderValueFromAscii (
  IN CONST DFCI_SETTING_PROVIDER  *Provider,
  IN CONST CHAR8                  *Value,
  IN CONST DFCI_AUTH_TOKEN        *AuthToken,
  IN OUT DFCI_SETTING_FLAGS       *Flags
  )
{
  CONST VOID  *SetValue  = NULL;
  BOOLEAN     v          = FALSE;
  UINT8       b          = 0;
  UINT8       *ByteArray = NULL;
  UINTN       ValueSize;
  EFI_STATUS  Status;
  UINTN       b64Size;
  UINT8       UsbPortState;
  UINTN       ByteArraySize;

  switch (Provider->Type) {
    /* Enable Type (Boolean)*/
    case DFCI_SETTING_TYPE_ENABLE:
      // convert to BOOLEAN

      if (AsciiStrCmp (Value, "Enabled") == 0) {
        v = TRUE;
        DEBUG ((DEBUG_INFO, "Setting to Enabled\n"));
      } else if (AsciiStrCmp (Value, "Disabled") == 0) {
        v = FALSE;
        DEBUG ((DEBUG_INFO, "Setting to Disabled\n"));
      } else {
        DEBUG ((DEBUG_ERROR, "Invalid Settings Ascii Value for Type Enable (%a)\n", Value));
        return EFI_INVALID_PARAMETER;
      }

      SetValue  = &v;
      ValueSize = sizeof (v);
      break;

    case DFCI_SETTING_TYPE_SECUREBOOTKEYENUM:
      if (AsciiStrCmp (Value, "MsOnly") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to MsOnly\n"));
        b = 0;
      } else if (AsciiStrCmp (Value, "MsPlus3rdParty") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to MsPlus3rdParty\n"));
        b = 1;
      } else if (AsciiStrCmp (Value, "None") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to None\n"));
        b = 2;
      } else {
        DEBUG ((DEBUG_INFO, "Invalid Secure Boot Key Enum Setting. %a\n", Value));
        return EFI_INVALID_PARAMETER;
      }

      SetValue  = &b;
      ValueSize = sizeof (b);
      break;

    case DFCI_SETTING_TYPE_PASSWORD:

      //
      // DFCI_PW_STORE_SIZE is 74.  *2=148. +"eb".len == 150.  So, < 150 is not enough
      // characters, and > 150 is too many for the password size buffer.  Also, the last
      // two characters must be "eb"
      //
      DEBUG ((DEBUG_ERROR, "New Password Value %a\n", Value));

      ByteArraySize = AsciiStrLen (Value);
      if ((ByteArraySize < 4) || (ByteArraySize & 0x01)) {
        // Arbitrary to force at least 1 hex character plus the 0xeb
        // The settings provider will validate the length.
        return EFI_INVALID_PARAMETER;
      }

      ByteArraySize -= 2;
      if (0 != AsciiStriCmp (Value + ByteArraySize, "eb")) {
        DEBUG ((DEBUG_ERROR, "End Byte 'EB' is missing. Not a valid store format. %a\n", Value));
        return EFI_INVALID_PARAMETER;
      }

      ByteArraySize /= 2;

      ByteArray = (UINT8 *)AllocateZeroPool (ByteArraySize);
      if (NULL == ByteArray) {
        return EFI_OUT_OF_RESOURCES;
      }

      // - 2 to account for the "eb" at the end
      Status = AsciiStrHexToBytes (Value, AsciiStrLen (Value) - 2, ByteArray, ByteArraySize);

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Cannot set password. Invalid Character Present \n"));
        return EFI_INVALID_PARAMETER;
      }

      DEBUG ((DEBUG_INFO, "Setting Password. %a\n", Value));

      SetValue  = ByteArray;
      ValueSize = ByteArraySize;
      break;

    case DFCI_SETTING_TYPE_USBPORTENUM:
      if (AsciiStrCmp (Value, "UsbPortEnabled") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to Usb Port Enabled\n"));
        UsbPortState = DfciUsbPortEnabled;
      } else if (AsciiStrCmp (Value, "UsbPortHwDisabled") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to Usb Port HW Disabled\n"));
        UsbPortState = DfciUsbPortHwDisabled;
      } else if (AsciiStrCmp (Value, "UsbPortDataDisabled") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to Usb Data Disabled\n"));
        UsbPortState = DfciUsbPortDataDisabled;
      } else if (AsciiStrCmp (Value, "UsbPortAuthenticated") == 0) {
        DEBUG ((DEBUG_INFO, "Setting to Usb Authenticated\n"));
        UsbPortState = DfciUsbPortAuthenticated;
      } else {
        DEBUG ((DEBUG_INFO, "Invalid or unsupported Usb Port Setting. %a\n", Value));
        return EFI_INVALID_PARAMETER;
      }

      SetValue  = &UsbPortState;
      ValueSize = sizeof (UsbPortState);
      break;

    case DFCI_SETTING_TYPE_STRING:

      ValueSize = AsciiStrnLenS (Value, MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE);

      if ('\0' != Value[ValueSize]) {
        // String is longer than allowed
        DEBUG ((DEBUG_ERROR, "String too long for String type\n"));
        return EFI_INVALID_PARAMETER;
      }

      ValueSize++;           // NULL is part of the setting.

      DEBUG ((DEBUG_INFO, "Setting String. %a\n", Value));
      SetValue = Value;

      break;

    case DFCI_SETTING_TYPE_BINARY:
    case DFCI_SETTING_TYPE_CERT:  // On writes, CERTS are binary blobs

      b64Size   = AsciiStrnLenS (Value, MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE);
      ValueSize = 0;
      Status    = Base64Decode (Value, b64Size, NULL, &ValueSize);
      if (Status != EFI_BUFFER_TOO_SMALL) {
        DEBUG ((DEBUG_ERROR, "Cannot query binary blob size. Code = %r\n", Status));
        return EFI_INVALID_PARAMETER;
      }

      ByteArray = (UINT8 *)AllocatePool (ValueSize);

      if (NULL == ByteArray) {
        DEBUG ((DEBUG_ERROR, "Cannot allocate memory for binary blob\n"));
        return EFI_OUT_OF_RESOURCES;
      }

      Status = Base64Decode (Value, b64Size, ByteArray, &ValueSize);
      if (EFI_ERROR (Status)) {
        FreePool (ByteArray);
        DEBUG ((DEBUG_ERROR, "Cannot set binary data. Code=%r\n", Status));
        return Status;
      }

      SetValue = ByteArray;
      DEBUG ((DEBUG_INFO, "Setting BINARY data\n"));
      DUMP_HEX (DEBUG_VERBOSE, 0, SetValue, ValueSize, "");
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Failed - SetProviderValueFromAscii for ID %a Unsupported Type = 0x%X\n", Provider->Id, Provider->Type));
      return EFI_INVALID_PARAMETER;
  }

  Status = mSystemSettingAccessProtocol.Set (&mSystemSettingAccessProtocol, Provider->Id, AuthToken, Provider->Type, ValueSize, SetValue, Flags);
  if (NULL != ByteArray) {
    FreePool (ByteArray);
  }

  return Status;
}

#define ENABLED_STRING_SIZE                (13)
#define SECURE_BOOT_ENUM_STRING_SIZE       (20)
#define SYSTEM_PASSWORD_STATE_STRING_SIZE  (30)
#define USB_PORT_STATE_STRING_SIZE         (21)

/**
Helper function to Print out the Value as Ascii text.
NOTE: -- This must match the XML format

Caller must free the return string if not null;

@param Provider : Pointer to provider instance the value should be printed for
@param Current  : TRUE if provider current value.  FALSE for provider default value.

@ret  String allocated with AllocatePool containing Ascii printable value.
**/
CHAR8 *
ProviderValueAsAscii (
  DFCI_SETTING_PROVIDER  *Provider,
  BOOLEAN                Current
  )
{
  EFI_STATUS  Status;
  CHAR8       *Value = NULL;
  UINTN       AsciiSize;
  UINT8       *Buffer;
  BOOLEAN     v = FALSE; // Boolean Types
  UINT8       b = 0xFF;  // Byte types (small enum)
  UINTN       ValueSize;

  switch (Provider->Type) {
    case DFCI_SETTING_TYPE_ENABLE:
      ValueSize = sizeof (v);
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, &v);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, &v);
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      Value = AllocateZeroPool (ENABLED_STRING_SIZE);
      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed - Couldn't allocate for string. \n"));
        break;
      }

      if (v == ENABLE_INCONSISTENT) {
        AsciiStrCpyS (Value, ENABLED_STRING_SIZE, "Inconsistent");
      } else if (v) {
        AsciiStrCpyS (Value, ENABLED_STRING_SIZE, "Enabled");
      } else {
        AsciiStrCpyS (Value, ENABLED_STRING_SIZE, "Disabled");
      }

      break;

    case DFCI_SETTING_TYPE_SECUREBOOTKEYENUM:
      ValueSize = sizeof (b);
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, &b);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, &b);
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      Value = AllocateZeroPool (SECURE_BOOT_ENUM_STRING_SIZE);
      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed - Couldn't allocate for string. \n"));
        break;
      }

      if (b == 0) {
        AsciiStrCpyS (Value, SECURE_BOOT_ENUM_STRING_SIZE, "MsOnly");
      } else if (b == 1) {
        AsciiStrCpyS (Value, SECURE_BOOT_ENUM_STRING_SIZE, "MsPlus3rdParty");
      } else if (b == 3) {
        // This is a special case.  Only supported as output.
        AsciiStrCpyS (Value, SECURE_BOOT_ENUM_STRING_SIZE, "Custom");
      } else {
        AsciiStrCpyS (Value, SECURE_BOOT_ENUM_STRING_SIZE, "None");
      }

      break;

    case DFCI_SETTING_TYPE_PASSWORD:
      ValueSize = sizeof (v);
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, &v);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, &v);
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      Value = AllocateZeroPool (SYSTEM_PASSWORD_STATE_STRING_SIZE);
      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed - Couldn't allocate for string. \n"));
        break;
      }

      if (v) {
        AsciiStrCpyS (Value, SYSTEM_PASSWORD_STATE_STRING_SIZE, "System Password Set");
      } else {
        AsciiStrCpyS (Value, SYSTEM_PASSWORD_STATE_STRING_SIZE, "No System Password");
      }

      break;

    case DFCI_SETTING_TYPE_USBPORTENUM:
      ValueSize = sizeof (b);
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, &b);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, &b);
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      Value = AllocateZeroPool (USB_PORT_STATE_STRING_SIZE);
      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed - Couldn't allocate for string. \n"));
        break;
      }

      if (b == DfciUsbPortHwDisabled) {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "UsbPortHwDisabled");
      } else if (b == DfciUsbPortEnabled) {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "UsbPortEnabled");
      } else if (b == DfciUsbPortDataDisabled) {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "UsbPortDataDisabled");
      } else if (b == DfciUsbPortAuthenticated) {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "UsbPortAuthenticated");
      } else if (b == ENABLE_INCONSISTENT) {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "Inconsistent");
      } else {
        AsciiStrCpyS (Value, USB_PORT_STATE_STRING_SIZE, "UnsupportedValue");
      }

      break;

    case DFCI_SETTING_TYPE_STRING:

      ValueSize = 0;
      Value     = NULL;
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, NULL);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, NULL);
      }

      if (EFI_BUFFER_TOO_SMALL != Status) {
        DEBUG ((DEBUG_ERROR, "Failed - Expected Buffer Too Small Current=%d for ID %a Status = %r\n", Current, Provider->Id, Status));
        break;
      }

      if (0 == ValueSize ) {
        break;                   // Return NULL for Value silently
      }

      if (ValueSize > MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE) {
        DEBUG ((DEBUG_ERROR, "Failed - ValueSize invalid for ID %a. Size=%ld\n", Provider->Id, ValueSize));
        break;
      }

      Value = AllocatePool (ValueSize);

      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "Allocation failed. \n"));
        break;
      }

      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, Value);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, Value);
      }

      if (EFI_ERROR (Status)) {
        FreePool (Value);
        Value = NULL;
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      if (ValueSize == AsciiStrnLenS (Value, ValueSize)) {
        FreePool (Value);
        Value = NULL;
        DEBUG ((DEBUG_ERROR, "String too long for String type\n"));
        break;
      }

      break;

    case DFCI_SETTING_TYPE_CERT:

      ValueSize = 0;
      Buffer    = NULL;
      Value     = NULL;
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, NULL);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, NULL);
      }

      if ((ValueSize != 0) && (EFI_BUFFER_TOO_SMALL != Status)) {
        DEBUG ((DEBUG_ERROR, "Failed - Expected Buffer Too Small for Current=%d, ID %a Status = %r\n", Current, Provider->Id, Status));
        break;
      }

      if (0 == ValueSize ) {
        ValueSize = sizeof ("");
        Value     = AllocatePool (ValueSize);
        if (NULL != Value) {
          AsciiStrnCpyS (Value, ValueSize, "", ValueSize-1);
        } else {
          DEBUG ((DEBUG_ERROR, "Unable to allocate return value for ID %a\n", Provider->Id));
        }

        break;
      }

      if (ValueSize > MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE) {
        DEBUG ((DEBUG_ERROR, "Failed - Incorrect size for ID %a\n", Provider->Id));
        break;
      }

      Buffer = AllocatePool (ValueSize);
      if (NULL == Buffer) {
        DEBUG ((DEBUG_ERROR, "Failed - to allocate buffer for ID %a\n", Provider->Id));
        break;
      }

      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, Buffer);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, Buffer);
      }

      if (EFI_ERROR (Status)) {
        FreePool (Buffer);
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      if (mAuthenticationProtocol == NULL) {
        Status = gBS->LocateProtocol (
                        &gDfciAuthenticationProtocolGuid,
                        NULL,
                        (VOID **)&mAuthenticationProtocol
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a - Failed to locate Authentication Protocol. Code=%r\n", __FUNCTION__, Status));
          mAuthenticationProtocol = NULL;
          FreePool (Buffer);
          return NULL;
        }
      }

      Status =  mAuthenticationProtocol->GetCertInfo (
                                           mAuthenticationProtocol,
                                           0,
                                           Buffer,
                                           ValueSize,
                                           DFCI_CERT_THUMBPRINT,
                                           DFCI_CERT_FORMAT_CHAR8_UI,
                                           (VOID **)&Value,
                                           &ValueSize
                                           );
      FreePool (Buffer);

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Unable to get strings from the certificate\n"));
        ValueSize = sizeof (CERT_NOT_AVAILABLE);
        Value     = AllocatePool (ValueSize);
        if (NULL != Value) {
          AsciiStrnCpyS (Value, ValueSize, CERT_NOT_AVAILABLE, ValueSize-sizeof (CHAR8));
        }
      }

      break;

    case DFCI_SETTING_TYPE_BINARY:

      ValueSize = 0;
      Buffer    = NULL;
      Value     = NULL;
      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, NULL);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, NULL);
      }

      if (EFI_BUFFER_TOO_SMALL != Status) {
        DEBUG ((DEBUG_ERROR, "Failed - Expected Buffer Too Small for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      if (0 == ValueSize ) {
        break;                     // Return NULL for Value silently
      }

      if (ValueSize > MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE) {
        DEBUG ((DEBUG_ERROR, "Failed - Incorrect size for ID %a\n", Provider->Id));
        break;
      }

      Buffer = AllocatePool (ValueSize);
      if (NULL == Buffer) {
        DEBUG ((DEBUG_ERROR, "Failed - to allocate buffer for ID %a\n", Provider->Id));
        break;
      }

      if (Current) {
        Status = Provider->GetSettingValue (Provider, &ValueSize, Buffer);
      } else {
        Status = Provider->GetDefaultValue (Provider, &ValueSize, Buffer);
      }

      if (EFI_ERROR (Status)) {
        FreePool (Buffer);
        DEBUG ((DEBUG_ERROR, "Failed - GetSettingValue for ID %a Status = %r\n", Provider->Id, Status));
        break;
      }

      Status = Base64Encode (Buffer, ValueSize, NULL, &AsciiSize);
      if (Status != EFI_BUFFER_TOO_SMALL) {
        DEBUG ((DEBUG_ERROR, "Cannot query ascii String size. Code = %r\n", Status));
        return NULL;
      }

      Value = (CHAR8 *)AllocatePool (AsciiSize);

      if (Value == NULL) {
        DEBUG ((DEBUG_ERROR, "%a - Failed to allocate pool.\n", __FUNCTION__));
        return NULL;
      }

      Status = Base64Encode (Buffer, ValueSize, Value, &AsciiSize);
      if (EFI_ERROR (Status)) {
        FreePool (Value);
        FreePool (Buffer);
        DEBUG ((DEBUG_ERROR, "Cannot set ascii data. Code=%r\n", Status));
        return NULL;
      }

      FreePool (Buffer);
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Failed - ProviderValueAsAscii for ID %a Unsupported Type = 0x%X\n", Provider->Id, Provider->Type));
      break;
  }

  return Value;
}

/*
Helper function to print out one Setting Provider
*/
VOID
DebugPrintProviderEntry (
  DFCI_SETTING_PROVIDER  *Provider
  )
{
  CHAR8  *Value        = ProviderValueAsAscii (Provider, TRUE);
  CHAR8  *DefaultValue = ProviderValueAsAscii (Provider, FALSE);

  DEBUG ((DEBUG_INFO, "Id:            %a\n", Provider->Id));
  DEBUG ((DEBUG_INFO, "Printing Prov@ 0x%X\n", (UINTN)Provider));
  DEBUG ((DEBUG_INFO, "Type:          %a\n", ProviderTypeAsAscii (Provider->Type)));
  DEBUG ((DEBUG_INFO, "Flags:         0x%X\n", Provider->Flags));
  DEBUG ((DEBUG_INFO, "Current Value: %a", Value));   // Split \n to separate DEBUG in case value is too long
  DEBUG ((DEBUG_INFO, "\n"));
  DEBUG ((DEBUG_INFO, "Default Value: %a", DefaultValue));
  DEBUG ((DEBUG_INFO, "\n"));

  if (DefaultValue != NULL) {
    FreePool (DefaultValue);
  }

  if (Value != NULL) {
    FreePool (Value);
  }
}

/*
Helper function to print out all Setting Providers currently registered
*/
VOID
DebugPrintProviderList (
  )
{
  LIST_ENTRY                        *Link = NULL;
  DFCI_SETTING_PROVIDER_LIST_ENTRY  *Prov = NULL;

  DEBUG ((DEBUG_INFO, "-----------------------------------------------------\n"));
  DEBUG ((DEBUG_INFO, "START PRINTING ALL REGISTERED SETTING PROVIDERS\n"));
  DEBUG ((DEBUG_INFO, "-----------------------------------------------------\n"));

  EFI_LIST_FOR_EACH (Link, &mProviderList) {
    // Convert Link Node into object stored
    Prov = PROV_LIST_ENTRY_FROM_LINK (Link);
    DebugPrintProviderEntry (&Prov->Provider);
  }
  DEBUG ((DEBUG_INFO, "-----------------------------------------------------\n"));
  DEBUG ((DEBUG_INFO, " END PRINTING ALL REGISTERED SETTING PROVIDERS\n"));
  DEBUG ((DEBUG_INFO, "-----------------------------------------------------\n"));
}

/*
Function to find a setting provider given an ID
If it isn't found a NULL will be returned
*/
DFCI_SETTING_PROVIDER *
FindProviderById (
  DFCI_SETTING_ID_STRING  Id
  )
{
  LIST_ENTRY                        *Link = NULL;
  DFCI_SETTING_PROVIDER_LIST_ENTRY  *Prov = NULL;
  DFCI_SETTING_ID_STRING            RealId;

  if ((Id[0] >= '0') && (Id[0] <= '9')) {
    // If first character is numeric
    RealId = DfciV1TranslateString (Id);
    if (RealId == NULL) {
      DEBUG ((DEBUG_ERROR, "FindProviderById - Failed to translate (%a)\n", Id));
      return NULL;
    }
  } else {
    RealId = Id;
  }

  EFI_LIST_FOR_EACH (Link, &mProviderList) {
    // Convert Link Node into object stored
    Prov = PROV_LIST_ENTRY_FROM_LINK (Link);

    if (0 == AsciiStrnCmp (Prov->Provider.Id, RealId, DFCI_MAX_ID_LEN)) {
      DEBUG ((DEBUG_INFO, "FindProviderById - Found (%a)\n", Id));
      return &Prov->Provider;
    }
  }
  return NULL;
}

/**
Registers a Setting Provider with the System Settings module

@param  This                 Protocol instance pointer.
@param  Provider             Provider pointer to register

@retval EFI_SUCCESS          The provider registered.
@retval ERROR                The provider could not be registered.

**/
EFI_STATUS
EFIAPI
RegisterProvider (
  IN DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  *This,
  IN DFCI_SETTING_PROVIDER                   *Provider
  )
{
  DFCI_SETTING_PROVIDER_LIST_ENTRY  *Entry = NULL;
  DFCI_SETTING_PROVIDER             *ExistingProvider;
  EFI_STATUS                        Status;

  if (Provider == NULL) {
    DEBUG ((DEBUG_ERROR, "Invalid Provider parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  if ((Provider->Id[0] >= '0') && (Provider->Id[0] <= '9')) {
    // If first character is numeric
    DEBUG ((DEBUG_ERROR, "Invalid Provider Id %a\n", Provider->Id));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "Registering Provider with ID %a\n", Provider->Id));

  // check to make sure it doesn't already exist.
  ExistingProvider = FindProviderById (Provider->Id);
  if (ExistingProvider != NULL) {
    DEBUG ((DEBUG_ERROR, "Error - Can't register a provider more than once.  id(%a)\n", Provider->Id));
    ASSERT (ExistingProvider == NULL);
    return EFI_INVALID_PARAMETER;
  }

  // Check function pointers
  ASSERT (Provider->SetDefaultValue != NULL);
  ASSERT (Provider->GetDefaultValue != NULL);
  ASSERT (Provider->GetSettingValue != NULL);
  ASSERT (Provider->SetSettingValue != NULL);

  // Allocate memory for provider
  Entry = AllocateZeroPool (sizeof (DFCI_SETTING_PROVIDER_LIST_ENTRY));
  if (Entry == NULL) {
    DEBUG ((DEBUG_ERROR, "RegisterProvider - Couldn't allocate memory for entry\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Entry->Signature = DFCI_SETTING_PROVIDER_LIST_ENTRY_SIGNATURE;

  // Copy provider to new entry
  CopyMem (&Entry->Provider, Provider, sizeof (DFCI_SETTING_PROVIDER));

  // insert into list
  InsertTailList (&mProviderList, &Entry->Link);

  Status = RegisterSettingToGroup (Provider->Id);
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "Error registering %a to a group. Code=%r", Provider->Id, Status));
  }

  // Groups are not known by a provider, so don't return group error to the provider.
  return EFI_SUCCESS;
}

/**
Function sets the providers to default for
any provider that contains the FilterFlag in its flags
**/
EFI_STATUS
EFIAPI
ResetAllProvidersToDefaultsWithMatchingFlags (
  DFCI_SETTING_FLAGS  FilterFlag
  )
{
  EFI_STATUS                        Status = EFI_SUCCESS;
  LIST_ENTRY                        *Link  = NULL;
  DFCI_SETTING_PROVIDER_LIST_ENTRY  *Prov  = NULL;

  EFI_LIST_FOR_EACH (Link, &mProviderList) {
    // Convert Link Node into object stored
    Prov = PROV_LIST_ENTRY_FROM_LINK (Link);
    if (Prov->Provider.Flags & FilterFlag) {
      DEBUG ((DEBUG_INFO, "%a - Setting Provider %a to defaults as part of a Reset request. \n", __FUNCTION__, Prov->Provider.Id));
      Status = Prov->Provider.SetDefaultValue (&(Prov->Provider));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a - Failed to Set Provider (%a) To DefaultPMask Value. Status = %r\n", __FUNCTION__, Prov->Provider.Id, Status));
      }
    }
  }
  return EFI_SUCCESS;
}
