*** Settings ***
# @file
#
Documentation    DFCI Shared Keywords
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

Library     OperatingSystem
Library     Process
Library     DateTime
Library     Remote  http://${IP_OF_DUT}:${RF_PORT}
Library     Support${/}Python${/}DFCI_SupportLib.py

Resource     Support${/}Robot${/}DFCI_Shared_Keywords2.robot

#Import the Generic Shared keywords
Resource        Support${/}Robot${/}DFCI_Shared_Paths.robot


*** Variables ***
${CMD_MFG}              Get-CimInstance -ClassName Win32_ComputerSystem -Property Manufacturer | Select-Object -ExpandProperty Manufacturer
${CMD_MODEL}            Get-CimInstance -ClassName Win32_ComputerSystem -Property Model | Select-Object -ExpandProperty Model
${CMD_SERIALNUMBER}     Get-CimInstance -ClassName Win32_systemenclosure -Property SerialNumber  | Select-Object -ExpandProperty SerialNumber
${CMD_UUID}             Get-CimInstance -ClassName Win32_computersystemproduct -Property uuid | Select-Object -ExpandProperty uuid


*** Keywords ***
Initialize Colors
    ${red}=       Evaluate    "\\033[31m"
    ${green}=     Evaluate    "\\033[32m"
    ${yellow}=    Evaluate    "\\033[33m"
    ${blue}=      Evaluate    "\\033[34m"
    ${purple}=    Evaluate    "\\033[35m"
    ${cyan}=      Evaluate    "\\033[36m"
    ${white}=     Evaluate    "\\033[37m"

    Set Suite Variable   ${red}
    Set Suite Variable   ${green}
    Set Suite Variable   ${yellow}
    Set Suite Variable   ${blue}
    Set Suite Variable   ${purple}
    Set Suite Variable   ${cyan}
    Set Suite Variable   ${white}

Compare Files
    [Arguments]     ${CompareFile1}  ${CompareFile2}  ${ExpectedRC}

    ${result}=  Run Process    fc.exe  /b  ${CompareFile1}  ${CompareFile2}
    Log     all stdout: ${result.stdout}
    Log     all stderr: ${result.stderr}
    Should Be Equal As Integers  ${result.rc}  ${ExpectedRC}


############################################################
#           Get system under test Information              #
############################################################

Get System Under Test SerialNumber
    ${Value}=   Run PowerShell And Return Output   ${CMD_SERIALNUMBER}
    Should Be True  '${Value}' != 'Error'
    Should Be True  '${Value}' != ''
    [Return]        ${Value}


Get System Under Test Manufacturer
    ${Value}=   Run PowerShell And Return Output   ${CMD_MFG}
    Should Be True  '${Value}' != 'Error'
    Should Be True  '${Value}' != ''
    [Return]        ${Value}


Get System Under Test ProductName
    ${Value}=   Run PowerShell And Return Output   ${CMD_MODEL}
    Should Be True  '${Value}' != 'Error'
    Should Be True  '${Value}' != ''
    [Return]        ${Value}


############################################################
#              Get Current Settings Value in XML           #
############################################################
Get Current Settings Package
    [Arguments]     ${stdoutfile}
    ${result} =     Run Process     ${DFCI_PY_PATH}${/}GetSEMResultData.py  --CurrentSettings   --IpAddress ${IP_OF_DUT}    shell=Yes   timeout=10sec    stdout=${stdoutfile}
    Log File    ${stdoutfile}
    Should Be Equal As Integers     ${result.rc}    0


Verify Provision Response
    [Arguments]     ${pktfile}  ${ResponseFile}  ${ExpectedRc}
    @{rc2}=     get status and sessionid from identity results  ${ResponseFile}
    ${id2}=     get sessionid from identity packet  ${pktfile}
    ${rc2zstring}=      get uefistatus string    ${rc2}[0]
    ${ExpectedString}=  get uefistatus string    ${ExpectedRc}
    Should Be Equal As Integers     ${rc2}[1]    ${id2}
    Should Be Equal As strings      ${rc2zstring}   ${ExpectedString}


Verify Permission Response
    [Arguments]     ${pktfile}  ${ResponseFile}  ${ExpectedRc}
    @{rc2}=     get status and sessionid from permission results  ${ResponseFile}
    ${id2}=     get sessionid from permission packet  ${pktfile}
    ${rc2zstring}=      get uefistatus string    ${rc2}[0]
    ${ExpectedString}=  get uefistatus string    ${ExpectedRc}
    Should Be Equal As Integers     ${rc2}[1]   ${id2}
    Should Be Equal As strings      ${rc2zstring}   ${ExpectedString}


Verify Settings Response
    [Arguments]     ${pktfile}  ${ResponseFile}  ${ExpectedRc}  ${checktype}
    @{rc2}=     get status and sessionid from settings results  ${ResponseFile}  ${checktype}
    ${id2}=     get sessionid from settings packet  ${pktfile}
    ${rc2zstring}=      get uefistatus string    ${rc2}[0]
    ${ExpectedString}=  get uefistatus string    ${ExpectedRc}
    Should Be Equal As Integers     ${rc2}[1]   ${id2}
    Should Be Equal As strings      ${rc2zstring}   ${ExpectedString}


Verify Identity Current
    [Arguments]     ${xmlfile}  ${Mfg}  ${ProdName}  ${SerialNumber}
    ${rc}=      Verify Device Id    ${xmlfile}  ${Mfg}  ${ProdName}  ${SerialNumber}
    Should Be Equal As Integers     ${rc}   0
    ${rc}=    Verify Dfci Version   ${xmlfile}   2
    Should Be True    ${rc}


Get and Print Current Identities
    [Arguments]   ${currentxmlFile}

    Get Current Identities   ${currentxmlFile}
    Print Xml Payload        ${currentxmlFile}


Get and Print Current Permissions
    [Arguments]   ${currentxmlFile}

    Get Current Permissions  ${currentxmlFile}
    Print Xml Payload        ${currentxmlFile}


Get and Print Current Settings
    [Arguments]   ${currentxmlFile}

    Get Current Settings    ${currentxmlFile}
    Print Xml Payload       ${currentxmlFile}


Get and Print Device Identifier
    [Arguments]   ${currentxmlFile}

    Get Device Identifier   ${currentxmlFile}
    Print Xml Payload       ${currentxmlFile}


############################################################
#      Resetting system and wait for PyRobotRemote         #
############################################################

Reboot System And Wait For System Online
    [Timeout]  10minutes

    #
    # remote warm reboot sets reboot_complete to False.
    #
    ${reboot_complete}=  Set Variable  ${False}

    TRY
        remote_warm_reboot
    EXCEPT  Connection to remote server broken  type=start
        BREAK
    END

    Log To Console  .
    ${index}=  Set Variable  ${0}
    WHILE  ${reboot_complete} == ${False}
        Sleep  1sec  "Waiting for Robot Framework Remote Server To to go offline"

        ${mod}=  Evaluate  ${index} % 10
        ${index}=  Evaluate  ${index} + 1
        Run keyword if  ${mod} == 0  Log To Console  Waiting for Robot Framework Remote Server To to go offline(${index})

        TRY
            ${reboot_complete}=  is_reboot_complete
        EXCEPT
            ${reboot_complete}=  Set Variable  ${False}
            BREAK
        END
    END

    ${index}=  Set Variable  ${0}
    WHILE  ${reboot_complete} == ${False}
        Sleep  1sec    "Waiting for Robot Framework Remote Server To come back online"

        ${mod}=  Evaluate  ${index} % 10
        ${index}=  Evaluate  ${index} + 1
        Run keyword if  ${mod} == 0  Log To Console  Waiting for Robot Framework Remote Server To come back online(${index})

        TRY
            ${reboot_complete}=  is_reboot_complete
        EXCEPT
            ${reboot_complete}=  Set Variable  ${False}
        END
    END


############################################################
#      Verify NO APPLY variables present                   #
############################################################

Verify No Mailboxes Have Data

    @{rcid}=    GetUefiVariable    ${IDENTITY_APPLY}  ${IDENTITY_GUID}  ${None}
    Run Keyword If   ${rcid}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${IDENTITY_APPLY}  ${IDENTITY_GUID}

    @{rcid2}=    GetUefiVariable    ${IDENTITY2_APPLY}  ${IDENTITY_GUID}  ${None}
    Run Keyword If   ${rcid2}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${IDENTITY2_APPLY}  ${IDENTITY_GUID}

    @{rcperm}=    GetUefiVariable    ${PERMISSION_APPLY}  ${PERMISSION_GUID}  ${None}
    Run Keyword If   ${rcperm}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${PERMISSION_APPLY}  ${PERMISSION_GUID}

    @{rcperm2}=    GetUefiVariable    ${PERMISSION2_APPLY}  ${PERMISSION_GUID}  ${None}
    Run Keyword If   ${rcperm2}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${PERMISSION2_APPLY}  ${PERMISSION_GUID}

    @{rcset}=    GetUefiVariable    ${SETTINGS_APPLY}  ${SETTINGS_GUID}  ${None}
    Run Keyword If   ${rcset}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${SETTINGS_APPLY}  ${SETTINGS_GUID}

    @{rcset2}=    GetUefiVariable    ${SETTINGS2_APPLY}  ${SETTINGS_GUID}  ${None}
    Run Keyword If   ${rcset2}[0] != ${STATUS_VARIABLE_NOT_FOUND}
    ...    SetUefiVariable    ${SETTINGS2_APPLY}  ${SETTINGS_GUID}

    Should Be True    ${rcid}[0] == ${STATUS_VARIABLE_NOT_FOUND}
    Should Be True    ${rcperm}[0] == ${STATUS_VARIABLE_NOT_FOUND}
    Should Be True    ${rcperm2}[0] == ${STATUS_VARIABLE_NOT_FOUND}
    Should Be True    ${rcset}[0] == ${STATUS_VARIABLE_NOT_FOUND}
    Should Be True    ${rcset2}[0] == ${STATUS_VARIABLE_NOT_FOUND}


########################################################################
#      Apply a Provision (Identity) Package, and check the results     #
########################################################################
Process Provision Packet
    [Arguments]    ${nameofTest}  ${Identity}  ${signPfxFile}  ${testsignPfxFile}  ${ownerCertFile}  ${KEY_INDEX}  @{TargetParms}
    ${applyPackageFile}=   Set Variable    ${TOOL_STD_OUT_DIR}${/}${nameofTest}_${Identity}_Provision_apply.log
    ${binPackageFile}=     Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Provision_apply.bin

    #Create and deploy an identity packet

    Create Dfci Provisioning Package     ${binPackageFile}  ${signPfxFile}  ${testsignPfxFile}  ${ownerCertFile}  ${KEY_INDEX}  @{TargetParms}
    Print Provisioning Package           ${binPackageFile}  ${applyPackageFile}

    Apply Identity  ${Identity}  ${binPackageFile}


Validate Provision Status
    [Arguments]    ${nameofTest}  ${Identity}  ${expectedStatus}
    ${binPackageFile}=     Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Provision_apply.bin
    ${binResultFile}=      Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Provision_result.bin

    Get Identity Results  ${Identity}  ${binResultFile}

    Verify Provision Response   ${binPackageFile}  ${binResultFile}  ${expectedStatus}


##############################################################
#      Apply a Permission Package, and check the results     #
##############################################################
Process Permission Packet
    [Arguments]  ${nameofTest}  ${Identity}  ${ownerPfxFile}  ${PayloadFile}  @{TargetParms}
    ${applyPackageFile}=  Set Variable  ${TOOL_STD_OUT_DIR}${/}${nameofTest}_${Identity}_Permission_apply.log
    ${binPackageFile}=    Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Permission_apply.bin


    #Create and deploy a permissions packet

    Create Dfci Permission Package  ${binPackageFile}  ${ownerPfxFile}  ${PayloadFile}  @{TargetParms}
    Print Permission Package        ${binPackageFile}  ${applyPackageFile}

    Apply Permission  ${Identity}  ${binPackageFile}


Validate Permission Status
    [Arguments]  ${nameofTest}  ${Identity}  ${expectedStatus}
    ${binPackageFile}=  Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Permission_apply.bin
    ${binResultFile}=   Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Permission_result.bin
    ${xmlResultFile}=   Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Permission_result.xml

    Get Permission Results  ${Identity}  ${binResultFile}

    Verify Permission Response  ${binPackageFile}  ${binResultFile}  ${expectedStatus}

    # V1 doesn't have permission payload
    Return From Keyword If  '${TARGET_VERSION}' == 'V1'

    Get Payload From Permissions Results  ${binResultFile}  ${xmlResultFile}
    File Should Exist  ${xmlResultFile}
    [return]  ${xmlResultFile}


############################################################
#      Apply a Settings Package, and check the results     #
############################################################
Process Settings Packet
    [Arguments]  ${nameofTest}  ${Identity}  ${Staged}  ${ownerPfxFile}  ${PayloadFile}  @{TargetParms}
    ${applyPackageFile}=  Set Variable  ${TOOL_STD_OUT_DIR}${/}${nameofTest}_${Identity}_Settings_apply.log
    ${binPackageFile}=    Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Settings_apply.bin

    #Create and deploy a settings packet

    Create Dfci Settings Package  ${binPackageFile}  ${ownerPfxFile}  ${PayloadFile}  @{TargetParms}
    Print Settings Package        ${binPackageFile}    ${applyPackageFile}

    IF  '${Staged}' == '${STAGE}'
        Apply Staged Settings  ${Identity}  ${binPackageFile}
    ELSE
        Apply Settings  ${Identity}  ${binPackageFile}
    END


Validate Settings Status
    [Arguments]  ${nameofTest}  ${Identity}  ${expectedStatus}  ${full}
    ${binPackageFile}=  Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Settings_apply.bin
    ${binResultFile}=   Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Settings_result.bin
    ${xmlResultFile}=   Set Variable  ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_Settings_result.xml

    Get Settings Results  ${Identity}  ${binResultFile}

    Verify Settings Response    ${binPackageFile}  ${binResultFile}  ${expectedStatus}  ${full}

    Get Payload From Settings Results  ${binResultFile}  ${xmlResultFile}
    Run Keyword If  '${expectedStatus}' == ${STATUS_SUCCESS}  File Should Exist  ${xmlResultFile}
    [return]  ${xmlResultFile}


########################################################################
#      Process Unenroll Package                                        #
########################################################################
Process UnEnroll Packet
    [Arguments]    ${nameofTest}  ${Identity}  ${signPfxFile}  ${KEY_INDEX}  @{TargetParms}
    ${applyPackageFile}=   Set Variable    ${TOOL_STD_OUT_DIR}${/}${nameofTest}_${Identity}_UnEnroll_apply.log
    ${binPackageFile}=     Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_UnEnroll_apply.bin

    #Create and deploy an identity packet

    Create Dfci Unenroll Package     ${binPackageFile}  ${signPfxFile}  ${KEY_INDEX}  @{TargetParms}
    Print Provisioning Package       ${binPackageFile}  ${applyPackageFile}

    Apply Identity  ${Identity}  ${binPackageFile}


Validate UnEnroll Status
    [Arguments]    ${nameofTest}  ${Identity}  ${expectedStatus}
    ${binPackageFile}=     Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_UnEnroll_apply.bin
    ${binResultFile}=      Set Variable    ${TOOL_DATA_OUT_DIR}${/}${nameofTest}_${Identity}_UnEnroll_result.bin

    Get Identity Results  ${Identity}  ${binResultFile}

    Verify Provision Response    ${binPackageFile}  ${binResultFile}  ${expectedStatus}
