# Testing DFCI

This describes the test structure for insuring DFCI operates properly.

1. A Host System (HOST) to run the test cases.
2. A Device Under Test (DUT) to be tested, with the new DFCI supported firmware, running the current
version of Windows.
3. Both systems on the same network.

## Overview

The DFCI tests are a collection of Robot Framework test cases.
Each Robot Framework test case collection is contained in a directory, and, as a minimum, contains a run.robot file.

Each test case collection is run manually in a proscribed order, and its status is verified before running the next test
case.
The tests must be run in order, with some exceptions, as they alter the system state, much like the real usage of DFCI.

## Equipment needed

The following equipment is needed to run the DFCI tests:

1. A system to run the test cases running a current version of Windows.
2. A System Under Test also running current version of Windows.
3. Both systems able to communicate with each other across the same network.

Optional equipment is a mechanism to collect firmware logs from the system under test.
Included is a serial port support function that looks for an FTDI serial connection.
See the Platforms\SimpleFTDI\ folder.

## Setting up the Device Under Test (DUT)

Copy the files needed for the DUT.
There is a script to help you do this.

1. Mount a USB device on the HOST system (the one with the DFCI source package).
Let's call this drive D:
2. Change the directory on the host system to ..\DfciPkg\UnitTests\DfctTests
3. Issue the command:

```text
DeviceUnderTest\CollectFilesForDut.cmd D:\DfciSetup
```

This will create a directory on the USB key named `DfciSetup` with the required files for setting up the remote server.
Mount the removable device on the DUT and start an administrator CMD Window, then run (where x: is the drive letter where
the USB key is mounted):

```text
x:\DfciSetup\SetupDUT.cmd
```

This will download and install Python, robotframework, robotremoteserver, and pypiwin32.
In addition, the SetupDUT command will update the firewall for the robot framework testing, and a make a couple of
configuration changes to Windows for a better test experience.

## Setting up the RefreshFromNetwork server

A Refresh Server is required to run the Refresh From Network portion of the DFCI E2E tests.
In addition, Refresh From Network may be run from the DUT firmware menu in lieu of running the DFCI_InTuneUnEnroll
testcase.

Instructions given here are to setup a Docker container running Ubuntu on Windows using Windows Subsystem for Linux 2 (WSL2).
You will need the IP address of the system that will run the RefreshFromNetwork container in order to generate
the SSL certificate used to access the RefreshFromNetwork server.

## Setting up the HOST system, with the Refresh From Network server running as a container

A Refresh Server is required to run the Refresh From Network portion of the DFCI E2E tests.
In addition, Refresh From Network may be run from the DUT firmware menu in lieu of running the DFCI_InTuneUnEnroll
testcase.

Instructions given here are to setup a Docker container on the HOST system.
This container will run the Ubuntu version of Linux on Windows using Windows Subsystem for Linux 2 (WSL2).

The HOST system requires the following software (NOTE - There are dependencies on x86-64 versions of Windows):

1. No other web server using ports 80 or 443 may run on the system chosen for the RefreshServer.
2. A current version of Windows x86-64.
3. The current Windows SDK, available here [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk).
4. Python x86-64 (the version tested), available here [Python 3.11.0](https://www.python.org/ftp/python/3.11.0/python-3.11.0-amd64.exe).
5. The testcase tree needs to be on the test host.  This can be your build repository, or you can clone just the files
needed by using this bat file to clone just the DfciTest tree: DfciPkg\UnitTests\DfciTests\CloneUnitTests.bat.
6. Install the required python packages by running using the pip-requirements.txt file in the DfciTests directory:

      ```text
         python -m pip install --upgrade -r pip-requirements.txt
       ```

7. Install Git for Windows, available here  [Git for Windows](https://gitforwindows.org/).
This is probably already installed, but the certificate generation in the next session will need to use the openssl.exe command.
Git for Windows distributes an acceptable version of openssl.exe that will be used in the preparation of the DFCI_HTTPS certificates.
8. Install Windows Subsystem for Linux. Install instructions here [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install).
9. Install Docker Desktop, available here [Docker Desktop](https://docs.docker.com/desktop/install/windows-install/).

### Configure DfciTests.ini, and initialize the Refresh Server Container

Dfci testing now uses a test configuration file, DfciTest.ini.
DfciTests.Template is the master .ini file.
If new settings are added in the future, they will be added to the DfciTests.Template file.
Every time that the DfciTests.ini file is read, the version is compared with DfciTest.Template.
When a new version is detected, the new fields are added to DfciTests.ini.

1. Copy DfciTests.Template DfciTests.ini
2. Edit DfciTests.ini and set the value of server_host_name to either a host name or an IP address of your HOST device.
The server_host_name field will be used to populate the Subject Alternative Names list in the DFCI_HTTPS SSL certificates.
3. In the directory DfciPkg\UnitTests\DfciTests\Certs, run MakeHTTPSCerts.  This will generate two SSL certificates.
One is for the RefreshServer, and the other is used by the RefreshServer testcase.
4. In the directory DfciPkg\UnitTests\DfciTests\RefreshServer, run CreateDockerContainer.  After the container is created,
the script will prompt you about starting the container.  I suggest using option 2 for this first test.
5. In another command window, cd to DfciPkg\UnitTests\DfciTests and run these two unit tests:

      ```text
         RunDfciTest.bat TestCases\DFCI_RefreshServer
         RunDfciTest.bat TestCases\DFCI_CertChainingTest
      ```

## Test Cases Collections

Table of DFCI Test case collections:

* Pre tests to validate the certs and the refresh server setup:

| Test Case Collection | Description of Test Case |
| ----- | ----- |
| DFCI_CertChainingTest | Verifies that a ZeroTouch enroll actually prompts for authorization to Enroll when the enroll package is not signed by the proper key.|
| DFCI_RefreshServer | Verifies the Refresh Server is operational before attempting the Refresh from Network EFI menu option in place of DFCI_InTuneUnenroll. |

* Testcases to validate the System Under Test:

| Test Case Collection | Description of Test Case |
| ----- | ----- |
| DFCI_InitialState | Verifies that the firmware indicates support for DFCI and that the system is Opted In for InTune, and is not already enrolled into DFCI. |
| DFCI_InTuneBadUpdate | Tries to apply a settings package signed with the wrong key |
| DFCI_InTunePermissions | Applies multiple sets of permissions to an InTune Enrolled system. |
| DFCI_InTuneEnroll | Applies a InTune Owner, an InTune Manager, and the appropriate permissions and settings. |
| DFCI_InTuneRollCerts | Updates the Owner and Manager certificates. This test can be run multiple times as it just swaps between two sets of certificates. |
| DFCI_InTuneSettings | Applies multiple sets of settings to a InTune Enrolled system. |
| DFCI_InTuneUnenroll | Applies an InTune Owner unenroll package, that removes both the InTune Owner and the InTune Manager, resets the Permission Database, and restores settings marked No UI to their default state. |

## Note on the firmware for testing DFCI

Most of DFCI functionality can be tested without regard of the Zero Touch certificate.
To test functionality of the Zero Touch feature, the firmware needs to be built with the ZTD_Leaf.cer file instead of
the ZtdRecovery.cer file.

To do this, change your platform .fdf file from:

```text
FILE FREEFORM = PCD(gZeroTouchPkgTokenSpaceGuid.PcdZeroTouchCertificateFile) {
    SECTION RAW = ZeroTouchPkg/Certs/ZeroTouch/ZtdRecovery.cer
}
```

to:

```text
FILE FREEFORM = PCD(gZeroTouchPkgTokenSpaceGuid.PcdZeroTouchCertificateFile) {
    SECTION RAW = DfciPkg/UnitTests/DfciTests/ZTD_Leaf.cer
}
```

## One time setup for the tests

### WARNING: Do not ship with the ZTD_Leaf.cer certificate in your firmware

Be sure your production systems are using the ZtdRecovery.cer file.

## Running The First Test Case

Run the first test as shown replacing 11.11.11.211 with the actual IP address of the DUT.
You should expect to see similar output with all four tests passing.

<!-- spellchecker: disable -->
<!-- This omits the below code block from cspell checking -->
```txt
DfciTests>python.exe -m robot.run -L TRACE -x DFCI_InitialState.xml -A Platforms\SimpleFTDI\Args.txt -v IP_OF_DUT:192.168.1.177 -v TEST_OUTPUT_BASE:C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759 -d C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759 TestCases\DFCI_InitialState\run.robot
==============================================================================
Run :: DFCI Initial State test - Verifies that there are no enrolled identi...
==============================================================================
Ensure Mailboxes Are Clean                                            ..
DfciTests\TestCases\DFCI_InitialState\run.robot
Ensure Mailboxes Are Clean                                            | PASS |
------------------------------------------------------------------------------
Verify System Unter Test is Opted In                                  | PASS |
------------------------------------------------------------------------------
Check that the starting DFCI Ownership is Unenrolled                  | PASS |
------------------------------------------------------------------------------
Obtain Target Parameters From Target                                  | PASS |
------------------------------------------------------------------------------
Verify Initial Permissions                                            ..Initializing testcases
..Running test
Verify Initial Permissions                                            | PASS |
------------------------------------------------------------------------------
Run :: DFCI Initial State test - Verifies that there are no enroll... | PASS |
5 tests, 5 passed, 0 failed
==============================================================================
Output:  C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759\output.xml
XUnit:   C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759\DFCI_InitialState.xml
Log:     C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759\log.html
Report:  C:\TestLogs\robot\DFCI_InitialState\logs_20230131_090759\report.html
```
<!-- spellchecker: enable -->

## Standard Testing

Starting with a DUT that is not enrolled in DFCI, run the tests in the following order:

1. DFCI_InitialState
2. DFCI_InTuneEnroll
3. DFCI_InTuneRollCerts
4. DFCI_InTunePermissions
5. DFCI_InTuneSettings
6. DFCI_InTuneBadUpdate
7. DFCI_InTuneUnenroll

Steps 3 through 6 can and should be repeated in any order.

## Extended Testing

This tests also start with a DUT that is not enrolled in DFCI, and will leave the system not enrolled if it completes successfully.

* DFCI_CertChainingTest

## Recovering from errors

Code issues can present issues with DFCI that may require deleting the Identity and Permission databases.
Using privileged access of a DUT that unlocks the varstore, you can delete the two master variables of DFCI.
These variables are:

1. \_SPP
2. \_IPCVN

## USB Refresh Test

The test cases DFCI_InTuneEnroll and DFCI_InTuneUnenroll have a GenUsb.bat file.
The GenUsb.bat file will generate a .dfi file that UEFI management menu can read.

```txt
GenUsb MFG_NAME PRODUCT_NAME SERIAL_NUMBER
```

If there is a space or other special characters, add double quotes as in:

```txt
GenUsb Fabrikam "Fabrikam Spelunker Kit" "SN-47599011345"
```

After producing your .dfi file, place it on a USB drive and take it to the system under test.
Press the Install from USB button to apply the packets.

## Variable Lock Test

DfciPkg/UnitTest/DfciVarLockAudit contains a UEFI shell command to audit the state of the DFCI
variables.
This test should be run at multiple points during the development cycle, with the
DfciVarLockAudit test run after enrollment and again after the system has been unenrolled.
This verifies that the variables that secure the state of DFCI are not compromised.
The automation to run DfciVarLock test automatically is not available at this time.
