/** @file
DfciRecoveryLib.c

This library contains crypto support functions for the DFCI
recovery feature.

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DfciRecoveryLib.h>
#include <Library/MemoryAllocationLib.h>            // AllocatePool
#include <Library/UefiBootServicesTableLib.h>       // gBS
#include <Library/UefiRuntimeServicesTableLib.h>    // gRT
#include <Library/BaseCryptLib.h>                   // Pkcs1v2Encrypt
#include <Library/DfciDeviceIdSupportLib.h>         // Provide device info for recovery operator

#include <Protocol/Rng.h>               // EFI_RNG_PROTOCOL

#define   RANDOM_SEED_BUFFER_SIZE  64   // We'll seed with 64 bytes of random so that OAEP can work its best.

//
// These represent UEFI SPEC defined algorithms that should be supported by
// the RNG protocol and are generally considered secure for DFCI purposes.
//
static EFI_GUID *CONST  mSecureRngAlgorithms[] = {
  &gEfiRngAlgorithmSp80090Ctr256Guid,  // SP800-90A DRBG CTR using AES-256
  &gEfiRngAlgorithmSp80090Hmac256Guid, // SP800-90A DRBG HMAC using SHA-256
  &gEfiRngAlgorithmSp80090Hash256Guid, // SP800-90A DRBG Hash using SHA-256
  &gEfiRngAlgorithmArmRndr,            // unspecified SP800-90B DRBG via ARM RNDR register
};

#define SECURE_RNG_ALGORITHMS_SIZE  (ARRAY_SIZE (mSecureRngAlgorithms))

/**
  Generate a random output of a given length using an algorithm considered secure.

  @param[out] Output        The buffer to store the generated random data.
  @param[in]  OutputLength  The length of the output buffer.

  @retval EFI_SUCCESS           The random data was generated successfully.
  @retval EFI_INVALID_PARAMETER The output buffer is NULL or the output length is zero.
  @retval EFI_NOT_FOUND         RNG protocol not found or a secure algorithm is not supported.
  @retval Others                An error occurred while generating random data.
**/
EFI_STATUS
EFIAPI
GetRandomValue (
  OUT  VOID   *Output,
  IN   UINTN  OutputLength
  )
{
  EFI_RNG_PROTOCOL  *RngProtocol;
  EFI_STATUS        Status;
  UINTN             AlgorithmIndex;

  if ((Output == NULL) || (OutputLength == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gEfiRngProtocolGuid, NULL, (VOID **)&RngProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate EFI_RNG_PROTOCOL: %r\n", __func__, Status));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  for (AlgorithmIndex = 0; AlgorithmIndex < SECURE_RNG_ALGORITHMS_SIZE; AlgorithmIndex++) {
    Status = RngProtocol->GetRNG (RngProtocol, mSecureRngAlgorithms[AlgorithmIndex], OutputLength, (UINT8 *)Output);
    if (!EFI_ERROR (Status)) {
      //
      // The secure algorithm is supported on this platform
      //
      return EFI_SUCCESS;
    } else if (Status == EFI_UNSUPPORTED) {
      //
      // The secure algorithm is not supported on this platform
      //
      DEBUG ((DEBUG_VERBOSE, "%a: Failed to generate random data using secure algorithm %d: %r\n", __func__, AlgorithmIndex, Status));

      //
      // Try the next secure algorithm
      //
      continue;
    } else {
      //
      // Some other error occurred
      //
      DEBUG ((DEBUG_ERROR, "%a: Failed to generate random data using secure algorithm %d: %r\n", __func__, AlgorithmIndex, Status));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }
  }

  //
  // If we get here, we failed to generate random data using any secure algorithm
  // Platform owner should ensure that at least one secure algorithm is supported
  //
  DEBUG ((DEBUG_ERROR, "%a: Failed to generate random data, no supported secure algorithm found\n", __func__));
  ASSERT_EFI_ERROR (Status);

  return EFI_NOT_FOUND;
}

/**
  This function will attempt to allocate and populate a buffer
  with a DFCI recovery challenge structure. If unsuccessful,
  will return an error and set pointer to NULL.

  @param[out] Challenge     Allocated buffer containing recovery challenge. NULL on error.
  @param[out] ChallengeSize Pointer to UINTN to receive the Size of Challenge object.

  @retval     EFI_SUCCESS           Challenge was successfully created and can be found in buffer.
  @retval     EFI_INVALID_PARAMETER Nuff said.
  @retval     EFI_NOT_FOUND         Could not locate a required protocol or resource.
  @retval     EFI_OUT_OF_RESOURCES  Could not allocate a buffer for the challenge.
  @retval     Others                Error returned from LocateProtocol, GetVariable, GetTime, or GetRNG.

**/
EFI_STATUS
GetRecoveryChallenge (
  OUT DFCI_RECOVERY_CHALLENGE  **Challenge,
  OUT UINTN                    *ChallengeSize
  )
{
  EFI_STATUS               Status;
  DFCI_RECOVERY_CHALLENGE  *NewChallenge;
  CHAR8                    *Element;
  UINTN                    ElementSize;

  DEBUG ((DEBUG_INFO, "%a()\n", __FUNCTION__));

  //
  // Check inputs...
  if (Challenge == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: NULL pointer provided!\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Set default state...
  *Challenge   = NULL;
  NewChallenge = NULL;

  //
  // From now on, don't proceed on errors.
  //

  //
  // Allocate the buffer...
  NewChallenge = AllocatePool (sizeof (DFCI_RECOVERY_CHALLENGE) + DFCI_MULTI_STRING_MAX_SIZE);

  // Exit if we ran out of resources
  if (NewChallenge == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Grab the system identifier
  //
  // The challenge serial number is now a string.  The UINTN V1 serial number is now deprecated,
  // with the string version of the serial number located in the multi-string field of NewChallenge.
  //
  NewChallenge->SerialNumber = 0;

  //
  // Grab a timestamp...
  Status = gRT->GetTime (&NewChallenge->Timestamp, NULL);
  DEBUG ((DEBUG_VERBOSE, "%a: GetTime() = %r\n", __FUNCTION__, Status));

  //
  // Generate the random nonce...
  if (!EFI_ERROR (Status)) {
    Status = GetRandomValue (&NewChallenge->Nonce.Bytes[0], DFCI_RECOVERY_NONCE_SIZE);
  }

  //
  // If we're still good, set the pointer...
  if (!EFI_ERROR (Status)) {
    *Challenge = NewChallenge;
  }

  //
  // Always put away your toys...
  // If there was an error, but the buffer was allocated, free it.
  if (EFI_ERROR (Status)) {
    if (NULL != NewChallenge) {
      FreePool (NewChallenge);
    }

    NewChallenge = NULL;
  }

  if (NewChallenge != NULL) {
    //
    // There is only room for about 100 characters of identifier.  This should be enough to identify
    // the system that is being recovered.
    //
    NewChallenge->MultiString[0] = '\0';
    Status                       = DfciIdSupportGetSerialNumber (&Element, &ElementSize);
    if (!EFI_ERROR (Status)) {
      Status = AsciiStrnCatS (&NewChallenge->MultiString[0], DFCI_MULTI_STRING_MAX_SIZE, Element, ElementSize - sizeof (CHAR8));
      FreePool (Element);
    }

    if (!EFI_ERROR (Status)) {
      Status = DfciIdSupportGetProductName (&Element, &ElementSize);
      if (!EFI_ERROR (Status)) {
        Status = AsciiStrnCatS (&NewChallenge->MultiString[0], DFCI_MULTI_STRING_MAX_SIZE, Element, ElementSize - sizeof (CHAR8));
        FreePool (Element);
      }
    }

    if (!EFI_ERROR (Status)) {
      Status = DfciIdSupportGetManufacturer (&Element, &ElementSize);
      if (!EFI_ERROR (Status)) {
        Status = AsciiStrnCatS (&NewChallenge->MultiString[0], DFCI_MULTI_STRING_MAX_SIZE, Element, ElementSize - sizeof (CHAR8));
        FreePool (Element);
      }
    }

    // Print a debug message, but it isn't a big issue if the identification
    // doesn't get added to the recovery packet.
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error getting system identifier for recovery packet\n"));
    }

    *ChallengeSize = sizeof (DFCI_RECOVERY_CHALLENGE) + AsciiStrnSizeS (&NewChallenge->MultiString[0], DFCI_MULTI_STRING_MAX_SIZE);
  }

  return Status;
} // GetRecoveryChallenge()

/**
  Take in a DER-encoded x509 cert buffer and a challenge
  and will attempt to encrypt it for transmission. Encrypted data
  buffer will be allocated and populated on success.

  @param[in]  Challenge     Pointer to an DFCI_RECOVERY_CHALLENGE to be encrypted.
  @param[in]  ChallengeSize Size of Challenge object.
  @param[in]  PublicKey     Pointer to a DER-encoded x509 cert.
  @param[in]  PublicKeySize Size of the x509 cert provided.
  @param[out] EncryptedData     Encrypted data buffer or NULL on error.
  @param[out] EncryptedDataSize Size of the encrypted data buffer or 0 on error.

  @retval     EFI_SUCCESS           Challenge was successfully encrypted and can be found in buffer.
  @retval     EFI_INVALID_PARAMETER Nuff said.
  @retval     EFI_NOT_FOUND         Could not locate the RNG protocol or a secure RNG algorithm
                                    supported by the platform.
  @retval     EFI_ABORTED           Call to Pkcs1v2Encrypt() failed.
  @retval     Others                Error returned from LocateProtocol or GetRNG.

**/
EFI_STATUS
EncryptRecoveryChallenge (
  IN  DFCI_RECOVERY_CHALLENGE  *Challenge,
  IN  UINTN                    ChallengeSize,
  IN  CONST UINT8              *PublicKey,
  IN  UINTN                    PublicKeySize,
  OUT  UINT8                   **EncryptedData,
  OUT  UINTN                   *EncryptedDataSize
  )
{
  EFI_STATUS  Status;
  UINT8       ExtraSeed[RANDOM_SEED_BUFFER_SIZE];

  DEBUG ((DEBUG_INFO, "%a()\n", __FUNCTION__));

  //
  // Check inputs...
  if ((Challenge == NULL) || (PublicKey == NULL) || (EncryptedData == NULL) || (EncryptedDataSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: NULL pointer provided!\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Set default state...
  *EncryptedData     = NULL;
  *EncryptedDataSize = 0;

  //
  // We would like to provide a *little* more random for a better seed.
  // NOTE: This *could* be done with a direct call to RandomSeed() rather than
  //       passing it into the Pkcs1v2Encrypt() function. There are merits to
  //       each implementation.
  Status = GetRandomValue (&ExtraSeed[0], RANDOM_SEED_BUFFER_SIZE);

  //
  // Now, we should be able to encrypt the data and be done with it.
  if (!EFI_ERROR (Status)) {
    if (!Pkcs1v2Encrypt (
           PublicKey,
           PublicKeySize,
           (UINT8 *)Challenge,
           ChallengeSize,
           &ExtraSeed[0],
           RANDOM_SEED_BUFFER_SIZE,
           EncryptedData,
           EncryptedDataSize
           ))
    {
      DEBUG ((DEBUG_ERROR, "%a: Failed to encrypt the challenge!\n", __FUNCTION__));
      Status = EFI_ABORTED;
    }
  }

  return Status;
} // EncryptRecoveryChallenge()
