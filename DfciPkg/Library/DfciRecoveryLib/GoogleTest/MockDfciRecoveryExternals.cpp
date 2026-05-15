/** @file MockDfciRecoveryExternals.cpp

  Google Test mock definitions for the external dependencies of
  DfciRecoveryLib (DfciDeviceIdSupportLib + BaseCryptLib::Pkcs1v2Encrypt).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "MockDfciRecoveryExternals.h"

MOCK_INTERFACE_DEFINITION (MockDfciDeviceIdSupportLib);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportV1GetSerialNumber, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetManufacturer,   2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetProductName,    2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetSerialNumber,   2, EFIAPI);

MOCK_INTERFACE_DEFINITION (MockPkcs1v2EncryptLib);
MOCK_FUNCTION_DEFINITION (MockPkcs1v2EncryptLib, Pkcs1v2Encrypt, 8, EFIAPI);
