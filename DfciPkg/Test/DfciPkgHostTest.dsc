## @file
# DfciPkg host-test platform DSC.
#
# Builds and runs all DfciPkg host-based GoogleTest executables.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME           = DfciPkgHostTest
  PLATFORM_GUID           = 8F2A8C76-2D8E-4F4F-9B1A-7E5C44B9F7B2
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build/DfciPkg/HostTest
  SUPPORTED_ARCHITECTURES = IA32|X64
  BUILD_TARGETS           = NOOPT
  SKUID_IDENTIFIER        = DEFAULT

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf

[Components]
  #
  # DfciRecoveryLib host-based GoogleTest. ASSERT() must remain enabled
  # because several tests use EXPECT_ANY_THROW to confirm the CUT's
  # ASSERT_EFI_ERROR() guards fire on bad RNG outcomes - so we do NOT
  # set PcdDebugPropertyMask|0x0E here.
  #
  DfciPkg/Library/DfciRecoveryLib/GoogleTest/DfciRecoveryLibGoogleTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  }
