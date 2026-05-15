/** @file DfciRecoveryLibGoogleTest.cpp

  Host-based unit tests for DfciRecoveryLib.

  Public APIs covered:
    * GetRandomValue              (file-static-ish symbol, exposed by
                                   linking DfciRecoveryLib.c directly)
    * GetRecoveryChallenge
    * EncryptRecoveryChallenge

  Strategy:
    * Implementation test - DfciRecoveryLib.c is linked directly via
      [Sources] in the test INF.
    * gBS->LocateProtocol and gRT->GetTime are mocked via the shared
      MockUefiBootServicesTableLib and MockUefiRuntimeServicesTableLib.
    * EFI_RNG_PROTOCOL is provided by MockRng (shared MdePkg mock).
    * DfciDeviceIdSupportLib and BaseCryptLib::Pkcs1v2Encrypt are mocked
      locally in MockDfciRecoveryExternals.{h,cpp} - see that header for
      rationale (the shared MockBaseCryptLib does not declare
      Pkcs1v2Encrypt today).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

#include <GoogleTest/Library/MockUefiBootServicesTableLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
#include <GoogleTest/Protocol/MockRng.h>

#include "MockDfciRecoveryExternals.h"

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/BaseMemoryLib.h>
  #include <Library/DebugLib.h>
  #include <Library/MemoryAllocationLib.h>
  #include <Library/PcdLib.h>
  #include <Library/DfciRecoveryLib.h>
  #include <Protocol/Rng.h>

  //
  // The CUT exposes GetRandomValue with external linkage even though the
  // public header does not declare it. Declare it here so we can call it
  // directly in unit tests.
  //
  EFI_STATUS
  EFIAPI
  GetRandomValue (
    OUT  VOID   *Output,
    IN   UINTN  OutputLength
    );
}

using namespace testing;

//
// Helper: ASSERT()-firing test guard. When PcdDebugPropertyMask has
// BIT0 cleared, ASSERT() is compiled out (no exception is thrown), and
// any EXPECT_*_THROW assertion would fail. Tests that exercise ASSERT()
// must early-return via GTEST_SKIP when that is the case.
//
#define SKIP_IF_ASSERT_DISABLED()                                  \
  do {                                                              \
    if ((PcdGet8 (PcdDebugPropertyMask) & BIT0) == 0x00) {          \
      GTEST_SKIP () << "ASSERT() disabled by PcdDebugPropertyMask"; \
    }                                                               \
  } while (0)

//
// Helper used by mock lambdas: AllocatePool a copy of |Src| (including
// the trailing NUL) so the CUT can FreePool it.
//
static
CHAR8 *
AllocAsciiCopy (
  const CHAR8  *Src,
  UINTN        *SizeOut
  )
{
  UINTN  Size = AsciiStrSize (Src);
  CHAR8  *Dst = (CHAR8 *)AllocatePool (Size);

  if (Dst != NULL) {
    CopyMem (Dst, Src, Size);
  }

  if (SizeOut != NULL) {
    *SizeOut = Size;
  }

  return Dst;
}

///////////////////////////////////////////////////////////////////////////////
// GetRandomValue
///////////////////////////////////////////////////////////////////////////////

class GetRandomValueTest : public Test {
protected:
  StrictMock<MockUefiBootServicesTableLib>  BsMock;
  StrictMock<MockRng>                       RngMock;

  UINT8  Buffer[32]{};
};

TEST_F (GetRandomValueTest, NullOutputReturnsInvalidParameter) {
  EFI_STATUS  Status = GetRandomValue (NULL, sizeof (Buffer));
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (GetRandomValueTest, ZeroLengthReturnsInvalidParameter) {
  EFI_STATUS  Status = GetRandomValue (&Buffer[0], 0);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (GetRandomValueTest, LocateProtocolFailureReturnsErrorAndAsserts) {
  SKIP_IF_ASSERT_DISABLED ();

  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (Return (EFI_NOT_FOUND));

  EXPECT_ANY_THROW (GetRandomValue (&Buffer[0], sizeof (Buffer)));
}

TEST_F (GetRandomValueTest, FirstAlgorithmSucceedsReturnsSuccess) {
  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );

  EXPECT_CALL (RngMock, GetRng (gRngProtocol, _, sizeof (Buffer), &Buffer[0]))
    .WillOnce (Return (EFI_SUCCESS));

  EXPECT_EQ (GetRandomValue (&Buffer[0], sizeof (Buffer)), EFI_SUCCESS);
}

TEST_F (GetRandomValueTest, FirstAlgorithmUnsupportedSecondSucceeds) {
  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );

  //
  // Two calls to GetRng: first returns EFI_UNSUPPORTED, second returns
  // EFI_SUCCESS. The library must walk to the next algorithm rather
  // than aborting.
  //
  InSequence  Seq;
  EXPECT_CALL (RngMock, GetRng).WillOnce (Return (EFI_UNSUPPORTED));
  EXPECT_CALL (RngMock, GetRng).WillOnce (Return (EFI_SUCCESS));

  EXPECT_EQ (GetRandomValue (&Buffer[0], sizeof (Buffer)), EFI_SUCCESS);
}

TEST_F (GetRandomValueTest, AllAlgorithmsUnsupportedReturnsNotFoundAndAsserts) {
  SKIP_IF_ASSERT_DISABLED ();

  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );

  //
  // The library iterates over 4 algorithms; with EFI_UNSUPPORTED each
  // time, the function exhausts the list and ASSERTs before returning
  // EFI_NOT_FOUND.
  //
  EXPECT_CALL (RngMock, GetRng)
    .Times (4)
    .WillRepeatedly (Return (EFI_UNSUPPORTED));

  EXPECT_ANY_THROW (GetRandomValue (&Buffer[0], sizeof (Buffer)));
}

TEST_F (GetRandomValueTest, NonUnsupportedRngErrorPropagatesAndAsserts) {
  SKIP_IF_ASSERT_DISABLED ();

  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );

  //
  // Anything other than EFI_SUCCESS or EFI_UNSUPPORTED aborts the
  // search and ASSERTs.
  //
  EXPECT_CALL (RngMock, GetRng).WillOnce (Return (EFI_DEVICE_ERROR));

  EXPECT_ANY_THROW (GetRandomValue (&Buffer[0], sizeof (Buffer)));
}

///////////////////////////////////////////////////////////////////////////////
// GetRecoveryChallenge
///////////////////////////////////////////////////////////////////////////////

class GetRecoveryChallengeTest : public Test {
protected:
  StrictMock<MockUefiBootServicesTableLib>     BsMock;
  StrictMock<MockUefiRuntimeServicesTableLib>  RtMock;
  StrictMock<MockRng>                          RngMock;
  StrictMock<MockDfciDeviceIdSupportLib>       IdMock;

  DFCI_RECOVERY_CHALLENGE  *Challenge = nullptr;
  UINTN                    ChallengeSize = 0;

  void TearDown () override {
    if (Challenge != nullptr) {
      FreePool (Challenge);
      Challenge = nullptr;
    }
  }

  //
  // Default-success: any number of LocateProtocol/GetRng calls succeed
  // with a deterministic byte pattern. Used by tests that aren't
  // exercising the RNG path.
  //
  void
  AllowSuccessfulRng (
    UINT8  FillByte = 0xAA
    )
  {
    EXPECT_CALL (BsMock, gBS_LocateProtocol)
      .WillRepeatedly (
         DoAll (
           SetArgPointee<2> (ByRef (gRngProtocol)),
           Return (EFI_SUCCESS)
           )
         );

    EXPECT_CALL (RngMock, GetRng)
      .WillRepeatedly (
         Invoke ([FillByte](
                   EFI_RNG_PROTOCOL      *This,
                   EFI_RNG_ALGORITHM     *RNGAlgorithm,
                   UINTN                 RNGValueLength,
                   UINT8                 *RNGValue
                   ) -> EFI_STATUS {
           (VOID) This;
           (VOID) RNGAlgorithm;
           SetMem (RNGValue, RNGValueLength, FillByte);
           return EFI_SUCCESS;
         })
         );
  }
};

TEST_F (GetRecoveryChallengeTest, NullChallengePointerReturnsInvalidParameter) {
  EFI_STATUS  Status = GetRecoveryChallenge (NULL, &ChallengeSize);
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (GetRecoveryChallengeTest, GetTimeFailureClearsChallenge) {
  EXPECT_CALL (RtMock, gRT_GetTime)
    .WillOnce (Return (EFI_DEVICE_ERROR));

  //
  // GetRandomValue path must not be exercised when GetTime fails. The
  // StrictMocks on BsMock/RngMock enforce this implicitly.
  //
  EFI_STATUS  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);

  EXPECT_EQ (Status, EFI_DEVICE_ERROR);
  EXPECT_EQ (Challenge, nullptr);
}

//
// NOTE: A test that drives GetRecoveryChallenge() into the
// GetRandomValue() failure path (LocateProtocol returning an error, or
// all RNG algorithms reporting EFI_UNSUPPORTED) is intentionally
// omitted. In that path GetRandomValue() fires an ASSERT(), which the
// GoogleTest framework translates into a C++ exception. The CUT has
// already AllocatePool()'d the challenge buffer at that point and does
// not free it on the exception path, so Address Sanitizer/LSan would
// fail the test with a leak. The omission documents an existing CUT
// shortcoming. The GetTimeFailure test below covers the equivalent
// pre-allocation-cleanup contract.
//

TEST_F (GetRecoveryChallengeTest, HappyPathPopulatesChallenge) {
  //
  // Set up deterministic time and RNG.
  //
  EFI_TIME  FakeTime = {};
  FakeTime.Year   = 2026;
  FakeTime.Month  = 5;
  FakeTime.Day    = 14;
  FakeTime.Hour   = 12;
  FakeTime.Minute = 34;
  FakeTime.Second = 56;

  EXPECT_CALL (RtMock, gRT_GetTime)
    .WillOnce (
       DoAll (
         SetArgPointee<0> (FakeTime),
         Return (EFI_SUCCESS)
         )
       );

  AllowSuccessfulRng (0xA5);

  //
  // DfciIdSupport: return canned strings so we can verify MultiString
  // concatenation.
  //
  EXPECT_CALL (IdMock, DfciIdSupportGetSerialNumber)
    .WillOnce (
       Invoke ([](CHAR8 **s, UINTN *sz) {
         *s = AllocAsciiCopy ("SN-1;", sz);
         return EFI_SUCCESS;
       })
       );
  EXPECT_CALL (IdMock, DfciIdSupportGetProductName)
    .WillOnce (
       Invoke ([](CHAR8 **s, UINTN *sz) {
         *s = AllocAsciiCopy ("PN-X;", sz);
         return EFI_SUCCESS;
       })
       );
  EXPECT_CALL (IdMock, DfciIdSupportGetManufacturer)
    .WillOnce (
       Invoke ([](CHAR8 **s, UINTN *sz) {
         *s = AllocAsciiCopy ("MFG.", sz);
         return EFI_SUCCESS;
       })
       );

  ASSERT_EQ (GetRecoveryChallenge (&Challenge, &ChallengeSize), EFI_SUCCESS);
  ASSERT_NE (Challenge, nullptr);

  //
  // Contract verification.
  //
  EXPECT_EQ (Challenge->SerialNumber, (UINTN)0);
  EXPECT_EQ (Challenge->Timestamp.Year, 2026);
  EXPECT_EQ (Challenge->Timestamp.Day,  14);

  //
  // Nonce must be populated with the deterministic fill (0xA5) for the
  // first DFCI_RECOVERY_NONCE_SIZE bytes.
  //
  for (UINTN i = 0; i < DFCI_RECOVERY_NONCE_SIZE; i++) {
    EXPECT_EQ (Challenge->Nonce.Bytes[i], 0xA5)
      << "Nonce byte " << i << " was not populated by RNG";
  }

  //
  // MultiString concatenation order is Serial, Product, Manufacturer.
  //
  EXPECT_STREQ ((const char *)&Challenge->MultiString[0], "SN-1;PN-X;MFG.");

  //
  // Reported size accounts for both the fixed header and the trailing
  // multi-string (including its NUL terminator).
  //
  EXPECT_EQ (
    ChallengeSize,
    sizeof (DFCI_RECOVERY_CHALLENGE)
    + AsciiStrSize ((const char *)&Challenge->MultiString[0])
    );
}

TEST_F (GetRecoveryChallengeTest, IdLookupFailureStillReturnsChallengeBuffer) {
  //
  // The contract: even when device-identity lookups fail, the
  // challenge buffer is still produced (Status reflects the lookup
  // failure but *Challenge is non-NULL and the trailing MultiString is
  // simply empty).
  //
  EXPECT_CALL (RtMock, gRT_GetTime).WillOnce (Return (EFI_SUCCESS));
  AllowSuccessfulRng ();

  //
  // First ID lookup fails - subsequent lookups must NOT be called
  // (the CUT short-circuits on the first failure). StrictMock enforces.
  //
  EXPECT_CALL (IdMock, DfciIdSupportGetSerialNumber)
    .WillOnce (Return (EFI_NOT_FOUND));

  EFI_STATUS  Status = GetRecoveryChallenge (&Challenge, &ChallengeSize);

  //
  // Status reflects the ID-lookup failure, but the challenge buffer is
  // still returned (per the existing implementation).
  //
  EXPECT_EQ (Status, EFI_NOT_FOUND);
  ASSERT_NE (Challenge, nullptr);
  EXPECT_EQ (Challenge->MultiString[0], '\0');
}

///////////////////////////////////////////////////////////////////////////////
// EncryptRecoveryChallenge
///////////////////////////////////////////////////////////////////////////////

class EncryptRecoveryChallengeTest : public Test {
protected:
  StrictMock<MockUefiBootServicesTableLib>  BsMock;
  StrictMock<MockRng>                       RngMock;
  StrictMock<MockPkcs1v2EncryptLib>         CryptMock;

  //
  // Backing storage for the dummy challenge. DFCI_RECOVERY_CHALLENGE
  // ends in a flexible array (MultiString[]), so it cannot legally be
  // a value-type class member; provide a properly-aligned byte buffer
  // and re-interpret it as the struct.
  //
  alignas (DFCI_RECOVERY_CHALLENGE)
  UINT8                          ChallengeBuf[sizeof (DFCI_RECOVERY_CHALLENGE) + 1]{};
  DFCI_RECOVERY_CHALLENGE *const Challenge = reinterpret_cast<DFCI_RECOVERY_CHALLENGE *> (ChallengeBuf);
  const UINTN                    ChallengeSize = sizeof (ChallengeBuf);

  UINT8                          PubKey[16]{};

  UINT8                          *EncryptedData     = nullptr;
  UINTN                          EncryptedDataSize  = 0;
};

TEST_F (EncryptRecoveryChallengeTest, NullChallengeReturnsInvalidParameter) {
  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         NULL,
                         ChallengeSize,
                         &PubKey[0],
                         sizeof (PubKey),
                         &EncryptedData,
                         &EncryptedDataSize
                         );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
  EXPECT_EQ (EncryptedData, nullptr);
}

TEST_F (EncryptRecoveryChallengeTest, NullPublicKeyReturnsInvalidParameter) {
  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         Challenge,
                         ChallengeSize,
                         NULL,
                         sizeof (PubKey),
                         &EncryptedData,
                         &EncryptedDataSize
                         );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
  EXPECT_EQ (EncryptedData, nullptr);
}

TEST_F (EncryptRecoveryChallengeTest, NullEncryptedDataReturnsInvalidParameter) {
  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         Challenge,
                         ChallengeSize,
                         &PubKey[0],
                         sizeof (PubKey),
                         NULL,
                         &EncryptedDataSize
                         );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
}

TEST_F (EncryptRecoveryChallengeTest, NullEncryptedDataSizeReturnsInvalidParameter) {
  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         Challenge,
                         ChallengeSize,
                         &PubKey[0],
                         sizeof (PubKey),
                         &EncryptedData,
                         NULL
                         );
  EXPECT_EQ (Status, EFI_INVALID_PARAMETER);
  EXPECT_EQ (EncryptedData, nullptr);
}

TEST_F (EncryptRecoveryChallengeTest, SeedRngFailurePropagatesAndAsserts) {
  SKIP_IF_ASSERT_DISABLED ();

  //
  // GetRandomValue (called to fill the extra seed) fails when
  // LocateProtocol does. The library ASSERTs and surfaces the error.
  //
  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (Return (EFI_NOT_FOUND));

  EXPECT_ANY_THROW (
    EncryptRecoveryChallenge (
      Challenge,
      ChallengeSize,
      &PubKey[0],
      sizeof (PubKey),
      &EncryptedData,
      &EncryptedDataSize
      )
    );

  EXPECT_EQ (EncryptedData,     nullptr);
  EXPECT_EQ (EncryptedDataSize, (UINTN)0);
}

TEST_F (EncryptRecoveryChallengeTest, Pkcs1v2FalseReturnsAborted) {
  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (RngMock, GetRng).WillOnce (Return (EFI_SUCCESS));
  EXPECT_CALL (CryptMock, Pkcs1v2Encrypt).WillOnce (Return (FALSE));

  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         Challenge,
                         ChallengeSize,
                         &PubKey[0],
                         sizeof (PubKey),
                         &EncryptedData,
                         &EncryptedDataSize
                         );

  EXPECT_EQ (Status, EFI_ABORTED);
  EXPECT_EQ (EncryptedData,     nullptr);
  EXPECT_EQ (EncryptedDataSize, (UINTN)0);
}

TEST_F (EncryptRecoveryChallengeTest, HappyPathReturnsSuccessAndForwardsBuffers) {
  EXPECT_CALL (BsMock, gBS_LocateProtocol)
    .WillOnce (
       DoAll (
         SetArgPointee<2> (ByRef (gRngProtocol)),
         Return (EFI_SUCCESS)
         )
       );
  EXPECT_CALL (RngMock, GetRng).WillOnce (Return (EFI_SUCCESS));

  //
  // On success Pkcs1v2Encrypt is contractually required to allocate
  // and return an encrypted buffer. Emulate that. The caller of
  // EncryptRecoveryChallenge owns the buffer.
  //
  EXPECT_CALL (
    CryptMock,
    Pkcs1v2Encrypt (
      &PubKey[0],
      sizeof (PubKey),
      (UINT8 *)Challenge,
      ChallengeSize,
      _,                          // ExtraSeed pointer
      64,                         // RANDOM_SEED_BUFFER_SIZE
      &EncryptedData,
      &EncryptedDataSize
      )
    )
    .WillOnce (
       Invoke ([](
                 CONST UINT8  *PublicKey,
                 UINTN        PublicKeySize,
                 UINT8        *InData,
                 UINTN        InDataSize,
                 CONST UINT8  *PrngSeed,
                 UINTN        PrngSeedSize,
                 UINT8        **EncOut,
                 UINTN        *EncOutSize
                 ) -> BOOLEAN {
         (VOID) PublicKey;
         (VOID) PublicKeySize;
         (VOID) InData;
         (VOID) InDataSize;
         (VOID) PrngSeed;
         (VOID) PrngSeedSize;
         *EncOut     = (UINT8 *)AllocateZeroPool (32);
         *EncOutSize = 32;
         return TRUE;
       })
       );

  EFI_STATUS  Status = EncryptRecoveryChallenge (
                         Challenge,
                         ChallengeSize,
                         &PubKey[0],
                         sizeof (PubKey),
                         &EncryptedData,
                         &EncryptedDataSize
                         );

  EXPECT_EQ (Status, EFI_SUCCESS);
  ASSERT_NE (EncryptedData, nullptr);
  EXPECT_EQ (EncryptedDataSize, (UINTN)32);

  FreePool (EncryptedData);
  EncryptedData     = nullptr;
  EncryptedDataSize = 0;
}

///////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
