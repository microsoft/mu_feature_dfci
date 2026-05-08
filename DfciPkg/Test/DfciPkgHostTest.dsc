## @file
# DfciPkg DSC file used to build host-based unit tests.
#
# Copyright (C) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME           = DfciPkgHostTest
  PLATFORM_GUID           = 7E3A2B5D-94E3-4F57-9F1B-9E0E0EDB6A11
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build/DfciPkg/HostTest
  SUPPORTED_ARCHITECTURES = IA32|X64
  BUILD_TARGETS           = NOOPT
  SKUID_IDENTIFIER        = DEFAULT

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc

[LibraryClasses]
  # Real (non-mock) library implementation of the CUT.
  DfciRecoveryLib|DfciPkg/Library/DfciRecoveryLib/DfciRecoveryLib.inf

[Components]
  #
  # Build the DfciRecoveryLib host unit test. Each external dependency
  # of the CUT is overridden to point at its gMock mock library so the
  # tests can drive every code path deterministically. The mock for
  # the RNG *protocol* (returned through gBS->LocateProtocol) is
  # delivered as a .cpp file pulled in via the test INF's [Sources].
  #
  DfciPkg/Test/UnitTest/DfciRecoveryLib/DfciRecoveryLibGoogleTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|MdePkg/Test/Mock/Library/GoogleTest/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      BaseCryptLib|CryptoPkg/Test/Mock/Library/GoogleTest/MockBaseCryptLib/MockBaseCryptLib.inf
      DfciDeviceIdSupportLib|DfciPkg/Test/Mock/Library/GoogleTest/MockDfciDeviceIdSupportLib/MockDfciDeviceIdSupportLib.inf
    <PcdsFixedAtBuild>
      # Disable Halt-on-Assert so unrelated ASSERT()s in the linked
      # library code do not abort the test harness mid-suite.
      gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x0E
  }

  #
  # Build the new DfciDeviceIdSupportLib mock as a stand-alone library
  # so future DfciPkg host tests can pick it up via the same DSC.
  #
  DfciPkg/Test/Mock/Library/GoogleTest/MockDfciDeviceIdSupportLib/MockDfciDeviceIdSupportLib.inf
