/** @file
  Code-Under-Test wrapper for DfciRecoveryLib.

  In the production DXE_DRIVER build, AutoGen.h transitively includes
  <Uefi.h>, so DfciRecoveryLib.c does not need to include it directly.
  In the HOST_APPLICATION build used by GoogleTest, the generated
  AutoGen.h only includes <Base.h>, leaving EFI types undefined.

  This wrapper pulls in <Uefi.h> first, then physically includes the
  unmodified Code-Under-Test (.c) file so it compiles in the host
  environment without modifying the production source.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include "../DfciRecoveryLib.c"
