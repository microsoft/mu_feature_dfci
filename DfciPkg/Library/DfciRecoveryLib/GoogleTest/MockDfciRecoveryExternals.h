/** @file MockDfciRecoveryExternals.h

  Google Test mocks for the external (non-mocked-by-shared-libs)
  dependencies of DfciRecoveryLib:

    * DfciDeviceIdSupportLib (DfciIdSupportGet{SerialNumber, ProductName,
      Manufacturer})
    * BaseCryptLib::Pkcs1v2Encrypt

  These mocks are scoped to the DfciRecoveryLib test executable. They are
  not published as reusable mock libraries because no other consumer
  currently needs them.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_DFCI_RECOVERY_EXTERNALS_H_
#define MOCK_DFCI_RECOVERY_EXTERNALS_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseCryptLib.h>
  #include <Library/DfciDeviceIdSupportLib.h>
}

//
// Mock of DfciDeviceIdSupportLib.
//
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
    (CHAR8 **Manufacturer,
     UINTN *ManufacturerSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportGetProductName,
    (CHAR8 **ProductName,
     UINTN *ProductNameSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    DfciIdSupportGetSerialNumber,
    (CHAR8 **SerialNumber,
     UINTN *SerialNumberSize)
    );
};

//
// Mock of the single BaseCryptLib entry point that DfciRecoveryLib uses.
// We do not pull in or extend MockBaseCryptLib because Pkcs1v2Encrypt is
// not declared there and adding it would touch CryptoPkg.
//
struct MockPkcs1v2EncryptLib {
  MOCK_INTERFACE_DECLARATION (MockPkcs1v2EncryptLib);

  MOCK_FUNCTION_DECLARATION (
    BOOLEAN,
    Pkcs1v2Encrypt,
    (IN   CONST UINT8  *PublicKey,
     IN   UINTN        PublicKeySize,
     IN   UINT8        *InData,
     IN   UINTN        InDataSize,
     IN   CONST UINT8  *PrngSeed   OPTIONAL,
     IN   UINTN        PrngSeedSize,
     OUT  UINT8        **EncryptedData,
     OUT  UINTN        *EncryptedDataSize)
    );
};

#endif // MOCK_DFCI_RECOVERY_EXTERNALS_H_
