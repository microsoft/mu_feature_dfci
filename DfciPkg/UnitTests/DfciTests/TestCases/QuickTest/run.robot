*** Settings ***
# @file
#
Documentation
...     DFCI All Settings test
...     This test suite checks the action of most current setting providers. The
...     limitation is for enable, known enum, and string settings.
...
...     NOTE:
...
...     The ASSET TAG test are dependent upon the DFCI PCD's being set to these values:
...
...         gDfciPkgTokenSpaceGuid.PcdDfciAssetTagChars|"0123456789-.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"|VOID*|0x40000017
...         gDfciPkgTokenSpaceGuid.PcdDfciAssetTagLen | 36 | UINT16 | 0x40000018
...
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

MetaData
...     - Build a settings packet
...     - Send it to the system under test
...     - Reboot the system under test to apply the settings
...     - Get the new "Current Settings"
...     - Verify the settings that were changed

Library         OperatingSystem
Library         Process
Library         Collections

Library         Support${/}Python${/}DFCI_SupportLib.py
Library         Support${/}Python${/}DependencyLib.py
Library         Support${/}Python${/}SettingsXMLLib.py
Library         Remote  http://${IP_OF_DUT}:${RF_PORT}

#Import the Generic Shared keywords
Resource        Support${/}Robot${/}DFCI_Shared_Paths.robot
Resource        Support${/}Robot${/}CertSupport.robot
Resource        Support${/}Robot${/}DFCI_Shared_Keywords.robot

#Import the platform specific log support
Resource        UefiSerial_Keywords.robot

# Use the following line for Python remote write to the UEFI Variables
Resource        Support${/}Robot${/}DFCI_VariableTransport.robot

Suite setup     Make Dfci Output
Suite Teardown  Terminate All Processes    kill=True


*** Variables ***
#default var but should be changed on the command line
${IP_OF_DUT}            127.0.0.1
${RF_PORT}              8270

#test output dir for data from this test run.
${TEST_OUTPUT_BASE}     ..${/}TEST_OUTPUT

#Test output location
${TEST_OUTPUT}          ${TEST_OUTPUT_BASE}

#Test Root Dir
${TEST_ROOT_DIR}        TestCases
${TEST_CASE_DIR}        ${TEST_ROOT_DIR}${/}DFCI_AllSettings

${TOOL_DATA_OUT_DIR}    ${TEST_OUTPUT}${/}bindata
${TOOL_STD_OUT_DIR}     ${TEST_OUTPUT}${/}stdout
${BOOT_LOG_OUT_DIR}     ${TEST_OUTPUT}${/}uefilogs

${CERTS_DIR}            Certs

${TARGET_VERSION}       V2

${DDS_CA_THUMBPRINT}      'Thumbprint Not Set'
${MDM_CA_THUMBPRINT}      'Thumbprint Not Set'
${ZTD_LEAF_THUMBPRINT}    'Thumbprint Not Set'

${CURRENT_PERMISSIONS_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}DfciPermissionCurrent.xml
${CURRENT_SETTINGS_XML_FILE}=       ${TOOL_DATA_OUT_DIR}${/}DfciSettingsCurrent.xml

${OWNER_SETTINGS1_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}OwnerSettings1.xml
${OWNER_SETTINGS2_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}OwnerSettings2.xml
${OWNER_SETTINGS3_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}OwnerSettings3.xml
${OWNER_RESTORE_XML_FILE}=      ${TOOL_DATA_OUT_DIR}${/}OwnerRestore.xml

${USER_SETTINGS1_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}UserSettings1.xml
${USER_SETTINGS2_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}UserSettings2.xml
${USER_SETTINGS3_XML_FILE}=    ${TOOL_DATA_OUT_DIR}${/}UserSettings3.xml
${USER_RESTORE_XML_FILE}=      ${TOOL_DATA_OUT_DIR}${/}UserRestore.xml

*** Keywords ***
Get The DFCI Settings
    [Arguments]    ${nameOfTest}
    ${deviceIdXmlFile}=         Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_deviceIdentifier.xml
    ${currentIdXmlFile}=        Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_currentIdentities.xml

    Get and Print Device Identifier    ${deviceIdXmlFile}

    Get and Print Current Identities   ${currentIdxmlFile}

    Get and Print Current Permissions  ${CURRENT_PERMISSIONS_XML_FILE}

    Get and Print Current Settings     ${CURRENT_SETTINGS_XML_FILE}

    [return]    ${currentIdxmlFile}


Initialize lists of tests

#[Documentation]
#... Get the current settings and the current permissions to generate a list
#... settings for the current owner to change.
    ${stdSettingsFileName}=    Set Variable   ${TEST_CASE_DIR}${/}DfciStdSettings.list

    File should Exist    ${CURRENT_PERMISSIONS_XML_FILE}
    File should Exist    ${CURRENT_SETTINGS_XML_FILE}

    ${OwnerList}  ${UserList}  ${UnsupportedList}=  Generate Master Lists  ${CURRENT_SETTINGS_XML_FILE}  ${CURRENT_PERMISSIONS_XML_FILE}

    Generate Settings Lists    ${OwnerList}    ${OWNER_SETTINGS1_XML_FILE}    ${OWNER_SETTINGS2_XML_FILE}    ${OWNER_SETTINGS3_XML_FILE}    ${OWNER_RESTORE_XML_FILE}

    IF  @{UnsupportedList}!=[]
        Log To Console    .
        Log To Console    ${yellow}WARNING:${white}
        Log To Console    The following settings cannot be tested due to the nature of the setting:
        FOR  ${element}  IN  @{UnsupportedList}
            Log To Console    ${yellow}${element}${white}
        END
    END

    File should Exist    ${OWNER_SETTINGS1_XML_FILE}
    File should Exist    ${OWNER_SETTINGS2_XML_FILE}
    File should Exist    ${OWNER_SETTINGS3_XML_FILE}
    File should Exist    ${OWNER_RESTORE_XML_FILE}

    Generate Settings Lists    ${UserList}    ${USER_SETTINGS1_XML_FILE}    ${USER_SETTINGS2_XML_FILE}    ${USER_SETTINGS3_XML_FILE}    ${USER_RESTORE_XML_FILE}

    File should Exist    ${USER_SETTINGS1_XML_FILE}
    File should Exist    ${USER_SETTINGS2_XML_FILE}
    File should Exist    ${USER_SETTINGS3_XML_FILE}
    File should Exist    ${USER_RESTORE_XML_FILE}

    @{UntestedSettings}    generate_standard_test_coverage    ${OWNER_RESTORE_XML_FILE}    ${USER_RESTORE_XML_FILE}    ${stdSettingsFileName}

    IF  @{UntestedSettings}!=[]
        Log To Console    .
        Log To Console    ${yellow}WARNING:${white}
        Log To Console    The following Standard settings are not supported by this platform:
        FOR  ${element}  IN  @{UntestedSettings}
            Log To Console    ${yellow}${element}${white}
        END
    END


Apply Staged Group Settings
    [Arguments]    ${nameOfTest}  ${initial_action}  ${owner_settings_xml_file}  ${user_settings_xml_file}

    ${StagedOwnerResultFile}=      Set Variable  ${nameofTest}_Owner_Settings_Result.bin
    ${StagedUserResultFile}=       Set Variable  ${nameofTest}_User_Settings_Result.bin
    ${StagedCurrentSettingsFile}=  Set Variable  ${nameofTest}_Current_Settings.xml

# Stage these packets to process later
    Process Settings Packet     ${nameofTest}  ${OWNER}  ${STAGE}  ${OLD_OWNER_PFX}  ${owner_settings_xml_file}  @{TARGET_PARAMETERS}
    Process Settings Packet     ${nameofTest}  ${USER}  ${STAGE}  ${OLD_USER_PFX}  ${user_settings_xml_file}  @{TARGET_PARAMETERS}

    ${StagedOwnerResultFile}=      Set Variable  ${nameofTest}_Owner_Settings_Result.bin
    ${StagedUserResultFile}=       Set Variable  ${nameofTest}_User_Settings_Result.bin
    ${StagedCurrentSettingsFile}=  Set Variable  ${nameofTest}_Current_Settings.xml

    Write Staged Action    ${initial_action}
    Write Staged Action    GetVariable    ${SETTINGS_RESULT}  ${SETTINGS_GUID}   ${StagedOwnerResultFile}
    Write Staged Action    GetVariable    ${SETTINGS2_RESULT}  ${SETTINGS_GUID}  ${StagedUserResultFile}
    Write Staged Action    GetVariable    ${SETTINGS_CURRENT}  ${SETTINGS_GUID}  ${StagedCurrentSettingsFile}


#
#
#
#------------------------------------------------------------------*
#  Test Cases                                                      *
#------------------------------------------------------------------*
*** Test Cases ***


Ensure Mailboxes Are Clean
#Documentation Ensure all mailboxes are clear at the beginning of a test.  If there are any mailboxes that have an element, a previous test failed.

   ${robot_version}=    remote get version
    Should Be True    ${robot_version} >= 1.07

    Initialize Colors

    Verify No Mailboxes Have Data

    Log To Console    .
    Log To Console    ${SUITE SOURCE}


Get the starting DFCI Settings
    [Setup]    Require test case    Ensure Mailboxes Are Clean
    ${nameofTest}=    Set Variable    DisplaySettingsAtStart

    ${currentIdXmlFile}=    Get The DFCI Settings    ${nameOfTest}

    ${OwnerThumbprint}=    Get Thumbprint Element    ${currentIdxmlFile}  Owner
    ${UserThumbprint}=     Get Thumbprint Element    ${currentIdxmlFile}  User

    Initialize Thumbprints    '${OwnerThumbprint}'    '${UserThumbprint}'

    Should Be True    '${OwnerThumbprint}' != 'Cert not installed'
    Should Be True    '${UserThumbprint}' != 'Cert not installed'


Obtain Target Parameters From Target
    [Setup]    Require test case    Get the starting DFCI Settings

    ${nameofTest}=           Set Variable     GetParameters
    ${SerialNumber}=         Get System Under Test SerialNumber
    ${Manufacturer}=         Get System Under Test Manufacturer
    ${Model}=                Get System Under Test ProductName
    @{TARGET_PARAMETERS}=    Build Target Parameters  ${TARGET_VERSION}  ${SerialNumber}  ${Manufacturer}  ${Model}
    Set Suite Variable       @{TARGET_PARAMETERS}

    ${currentXmlFile}=    Set Variable   ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_UefiDeviceId.xml

    Get Device Identifier    ${currentXmlFile}
    Verify Identity Current  ${currentXmlFile}  ${Manufacturer}  ${Model}  ${SerialNumber}


Initialize Testcases
    [Setup]    Require test case    Obtain Target Parameters From Target

    Log To Console    .
    Log To Console    Initializing testcases

    Initialize lists of tests

Send User settings group 1
    [Setup]    Require test case    Initialize Testcases
    ${nameofTest}=    Set Variable     GroupU1

#   For the first apply, clear the staging list, and all .bin and .all previous xml files
    Apply Staged Group Settings  ${nameofTest}  ClearStagedFiles  ${OWNER_SETTINGS1_XML_FILE}  ${USER_SETTINGS1_XML_FILE}


Verifying the Staged Actions for group 2
    ${nameofTest}=    SetVariable    GroupU1

    Write Staged Action    ResetSystem
    Write Staged Action    ResetSystem

    Reboot System And Wait For Staged Restarts    10

