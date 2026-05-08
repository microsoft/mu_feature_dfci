/** @file MockDfciDeviceIdSupportLib.h
  Google Test mocks for DfciDeviceIdSupportLib.

  Each accessor returns a heap-allocated string buffer and its byte
  size (including the terminating NUL) per the real library's
  contract. Tests use `EXPECT_CALL(...).WillOnce(...)` with the
  custom `SetArgBuffer()` action from `GoogleTestLib` to prime the
  output pointers, or with `Return(EFI_NOT_FOUND)` to exercise
  failure paths.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_DFCI_DEVICE_ID_SUPPORT_LIB_H_
#define MOCK_DFCI_DEVICE_ID_SUPPORT_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/DfciDeviceIdSupportLib.h>
}

struct MockDfciDeviceIdSupportLib {
  MOCK_INTERFACE_DECLARATION (MockDfciDeviceIdSupportLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportV1GetSerialNumber,
    (OUT UINTN  *SerialNumber)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportGetManufacturer,
    (CHAR8  **Manufacturer,
     UINTN  *ManufacturerSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportGetProductName,
    (CHAR8  **ProductName,
     UINTN  *ProductNameSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportGetSerialNumber,
    (CHAR8  **SerialNumber,
     UINTN  *SerialNumberSize)
    );
};

#endif
