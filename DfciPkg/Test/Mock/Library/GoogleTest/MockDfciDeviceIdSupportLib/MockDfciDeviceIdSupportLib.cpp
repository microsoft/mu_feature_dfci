/** @file MockDfciDeviceIdSupportLib.cpp
  Google Test mocks for DfciDeviceIdSupportLib.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Library/MockDfciDeviceIdSupportLib.h>

MOCK_INTERFACE_DEFINITION (MockDfciDeviceIdSupportLib);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportV1GetSerialNumber, 1, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetManufacturer, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetProductName, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockDfciDeviceIdSupportLib, DfciIdSupportGetSerialNumber, 2, EFIAPI);
