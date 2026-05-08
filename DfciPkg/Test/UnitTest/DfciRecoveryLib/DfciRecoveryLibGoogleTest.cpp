/** @file DfciRecoveryLibGoogleTest.cpp
  Public-API host-based unit tests for DfciRecoveryLib. Verifies the
  documented contract of the two functions exposed by
  `DfciPkg/Include/Library/DfciRecoveryLib.h`:

    * GetRecoveryChallenge
    * EncryptRecoveryChallenge

  External dependencies (gBS, gRT, EFI_RNG_PROTOCOL, BaseCryptLib,
  DfciDeviceIdSupportLib) are mocked via gMock + FunctionMockLib so
  every call into the OS / crypto stack is observable and
  controllable from each test case.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
#include <GoogleTest/Library/MockBaseCryptLib.h>
#include <GoogleTest/Library/MockDfciDeviceIdSupportLib.h>
#include <GoogleTest/Protocol/MockRng.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Library/DfciRecoveryLib.h>
  #include <Protocol/Rng.h>
}

using namespace testing;

//
// gRngProtocol is the static EFI_RNG_PROTOCOL instance exported by
// MdePkg/Test/Mock/Library/GoogleTest/Protocol/MockRng.cpp. Its
// GetInfo / GetRng function pointers route into the gMock interface
// MockRng below, so EXPECT_CALL on RngMock controls what the CUT
// sees through `RngProtocol->GetRNG()`.
//
extern "C" EFI_RNG_PROTOCOL  *gRngProtocol;

// ===========================================================================
// Helpers
// ===========================================================================

//
// Build a heap-allocated, NUL-terminated CHAR8* and matching size
// (size INCLUDES the terminator, matching DfciDeviceIdSupportLib's
// contract). The buffer is allocated with AllocatePool so the CUT
// can safely FreePool it.
//
STATIC
EFI_STATUS
CloneAsciiToHeap (
  IN  CONST CHAR8  *Source,
  OUT CHAR8        **Out,
  OUT UINTN        *OutSize
  )
{
  UINTN  Length;

  if ((Out == NULL) || (Source == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Length = AsciiStrLen (Source) + 1;
  *Out   = (CHAR8 *)AllocatePool (Length);
  if (*Out == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (*Out, Source, Length);
  if (OutSize != NULL) {
    *OutSize = Length;
  }

  return EFI_SUCCESS;
}

// ===========================================================================
// Fixture for tests that exercise GetRecoveryChallenge / EncryptRecoveryChallenge
// ===========================================================================

class DfciRecoveryLibTest : public ::testing::Test {
protected:
  StrictMock<MockUefiBootServicesTableLib>     BsMock;
  StrictMock<MockUefiRuntimeServicesTableLib>  RtMock;
  StrictMock<MockBaseCryptLib>                 CryptMock;
  StrictMock<MockDfciDeviceIdSupportLib>       IdMock;
  StrictMock<MockRng>                          RngMock;

  EFI_TIME  HappyTime = {
    /* Year       */ 2026,
    /* Month      */ 5,
    /* Day        */ 8,
    /* Hour       */ 12,
    /* Minute     */ 0,
    /* Second     */ 0,
    /* Pad1       */ 0,
    /* Nanosecond */ 0,
    /* TimeZone   */ 0,
    /* Daylight   */ 0,
    /* Pad2       */ { 0, 0 }
  };

  //
  // Wires LocateProtocol to return the static gRngProtocol exactly
  // `Times` time(s).
  //
  void
  ExpectLocateRng (
    int  Times = 1
    )
  {
    EXPECT_CALL (BsMock, gBS_LocateProtocol (_, _, _))
      .Times (Times)
      .WillRepeatedly (
         DoAll (
           SetArgPointee<2>((VOID *)gRngProtocol),
           Return (EFI_SUCCESS)
           )
         );
  }

  //
  // Configures RngMock.GetRng so the first algorithm attempted always
  // succeeds, returning a deterministic byte pattern derived from
  // (FillByte + index). Each call uses a different FillByte so two
  // back-to-back calls produce different bytes.
  //
  void
  ExpectGetRngSuccess (
    UINT8  FillByte,
    UINTN  ExpectedLength
    )
  {
    EXPECT_CALL (RngMock, GetRng (_, _, ExpectedLength, _))
      .WillOnce (
         Invoke (
           [FillByte](
             EFI_RNG_PROTOCOL    *,
             EFI_RNG_ALGORITHM   *,
             UINTN               Len,
             UINT8               *Out
             ) -> EFI_STATUS {
             for (UINTN I = 0; I < Len; I++) {
               Out[I] = (UINT8)((FillByte + I) & 0xFF);
             }

             return EFI_SUCCESS;
           }
           )
         );
  }

  //
  // Default success-path device ID expectations.
  //
  void
  ExpectDeviceIdSuccess (
    CONST CHAR8  *Serial = "SN-12345",
    CONST CHAR8  *Product = "TestProduct",
    CONST CHAR8  *Manuf = "TestManufacturer"
    )
  {
    EXPECT_CALL (IdMock, DfciIdSupportGetSerialNumber (_, _))
      .WillOnce (
         Invoke (
           [Serial](CHAR8 **Out, UINTN *Size) -> EFI_STATUS {
             return CloneAsciiToHeap (Serial, Out, Size);
           }
           )
         );
    EXPECT_CALL (IdMock, DfciIdSupportGetProductName (_, _))
      .WillOnce (
         Invoke (
           [Product](CHAR8 **Out, UINTN *Size) -> EFI_STATUS {
             return CloneAsciiToHeap (Product, Out, Size);
           }
           )
         );
    EXPECT_CALL (IdMock, DfciIdSupportGetManufacturer (_, _))
      .WillOnce (
         Invoke (
           [Manuf](CHAR8 **Out, UINTN *Size) -> EFI_STATUS {
             return CloneAsciiToHeap (Manuf, Out, Size);
           }
           )
         );
  }
};

// ===========================================================================
// GetRecoveryChallenge - parameter validation
// ===========================================================================

TEST_F (DfciRecoveryLibTest, GetChallenge_NullChallengePtrIsRejected) {
  UINTN  Size = 0xDEAD;

  // Per docstring: NULL Challenge must produce EFI_INVALID_PARAMETER
  // and no side-effect on any external dependency.
  EFI_STATUS  Status = GetRecoveryChallenge (NULL, &Size);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

// ===========================================================================
// GetRecoveryChallenge - happy path
// ===========================================================================

TEST_F (DfciRecoveryLibTest, GetChallenge_HappyPath_AllocatesAndPopulates) {
  DFCI_RECOVERY_CHALLENGE  *Challenge = NULL;
  UINTN                    ChallengeSize = 0;
  EFI_STATUS               Status;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .WillOnce (
       DoAll (
         SetArgBuffer<0>(&HappyTime, sizeof (HappyTime)),
         Return (EFI_SUCCESS)
         )
       );
  ExpectLocateRng ();
  ExpectGetRngSuccess (0x10, DFCI_RECOVERY_NONCE_SIZE);
  ExpectDeviceIdSuccess ();

  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);
  ASSERT_EQ (Status, EFI_SUCCESS);
  ASSERT_NE (Challenge, nullptr);

  // Documented V2 deprecation: SerialNumber field is always 0.
  EXPECT_EQ (Challenge->SerialNumber, (UINTN)0);

  // Timestamp must be exactly what gRT_GetTime returned.
  EXPECT_EQ (CompareMem (&Challenge->Timestamp, &HappyTime, sizeof (EFI_TIME)), 0);

  // Nonce must be the deterministic stream we asked the mock for.
  for (UINTN I = 0; I < DFCI_RECOVERY_NONCE_SIZE; I++) {
    EXPECT_EQ (Challenge->Nonce.Bytes[I], (UINT8)((0x10 + I) & 0xFF));
  }

  // ChallengeSize accounts for the variable-length identifier tail.
  EXPECT_GE (ChallengeSize, sizeof (DFCI_RECOVERY_CHALLENGE) + 1);
  EXPECT_LE (ChallengeSize, sizeof (DFCI_RECOVERY_CHALLENGE) + DFCI_MULTI_STRING_MAX_SIZE);

  // The MultiString must be NUL-terminated within the allotted bytes.
  EXPECT_LE (
    AsciiStrnLenS (&Challenge->MultiString[0], DFCI_MULTI_STRING_MAX_SIZE),
    (UINTN)(DFCI_MULTI_STRING_MAX_SIZE - 1)
    );

  FreePool (Challenge);
}

// ===========================================================================
// GetRecoveryChallenge - propagates errors from each external dep
// ===========================================================================

TEST_F (DfciRecoveryLibTest, GetChallenge_LocateRngFailureIsPropagated) {
  DFCI_RECOVERY_CHALLENGE  *Challenge = (DFCI_RECOVERY_CHALLENGE *)0x1;
  UINTN                    ChallengeSize = 0xDEAD;
  EFI_STATUS               Status;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .WillOnce (
       DoAll (
         SetArgBuffer<0>(&HappyTime, sizeof (HappyTime)),
         Return (EFI_SUCCESS)
         )
       );
  // RNG protocol cannot be located.
  EXPECT_CALL (BsMock, gBS_LocateProtocol (_, _, _))
    .WillOnce (Return (EFI_NOT_FOUND));

  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);
  EXPECT_NE (Status, EFI_SUCCESS);
  EXPECT_EQ (Challenge, nullptr);
}

TEST_F (DfciRecoveryLibTest, GetChallenge_GetTimeFailureIsPropagated) {
  DFCI_RECOVERY_CHALLENGE  *Challenge = (DFCI_RECOVERY_CHALLENGE *)0x1;
  UINTN                    ChallengeSize = 0xDEAD;
  EFI_STATUS               Status;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .WillOnce (Return (EFI_DEVICE_ERROR));
  // GetRandomValue / DfciIdSupport must NOT be invoked when GetTime fails.

  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
  EXPECT_EQ (Challenge, nullptr);
}

TEST_F (DfciRecoveryLibTest, GetChallenge_GetRngFailureIsPropagated) {
  DFCI_RECOVERY_CHALLENGE  *Challenge = (DFCI_RECOVERY_CHALLENGE *)0x1;
  UINTN                    ChallengeSize = 0xDEAD;
  EFI_STATUS               Status;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .WillOnce (
       DoAll (
         SetArgBuffer<0>(&HappyTime, sizeof (HappyTime)),
         Return (EFI_SUCCESS)
         )
       );
  ExpectLocateRng ();
  // Every secure algorithm fails with a non-EFI_UNSUPPORTED error;
  // GetRandomValue must propagate it. (EFI_UNSUPPORTED would cause the
  // loop to try the next algorithm, which is a different test path.)
  EXPECT_CALL (RngMock, GetRng (_, _, DFCI_RECOVERY_NONCE_SIZE, _))
    .WillOnce (Return (EFI_DEVICE_ERROR));

  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
  EXPECT_EQ (Challenge, nullptr);
}

// ===========================================================================
// GetRecoveryChallenge - DfciDeviceIdSupport behavior
// ===========================================================================

TEST_F (DfciRecoveryLibTest, GetChallenge_SerialIdFailure_DocSaysIgnored_ButCodePropagates) {
  //
  // The docstring says identifier failures aren't a big deal ("isn't a
  // big issue if the identification doesn't get added to the recovery
  // packet"), but the actual code returns whatever Status the last
  // DfciIdSupport call produced. This test pins that real behavior so
  // any future change to either the docstring or the code is forced
  // to update this test deliberately.
  //
  DFCI_RECOVERY_CHALLENGE  *Challenge = NULL;
  UINTN                    ChallengeSize = 0;
  EFI_STATUS               Status;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .WillOnce (
       DoAll (
         SetArgBuffer<0>(&HappyTime, sizeof (HappyTime)),
         Return (EFI_SUCCESS)
         )
       );
  ExpectLocateRng ();
  ExpectGetRngSuccess (0x10, DFCI_RECOVERY_NONCE_SIZE);

  EXPECT_CALL (IdMock, DfciIdSupportGetSerialNumber (_, _))
    .WillOnce (Return (EFI_NOT_FOUND));
  // Product / Manufacturer must NOT be invoked once the chain breaks.

  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);
  EXPECT_EQ (Status, EFI_NOT_FOUND);
  // ChallengeSize and Challenge are still set in this path -
  // documented quirk; pin it so any rewrite is intentional.
  EXPECT_NE (Challenge, nullptr);
  if (Challenge != NULL) {
    FreePool (Challenge);
  }
}

TEST_F (DfciRecoveryLibTest, GetChallenge_NonceDiffersAcrossTwoCalls) {
  //
  // Two consecutive GetRecoveryChallenge calls must populate the
  // Nonce field with different bytes. We use a pair of EXPECT_CALL
  // invocations on RngMock with different fill-bytes; each
  // GetRecoveryChallenge call invokes LocateProtocol once.
  //
  DFCI_RECOVERY_CHALLENGE  *First  = NULL;
  DFCI_RECOVERY_CHALLENGE  *Second = NULL;
  UINTN                    Size1   = 0;
  UINTN                    Size2   = 0;

  EXPECT_CALL (RtMock, gRT_GetTime (_, _))
    .Times (2)
    .WillRepeatedly (
       DoAll (
         SetArgBuffer<0>(&HappyTime, sizeof (HappyTime)),
         Return (EFI_SUCCESS)
         )
       );
  ExpectLocateRng (2);
  EXPECT_CALL (RngMock, GetRng (_, _, DFCI_RECOVERY_NONCE_SIZE, _))
    .WillOnce (
       Invoke (
         [](EFI_RNG_PROTOCOL *, EFI_RNG_ALGORITHM *, UINTN Len, UINT8 *Out) -> EFI_STATUS {
           SetMem (Out, Len, 0xAA);
           return EFI_SUCCESS;
         }
         )
       )
    .WillOnce (
       Invoke (
         [](EFI_RNG_PROTOCOL *, EFI_RNG_ALGORITHM *, UINTN Len, UINT8 *Out) -> EFI_STATUS {
           SetMem (Out, Len, 0x55);
           return EFI_SUCCESS;
         }
         )
       );
  EXPECT_CALL (IdMock, DfciIdSupportGetSerialNumber (_, _))
    .Times (2)
    .WillRepeatedly (Invoke ([](CHAR8 **Out, UINTN *Size) { return CloneAsciiToHeap ("SN", Out, Size); }));
  EXPECT_CALL (IdMock, DfciIdSupportGetProductName (_, _))
    .Times (2)
    .WillRepeatedly (Invoke ([](CHAR8 **Out, UINTN *Size) { return CloneAsciiToHeap ("P", Out, Size); }));
  EXPECT_CALL (IdMock, DfciIdSupportGetManufacturer (_, _))
    .Times (2)
    .WillRepeatedly (Invoke ([](CHAR8 **Out, UINTN *Size) { return CloneAsciiToHeap ("M", Out, Size); }));

  ASSERT_EQ (GetRecoveryChallenge (&First,  &Size1), EFI_SUCCESS);
  ASSERT_EQ (GetRecoveryChallenge (&Second, &Size2), EFI_SUCCESS);

  // First call's nonce should be all 0xAA, second all 0x55.
  EXPECT_EQ (First->Nonce.Bytes[0],  0xAA);
  EXPECT_EQ (Second->Nonce.Bytes[0], 0x55);
  EXPECT_NE (
    CompareMem (
      &First->Nonce.Bytes[0],
      &Second->Nonce.Bytes[0],
      DFCI_RECOVERY_NONCE_SIZE
      ),
    0
    );

  FreePool (First);
  FreePool (Second);
}

// ===========================================================================
// EncryptRecoveryChallenge - parameter validation
// ===========================================================================

TEST_F (DfciRecoveryLibTest, Encrypt_NullChallengeRejected) {
  UINT8       PublicKey[8] = { 0 };
  UINT8       *Out         = (UINT8 *)0x1;
  UINTN       OutSize      = 0xDEAD;
  EFI_STATUS  Status;

  Status = EncryptRecoveryChallenge (NULL, 0, PublicKey, sizeof (PublicKey), &Out, &OutSize);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (DfciRecoveryLibTest, Encrypt_NullPublicKeyRejected) {
  DFCI_RECOVERY_CHALLENGE  Challenge = { 0 };
  UINT8                    *Out      = (UINT8 *)0x1;
  UINTN                    OutSize   = 0xDEAD;
  EFI_STATUS               Status;

  Status = EncryptRecoveryChallenge (&Challenge, sizeof (Challenge), NULL, 0, &Out, &OutSize);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (DfciRecoveryLibTest, Encrypt_NullEncryptedDataPtrRejected) {
  DFCI_RECOVERY_CHALLENGE  Challenge       = { 0 };
  UINT8                    PublicKey[8]    = { 0 };
  UINTN                    OutSize         = 0xDEAD;
  EFI_STATUS               Status;

  Status = EncryptRecoveryChallenge (&Challenge, sizeof (Challenge), PublicKey, sizeof (PublicKey), NULL, &OutSize);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (DfciRecoveryLibTest, Encrypt_NullEncryptedDataSizeRejected) {
  DFCI_RECOVERY_CHALLENGE  Challenge    = { 0 };
  UINT8                    PublicKey[8] = { 0 };
  UINT8                    *Out         = (UINT8 *)0x1;
  EFI_STATUS               Status;

  Status = EncryptRecoveryChallenge (&Challenge, sizeof (Challenge), PublicKey, sizeof (PublicKey), &Out, NULL);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

// ===========================================================================
// EncryptRecoveryChallenge - happy path / failure propagation
// ===========================================================================

TEST_F (DfciRecoveryLibTest, Encrypt_HappyPath_PassesPlaintextAndKeyToCrypto) {
  DFCI_RECOVERY_CHALLENGE  Challenge    = { 0 };
  UINT8                    PublicKey[]  = { 0xAA, 0xBB, 0xCC };
  UINT8                    *Out         = NULL;
  UINTN                    OutSize      = 0;
  EFI_STATUS               Status;

  // Capture buffers - must persist past the EXPECT_CALL.
  static UINT8  CapturedInData[256] = { 0 };
  static UINTN  CapturedInDataSize  = 0;
  static UINTN  CapturedKeySize     = 0;
  CapturedInDataSize = 0;
  CapturedKeySize    = 0;

  ExpectLocateRng ();
  ExpectGetRngSuccess (0x42, /*RANDOM_SEED_BUFFER_SIZE = */ 64);

  EXPECT_CALL (CryptMock, Pkcs1v2Encrypt (_, _, _, _, _, _, _, _))
    .WillOnce (
       Invoke (
         [](
           CONST UINT8  *,
           UINTN        KeyLen,
           UINT8        *InData,
           UINTN        InDataSize,
           CONST UINT8  *,
           UINTN        ,
           UINT8        **EncOut,
           UINTN        *EncOutSize
           ) -> BOOLEAN {
           if (InDataSize <= sizeof (CapturedInData)) {
             CopyMem (CapturedInData, InData, InDataSize);
           }

           CapturedInDataSize = InDataSize;
           CapturedKeySize    = KeyLen;

           *EncOut     = (UINT8 *)AllocatePool (16);
           SetMem (*EncOut, 16, 0xCD);
           *EncOutSize = 16;
           return TRUE;
         }
         )
       );

  Challenge.SerialNumber = 0;
  Status = EncryptRecoveryChallenge (
             &Challenge,
             sizeof (Challenge),
             PublicKey,
             sizeof (PublicKey),
             &Out,
             &OutSize
             );
  ASSERT_EQ (Status, EFI_SUCCESS);
  ASSERT_NE (Out, nullptr);
  EXPECT_EQ (OutSize, (UINTN)16);

  // The CUT must pass our exact key size and challenge bytes through.
  EXPECT_EQ (CapturedKeySize,    sizeof (PublicKey));
  EXPECT_EQ (CapturedInDataSize, sizeof (Challenge));
  EXPECT_EQ (
    CompareMem (CapturedInData, &Challenge, sizeof (Challenge)),
    0
    );

  FreePool (Out);
}

TEST_F (DfciRecoveryLibTest, Encrypt_RngSeedFailureIsPropagated) {
  DFCI_RECOVERY_CHALLENGE  Challenge    = { 0 };
  UINT8                    PublicKey[8] = { 0 };
  UINT8                    *Out         = (UINT8 *)0x1;
  UINTN                    OutSize      = 0xDEAD;
  EFI_STATUS               Status;

  ExpectLocateRng ();
  EXPECT_CALL (RngMock, GetRng (_, _, _, _))
    .WillOnce (Return (EFI_DEVICE_ERROR));
  // Pkcs1v2Encrypt must NOT be invoked once seed gathering fails.

  Status = EncryptRecoveryChallenge (
             &Challenge,
             sizeof (Challenge),
             PublicKey,
             sizeof (PublicKey),
             &Out,
             &OutSize
             );
  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
  EXPECT_EQ (Out,    nullptr);
  EXPECT_EQ (OutSize, (UINTN)0);
}

TEST_F (DfciRecoveryLibTest, Encrypt_Pkcs1v2EncryptReturnsFalse_MapsToAborted) {
  DFCI_RECOVERY_CHALLENGE  Challenge    = { 0 };
  UINT8                    PublicKey[8] = { 0 };
  UINT8                    *Out         = (UINT8 *)0x1;
  UINTN                    OutSize      = 0xDEAD;
  EFI_STATUS               Status;

  ExpectLocateRng ();
  ExpectGetRngSuccess (0x10, 64);
  EXPECT_CALL (CryptMock, Pkcs1v2Encrypt (_, _, _, _, _, _, _, _))
    .WillOnce (Return ((BOOLEAN)FALSE));

  Status = EncryptRecoveryChallenge (
             &Challenge,
             sizeof (Challenge),
             PublicKey,
             sizeof (PublicKey),
             &Out,
             &OutSize
             );
  EXPECT_EQ (Status, EFI_ABORTED);
  EXPECT_EQ (Out,    nullptr);
  EXPECT_EQ (OutSize, (UINTN)0);
}

// ===========================================================================
// Header / contract tests (no mocks needed)
// ===========================================================================

class DfciRecoveryLibContractTest : public ::testing::Test {};

TEST_F (DfciRecoveryLibContractTest, NonceSizeIs512Bits) {
  EXPECT_EQ (DFCI_RECOVERY_NONCE_SIZE, (UINTN)(512 / 8));
}

TEST_F (DfciRecoveryLibContractTest, NonceKeySizeIs10Bytes) {
  EXPECT_EQ (DFCI_RECOVERY_NONCE_KEY_SIZE, (UINTN)10);
}

TEST_F (DfciRecoveryLibContractTest, MultiStringMaxIs104Bytes) {
  EXPECT_EQ (DFCI_MULTI_STRING_MAX_SIZE, (UINTN)104);
}

TEST_F (DfciRecoveryLibContractTest, NonceUnionIsExactly512Bits) {
  EXPECT_EQ (sizeof (DFCI_CHALLENGE_NONCE), (UINTN)DFCI_RECOVERY_NONCE_SIZE);
  EXPECT_EQ (
    sizeof (((DFCI_CHALLENGE_NONCE *)0)->Parts.Nonce) +
    sizeof (((DFCI_CHALLENGE_NONCE *)0)->Parts.Key),
    (UINTN)DFCI_RECOVERY_NONCE_SIZE
    );
}

TEST_F (DfciRecoveryLibContractTest, NonceUnionAliasesBytesAndParts) {
  // Writing through Bytes[] must be visible through Parts.Nonce[] and
  // Parts.Key[] at the right offsets, proving the union layout.
  DFCI_CHALLENGE_NONCE  N;
  for (UINTN I = 0; I < DFCI_RECOVERY_NONCE_SIZE; I++) {
    N.Bytes[I] = (UINT8)I;
  }

  for (UINTN I = 0; I < (DFCI_RECOVERY_NONCE_SIZE - DFCI_RECOVERY_NONCE_KEY_SIZE); I++) {
    EXPECT_EQ (N.Parts.Nonce[I], (UINT8)I);
  }

  for (UINTN I = 0; I < DFCI_RECOVERY_NONCE_KEY_SIZE; I++) {
    EXPECT_EQ (
      N.Parts.Key[I],
      (UINT8)(I + DFCI_RECOVERY_NONCE_SIZE - DFCI_RECOVERY_NONCE_KEY_SIZE)
      );
  }
}

// ===========================================================================
// Test entry point
// ===========================================================================

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
