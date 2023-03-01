# @file
#
# DFCI_SupportLib- DFCI basic functions
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

#
# DFCI_SupportLib
#
import binascii
import os
import random
import socket
import subprocess
import time
import traceback
import xml.dom.minidom
import configparser
import json
import pathlib
import xml.etree.ElementTree as ElementTree

from io import BytesIO

from builtins import int

from edk2toollib.uefi.status_codes import UefiStatusCode
from edk2toollib.windows.locate_tools import FindToolInWinSdk

from Data.CertProvisioningVariable import CertProvisioningApplyVariable
from Data.CertProvisioningVariable import CertProvisioningResultVariable
from Data.PermissionPacketVariable import PermissionApplyVariable
from Data.PermissionPacketVariable import PermissionResultVariable
from Data.SecureSettingVariable import SecureSettingsApplyVariable
from Data.SecureSettingVariable import SecureSettingsResultVariable

SignToolPath = None
CertMgrPath = None
DfciTest_Template = 'DfciTests.Template'
DfciTest_Config = 'DfciTests.ini'

permission_database = None


class DFCI_SupportLib(object):

    def get_test_config(self):

        #
        # The config files are located two directories above where DFCI_SupportLib.py is located.
        #
        modpath = pathlib.Path(__file__).parent.parent.parent
        config_path = os.path.realpath(modpath)

        template_path = os.path.join(config_path, DfciTest_Template)
        ini_path = os.path.join(config_path, DfciTest_Config)
        config = configparser.ConfigParser()

        if not os.path.exists(template_path):
            raise Exception(f'Unable to locate test configuration template({template_path}).')

        config.read(template_path)
        template_ver = int(config["DfciConfig"]["version"])
        update_config = True
        if os.path.exists(ini_path):
            config.read(ini_path)
            current_ver = int(config["DfciConfig"]["version"])
            if current_ver < template_ver:
                config["DfciConfig"]["version"] = str(template_ver)
            else:
                update_config = False
        if update_config:
            with open(ini_path, 'w') as config_file:
                config.write(config_file)
                config_file.close()

        return config

    def compare_json_files(self, request_name, expected_name):

        with open(request_name, "r") as request_file:
            requested_data = json.load(request_file)

        with open(expected_name, "r") as expected_file:
            expected_data = json.load(expected_file)

        is_equal = all((requested_data.get(k) == v for k, v in expected_data.items()))
        if is_equal:
            is_equal = all((expected_data.get(k) == v for k, v in requested_data.items()))
        return is_equal

    def _ReturnSessionIdValue(self, input_string):
        # Session Id:       0xF08A4
        return int(input_string.partition(":")[2].strip(), base=0)

    def compare_session_id_match(self, resultfile, applyfile):
        resultsid = 0
        applysid = 1

        r = open(resultfile, "r")
        for line in r.readlines():
            if ("SessionId:" in line):
                resultsid = self._ReturnSessionIdValue(line)
                break
        r.close()

        a = open(applyfile, "r")
        for line in a.readlines():
            if ("SessionId:" in line):
                applysid = self._ReturnSessionIdValue(line)
                break
        a.close()

        print("Apply Session Id: 0x%x" % applysid)
        print("Result Session Id: 0x%x" % resultsid)
        return resultsid == applysid

    def check_status(self, resultfile, code):
        a = open(resultfile, "r")
        for line in a.readlines():
            if ("Status:" in line):
                b = line.partition(":")[2]
                b = b.partition("(")[2]
                b = b.strip()
                b = b.rstrip(')')
                t = int(b, base=0)
                break
        a.close()
        print("Result Status: %s" % line)
        return t == int(code, base=0)

    def check_setting_status(self, resultfile, id, statuscode):
        xmlstring = ""
        found = False
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to XML string variable
        for line in a.readlines():
            if (found):
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from xml string
        r = None
        root = ElementTree.fromstring(xmlstring)
        for e in root.findall("./Settings/SettingResult"):
            i = e.find("Id")
            if (i.text == str(id)):
                r = e.find("Result")
                break
        if (r is None):
            print("Failed to find ID (%s) in the Xml results" % str(id))
            print(xmlstring)
            return False

        print("Result Status for Id (%s): %s" % (str(id), r.text))
        return int(r.text.strip(), base=0) == int(statuscode, base=0)

    def check_current_setting_value(self, resultfile, id, valuestring):
        xmlstring = ""
        found = False
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to xmlstring variable
        for line in a.readlines():
            if (found):
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from XML string
        r = None
        root = ElementTree.fromstring(xmlstring)
        for e in root.findall("./Settings/SettingCurrent"):
            i = e.find("Id")
            if (i.text == str(id)):
                r = e.find("Value")
                break
        if (r is None):
            print("Failed to find ID (%s) in the Xml results" % str(id))
            print(xmlstring)
            return False

        print("Result Value for Id (%s): %s" % (str(id), r.text))

        if r.text is None:
            if valuestring == '':
                return True
            else:
                return False
        return r.text.strip() == valuestring.strip()

    def get_list_of_settings(self, current_settings_xml_file):
        xmlstring = ""
        found = False
        a = open(current_settings_xml_file, "r")

        # find the start of the xml string and then copy all lines to xmlstring variable
        for line in a.readlines():
            if found:
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from xml string
        r = None
        list_of_settings = []

        root = ElementTree.fromstring(xmlstring)
        for e in root.findall("./Settings/SettingCurrent"):
            i = e.find("Id")
            if i is None:
                print("Malformed Settings XML.  Id element not found")
                print(xmlstring)
                return []

            r = e.find("Value")
            if r is None:
                print("Failed to find value for ID (%s)" % str(id))
                print(xmlstring)
                return False

            list_of_settings.append((i.text, r.text.strip()))

        return list_of_settings

    def get_current_permission_value(self, resultfile, id):
        xmlstring = ""
        found = False
        a = open(resultfile, "r")

        # find the start of the xml string and then copy all lines to xmlstring variable
        for line in a.readlines():
            if found:
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from xml string
        r1 = None
        r2 = None
        root = ElementTree.fromstring(xmlstring)

        for e in root.findall("./Permissions/PermissionCurrent"):
            i = e.find("Id")
            if (i.text == str(id)):
                j = e.find("PMask")
                if (j is not None):
                    r1 = j.text
                j = e.find("DMask")
                if (j is not None):
                    r2 = j.text
                break
        print("Result Value for Id (%s): PMask=%s, DMask=%s" % (str(id), r1, r2))
        return r1, r2

    def get_current_permission_defaults(self, resultfile):
        xmlstring = ""
        found = False
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to XML string variable
        for line in a.readlines():
            if found:
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from xml string
        root = ElementTree.fromstring(xmlstring)

        # Collect the root attributes
        try:
            r1 = root.attrib["Default"]
        except KeyError:
            r1 = None

        try:
            r2 = root.attrib["Delegated"]
        except KeyError:
            r2 = None

        print(f"Result Default Values for : Default={r1}, Delegated={r2}")
        return r1, r2

    #
    # Check all individual status codes for each setting and confirm it matches the input status code
    #
    def check_all_permission_status(self, resultfile, status):
        found = False
        xmlstring = ""
        statuscode = int(status, base=0)
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to XML string variable
        for line in a.readlines():
            if (found):
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from XML string
        r = None
        root = ElementTree.fromstring(xmlstring)
        rc = True
        for e in root.findall("./Permissions/PermissionResult"):
            i = e.find("Id")
            r = e.find("Result")

            if (r is None):
                print("Failed to find a result node for id (%s)" % i.text.strip())
                return False

            result = int(r.text.strip(), base=0)
            print("Result Status for Id (%s): %s" % (str(i.text.strip()), r.text))
            if (result != statuscode):
                print("Error.  Status Code for id (%s) didn't match expected" % i.text.strip())
                rc = False
        # done with loop
        return rc

    #
    # Check all individual status codes for each setting and confirm it matches the input status code
    #
    def check_all_setting_status(self, resultfile, status):
        found = False
        xmlstring = ""
        statuscode = int(status, base=0)
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to XML string variable
        for line in a.readlines():
            if (found):
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from XML string
        r = None
        root = ElementTree.fromstring(xmlstring)
        rc = True
        for e in root.findall("./Settings/SettingResult"):
            i = e.find("Id")
            r = e.find("Result")

            if (r is None):
                print("Failed to find a result node for id (%s)" % i.text.strip())
                return False

            result = int(r.text.strip(), base=0)
            print("Result Status for Id (%s): %s" % (str(i.text.strip()), r.text))
            if (result != statuscode):
                print("Error.  Status Code for id (%s) didn't match expected" % i.text.strip())
                rc = False
        # done with loop
        return rc

    #
    # Check list list of settings results
    #
    def check_setting_status_by_dictionary(self, resultfile, settingdict):
        found = False
        xmlstring = ""
        a = open(resultfile, "r")

        # find the start of the XML string and then copy all lines to XML string variable
        for line in a.readlines():
            if (found):
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return False

        # make an element tree from XML string
        r = None
        root = ElementTree.fromstring(xmlstring)
        rc = True
        for e in root.findall("./Settings/SettingResult"):
            i = e.find("Id")
            r = e.find("Result")

            if (r is None):
                print("Failed to find a result node for id (%s)" % i.text.strip())
                return False

            index = str(i.text.strip())
            result = int(r.text.strip(), base=0)
            print("Result Status for Id (%s): %s" % (str(i.text.strip()), r.text))
            if index in settingdict:
                if result != int(settingdict[index], base=0):
                    print("Error.  Status Code for id (%s) didn't match expected" % i.text.strip())
                    rc = False
            else:
                print("Error.  Index %s not in dictionary" % i.text.strip())
                rc = False
        # done with loop
        return rc

    def extract_payload_from_current(self, resultfile, payloadfile):
        try:
            tree = ElementTree.parse(resultfile)
            elem = tree.find('./SyncBody/Results/Item/Data')
        except Exception:
            elem = None

        if elem is None:
            return 0x8000000000000007  # EFI_DEVICE_ERROR

        tree = xml.dom.minidom.parseString(elem.text.encode())
        f = open(payloadfile, "wb")
        f.write(tree.toprettyxml().encode())
        f.close
        return 0

    def extract_results_packet(self, resultfile, resultpktfile):
        try:
            tree = ElementTree.parse(resultfile)
            elem = tree.find('./SyncBody/Results/Item/Data')
        except Exception:
            elem = None

        if elem is None:
            return (0x8000000000000007, 0)  # EFI_DEVICE_ERROR

        bindata = binascii.a2b_base64(elem.text)  # .decode('utf-16')
        f = BytesIO(bindata)

        g = open(resultpktfile, "wb")
        g.write(f.read())
        g.close()
        f.close()
        return 0

    def get_status_and_sessionid_from_identity_results(self, resultfile):
        f = open(resultfile, 'rb')
        rslt = CertProvisioningResultVariable(f)
        f.close()
        rslt.Print()
        return rslt.Status, rslt.SessionId

    def get_sessionid_from_identity_packet(self, identityfile):
        f = open(identityfile, 'rb')
        rslt = CertProvisioningApplyVariable(f)
        f.close()
        rslt.Print()
        return rslt.SessionId

    def get_status_and_sessionid_from_permission_results(self, resultfile):
        f = open(resultfile, 'rb')
        rslt = PermissionResultVariable(f)
        f.close()
        rslt.Print()
        return rslt.Status, rslt.SessionId

    def get_sessionid_from_permission_packet(self, identityfile):
        f = open(identityfile, 'rb')
        rslt = PermissionApplyVariable(f)
        f.close()
        rslt.Print()
        return rslt.SessionId

    def get_status_and_sessionid_from_settings_results(self, resultfile, checktype):
        f = open(resultfile, 'rb')
        rslt = SecureSettingsResultVariable(f)
        rslt.Print()
        f.close()
        if (checktype != "FULL") and (checktype != "BASIC"):
            print('checktype invalid')
            return (0x8000000000000007, 0)  # EFI_DEVICE_ERROR
        rslt_rc = rslt.Status
        if rslt_rc == 0 and checktype == "FULL":
            try:
                tree = ElementTree.fromstring(rslt.Payload)
                for elem in tree.findall('./Settings/SettingResult'):
                    rc = int(elem.find('Result').text, 0)
                    if rc != 0:
                        rslt_rc = rc
                    print('Setting %s - Code %s' % (elem.find('Id').text, elem.find('Result').text))
            except Exception:
                traceback.print_exc()
        return rslt_rc, rslt.SessionId

    def get_payload_from_permissions_results(self, resultfile, payloadfile):
        f = open(resultfile, 'rb')
        rslt = PermissionResultVariable(f)
        rslt.Print()
        f.close()
        if rslt.Payload is not None:
            f = open(payloadfile, "w")
            f.write(rslt.Payload)
            f.close
        return 0

    def get_payload_from_settings_results(self, resultfile, payloadfile):
        f = open(resultfile, 'rb')
        rslt = SecureSettingsResultVariable(f)
        rslt.Print()
        f.close()
        if rslt.Payload is not None:
            f = open(payloadfile, "w")
            f.write(rslt.Payload)
            f.close
        return 0

    def get_sessionid_from_settings_packet(self, settingsfile):
        f = open(settingsfile, 'rb')
        rslt = SecureSettingsApplyVariable(f)
        f.close()
        rslt.Print()
        return rslt.SessionId

    def get_status_from_dmtools_results(self, resultsfile):
        result = 0x8000000000000007  # EFI_DEVICE_ERROR
        try:
            tree = ElementTree.parse(resultsfile)
            root = tree.getroot()
            result = 0
            for e in root.findall("./SyncBody/Status"):
                i = e.find("Data")
                if (i.text != "200"):
                    result = 0x8000000000000007  # EFI_DEVICE_ERROR
                    break
                result = 0
        except Exception:
            traceback.print_exc()

        return result

    def get_uefistatus_string(self, status_code):
        print(type(status_code))

        isint = False
        if isinstance(status_code, int):
            isint = True

        if isint is True:
            ret = UefiStatusCode().Convert64BitToString(status_code)
        else:
            ret = UefiStatusCode().ConvertHexString64ToString(status_code)

        if ret == '':
            ret = '%x' % status_code
        return ret

    def print_xml_payload(self, xml_file_name):
        try:
            tree = xml.dom.minidom.parse(xml_file_name)
            print('%s' % tree.toprettyxml())
        except Exception:
            traceback.print_exc()
            print('Unable to print settings XML')

    def build_target_parameters(self, version, serial_number='', mfg='', prodname=''):
        rslt = []

        if version == 'V1':
            rslt.append('--HdrVersion')
            rslt.append('1')
            if serial_number != '':
                rslt.append('--SnTarget')
                rslt.append(serial_number)

        elif version == 'V2':
            rslt.append('--HdrVersion')
            rslt.append('2')
            if mfg != '':
                rslt.append('--SMBIOSMfg')
                rslt.append(mfg)
            if prodname != '':
                rslt.append('--SMBIOSProd')
                rslt.append(prodname)
            if serial_number != '':
                rslt.append('--SMBIOSSerial')
                rslt.append(serial_number)

        else:
            raise ValueError('Invalid version {}'.format(version))

        return rslt

    def get_device_ids(self, xml_file_name):
        d = {}
        try:
            tree = ElementTree.parse(xml_file_name)
            root = tree.getroot()

            elem = root.findall('./Identifiers/Identifier')
            for e in elem:
                xid = e.find('Id')
                pn = e.find('Value')
                # print(' {0} = {1}'.format(xid.text, pn.text)
                d[xid.text] = pn.text

            for key in d:
                print(' Key {0} has the value of {1}'.format(key,  d[key]))

        except Exception:
            traceback.print_exc()
            print('Unable to extract DeviceIdElements.')
            d = {}

        return d

    def get_device_id_element(self, id_xml_file, id):

        d = self.get_device_ids(id_xml_file)
        return d[id]

    def verify_device_id(self, xml_file_name, mfg, prodname, sn):

        d = self.get_device_ids(xml_file_name)

        manufacturer = d['Manufacturer']
        productname = d['Product Name']
        serialnumber = d['Serial Number']

        rc = 0
        if mfg != manufacturer:
            rc += 4
        if prodname != productname:
            rc += 8
        if sn != serialnumber:
            rc += 16

        return rc

    def get_dfci_version(self, xml_file_name):
        try:
            tree = ElementTree.parse(xml_file_name)
            root = tree.getroot()

            elem = root.find('./DfciVersion')
            d = elem.text
            print('Dfci Version detected as %s' % d)

        except Exception:
            traceback.print_exc()
            print('Unable to extract DfciVersion from %s' % xml_file_name)
            d = None

        return d

    def verify_dfci_version(self, xml_file_name, version):

        d = self.get_dfci_version(xml_file_name)

        rc = True
        if d != version:
            rc = False

        return rc

    def get_thumbprints(self, xml_file_name):
        d = {}
        try:
            tree = ElementTree.parse(xml_file_name)
            root = tree.getroot()

            elem = root.findall('./Certificates/Certificate')
            for e in elem:
                xid = e.find('Id')
                pn = e.find('Value')
                # print(' {0} = {1}'.format(xid.text, pn.text)
                d[xid.text] = pn.text

            for key in d:
                print(' Key {0} has the value of {1}'.format(key,  d[key]))

        except Exception:
            traceback.print_exc()
            print('Unable to extract DeviceIdElements.')
            d = {}

        return d

    def get_thumbprint_element(self, xml_file, id):
        d = self.get_thumbprints(xml_file)
        return d[id]

    #
    # Determine if the DUT is online by pinging it
    #
    def is_device_online(self, ipaddress):
        output = subprocess.Popen(["ping.exe", "-n", "1", "-w", "30", ipaddress], stdout=subprocess.PIPE).communicate()[0]

        if (b'TTL' in output):
            return True
        else:
            return False

    def get_signtool_path(self):
        global SignToolPath

        if SignToolPath is None:
            SignToolPath = FindToolInWinSdk("signtool.exe")

            # check if exists
            if SignToolPath is None or not os.path.exists(SignToolPath):
                raise Exception("Can't find signtool.exe on this machine.  Please install the Windows 10 WDK - "
                                "https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit")

        return SignToolPath

    def get_certmgr_path(self):
        global CertMgrPath
        if CertMgrPath is None:
            CertMgrPath = FindToolInWinSdk("certmgr.exe")

            # check if exists
            if CertMgrPath is None or not os.path.exists(CertMgrPath):
                raise Exception("Can't find certmgr.exe on this machine.  Please install the Windows 10 WDK - "
                                "https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit")

        return CertMgrPath

    def generate_master_lists(self, current_settings_file_name, current_permissions_file_name):
        owner_list = []
        user_list = []
        unsupported_list = []

        self.load_permission_database(current_permissions_file_name)
        try:
            xmlp = ElementTree.XMLParser(encoding="utf-8")
            tree = ElementTree.parse(current_settings_file_name, parser=xmlp)
            settings_database = tree.getroot()
        except Exception:
            traceback.print_exc()
            print('Unable to load settings database.')
            return False

        elem = settings_database.findall('./Settings/SettingCurrent')
        for e in elem:
            id = e.find("Id").text

            p_mask, d_mask, exact = self.query_permission_database(id)
            print(f"p_mask = {hex(p_mask)}, d_mask = {hex(d_mask)}, exact = {exact}, id={id}")

            owner_mask = int(128)  # 0x80
            user_mask = int(64)   # 0x40
            p_mask &= (owner_mask + user_mask)
            if p_mask == 0:  # no permission for either owner or user
                continue

            value = e.find("Value")
            if value is None:
                value = ''
            else:
                value = value.text

            type = e.find("Type")
            if type is None:
                raise Exception("Type not found in current settings.  System DFCI is downlevel.\n")
                continue

            type = type.text

            new_entry = (id, value, type)

            #
            # generate_master_lists generates list of standard settings that all behave the same.
            # The following are exceptions that have to be tested elsewhere:
            #     PASSWORD_TYPE   - There is no standard algorithm for password hashing, so
            #                       passwords need a platform specific test.
            #     CERT_TYPE       - Certificate types cannot be read by these tests.  The
            #                       current settings only show the hash.  There is one defined
            #                       setting with a CERT_TYPE, Dfci.HttpsCert.Binary.  This
            #                       setting is tested in the Enroll and Unenroll operations.
            #     CpuAndIoVirtualization - This is a special setting that, on some systems,
            #                       reports that it is enabled, but returns UNSUPPORTEDD when
            #                       there is attempt to disable this setting.  This setting
            #                       requires its own testcase.
            #
            if (
                  type in ['PASSWORD TYPE', 'CERT TYPE'] or
                  id == 'Dfci.CpuAndIoVirtualization.Enable' or
                  id == 'Device.CpuAndIoVirtualization.Enable'
                  ):
                unsupported_list.append(new_entry)
            else:
                if p_mask == (owner_mask + user_mask):
                    p_mask = random.choice([owner_mask, user_mask])
                if p_mask == owner_mask:
                    owner_list.append(new_entry)
                else:
                    user_list.append(new_entry)

        self.unload_permission_database()

        return (owner_list, user_list, unsupported_list)

    def _new_value(self, current_element):
        id, value, type = current_element

        if type == "ENABLE/DISABLE TYPE":
            if value == "Enabled":
                value = "Disabled"
            elif value == "Disabled":
                value = "Enabled"
            elif value == "Inconsistent":
                value = "Enabled"
            else:
                raise Exception(f"Unsupported value {value} for {type}")

        elif type == "SECURE BOOT KEY ENUM TYPE":
            if value == "MsOnly":
                value = "MsPlus3rdParty"
            elif value == "MsPlus3rdParty":
                value = "MsOnly"
            elif value == "None":
                value = "MsOnly"
            else:
                raise Exception(f"Unsupported value {value} for type {type}")

        elif type == "USB PORT STATE TYPE":
            if value == "UsbPortEnabled":
                value = "UsbPortHwDisabled"
            elif value == "UsbPortHwDisabled":
                value = "UsbPortEnabled"
            elif value == "UsbPortDataDisabled":
                value = "UsbPortEnabled"
            elif value == "UsbPortAuthenticated":
                value = "UsbPortHwDisabled"
            else:
                raise Exception(f"Unsupported value {value} for type {type}")

        elif type == "STRING TYPE":
            if value == "Some test value":
                value = ""
            else:
                value = "Some test value"

        elif type == "BINARY TYPE":
            if value == "55 aa 55":
                value = 'aa 55 aa'
            else:
                value = "55 aa 55"

        elif type == "PASSWORD TYPE":
            value = None

        elif type == "CERT TYPE":
            value = None

        else:
            raise Exception(f"Unsupported type {type}")

        return (id, value, type)

    def generate_settings_lists(self, master_list, setting1_list_file_name, setting2_list_file_name,
                                setting3_list_file_name, restore_list_file_name):
        from SettingsXMLLib import SettingsXMLLib

        master_list_copy = master_list.copy()

        check_count = len(master_list)

        setting1_list = []
        setting2_list = []
        setting3_list = []
        restore_list = []

        count = int(len(master_list_copy) / 3)
        print(f"Length of master_list={len(master_list_copy)}, count={count}")
        for i in range(count):
            chosen_element = random.choice(master_list_copy)
            new_value = self._new_value(chosen_element)
            setting1_list.append(new_value)
            restore_list.append(chosen_element)
            master_list_copy.remove(chosen_element)

        print(f"Length of master_list={len(master_list_copy)}")
        for i in range(count):
            chosen_element = random.choice(master_list_copy)
            new_value = self._new_value(chosen_element)
            setting2_list.append(new_value)
            restore_list.append(chosen_element)
            master_list_copy.remove(chosen_element)

        print(f"Length of master_list={len(master_list_copy)}")
        for chosen_element in master_list_copy:
            new_value = self._new_value(chosen_element)
            setting3_list.append(new_value)
            restore_list.append(chosen_element)

        print(f"setting1_list({len(setting1_list)}) = {setting1_list}")
        print(f"setting2_list({len(setting2_list)}) = {setting2_list}")
        print(f"setting3_list({len(setting3_list)}) = {setting3_list}")
        print(f"restore_list({len(restore_list)}) = {restore_list}")

        a = SettingsXMLLib()
        a.create_settings_xml(setting1_list_file_name, '2', '2', setting1_list)
        a.create_settings_xml(setting2_list_file_name, '2', '2', setting2_list)
        a.create_settings_xml(setting3_list_file_name, '2', '2', setting3_list)
        a.create_settings_xml(restore_list_file_name, '2', '2', restore_list)

        if len(restore_list) != check_count:
            raise Exception("Restore list does not match master_list")

    def query_permission_database(self, setting):
        global permission_database

        if permission_database is None:
            raise Exception("Permission database not loaded")

        # Collect the root attributes
        try:
            p_mask = permission_database.attrib["Default"]
        except KeyError:
            p_mask = '0'

        try:
            d_mask = permission_database.attrib["Delegated"]
        except KeyError:
            d_mask = '0'

        exact = False
        elem = permission_database.findall('./Permissions/PermissionCurrent')
        for e in elem:
            i = e.find("Id")
            if (i.text == str(setting)):
                j = e.find("PMask")
                if (j is not None):
                    p_mask = j.text
                j = e.find("DMask")
                if (j is not None):
                    d_mask = j.text
                exact = True
                break

        return int(p_mask), int(d_mask), exact

    def load_permission_database(self, xml_filename):
        global permission_database

        try:
            xmlp = ElementTree.XMLParser(encoding="utf-8")
            tree = ElementTree.parse(xml_filename, parser=xmlp)
            permission_database = tree.getroot()
            return True
        except Exception:
            traceback.print_exc()
            print('Unable to load permission database.')
            return False

    def unload_permission_database(self):
        global permission_database

        permission_database = None

    def generate_standard_test_coverage(self, tested_settings_filename1, tested_settings_filename2, std_settings_filename):

        not_tested = []
        try:
            xmlp = ElementTree.XMLParser(encoding="utf-8")
            tree = ElementTree.parse(tested_settings_filename1, parser=xmlp)
            tested_settings_database1 = tree.getroot()
            xmlp2 = ElementTree.XMLParser(encoding="utf-8")
            tree2 = ElementTree.parse(tested_settings_filename2, parser=xmlp2)
            tested_settings_database2 = tree2.getroot()
        except Exception:
            traceback.print_exc()
            print('Unable to load tested settings database.')
            return None

        my_namespaces = dict([node for _, node in ElementTree.iterparse(tested_settings_filename1, events=['start-ns'])])

        with open(std_settings_filename) as f:
            std_settings_list = f.readlines()

        for line in std_settings_list:
            line = line.strip()
            if line == 'Dfci.CpuAndIoVirtualization.Enable':
                continue
            search = './/*[Id="' + line + '"]'
            result = tested_settings_database1.find(search, my_namespaces)
            if result is None:
                result = tested_settings_database2.find(search, my_namespaces)
                if result is None:
                    not_tested.append(line.strip())

        return not_tested

    def get_base_name_from_path(self, file_path_name):
        return os.path.basename(file_path_name)

    def file_exists(self, file_path_name):
        return os.path.isfile(file_path_name)

    def get_dut_status(self, host_ip='0.0.0.0', status_port=8271):
        print("socket.socket")
        c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        print("c.settimeout(30)")
        c.settimeout(30)
        timeout = False
        start_time = time.time()
        try:
            print(f"Connecting to {host_ip}:{status_port}")
            c.connect((host_ip, status_port))
            print(f"Connected to {host_ip}:{status_port}")
        except socket.error as e:
            print(f'Connection error: {e}')
            timeout = True

        end_time = time.time()
        print(f'Connect time was {end_time - start_time}')

        data = 'Continue'
        if not timeout:
            # message to server
            message = "Request Status"
            try:
                c.sendall(message.encode())
                end_time = time.time()
                print(f'Sendall time was {end_time - start_time}')
                data = c.recv(1024).decode()
            except socket.error as e:
                print(f'Receive error: {e}')
                timeout = True

            if not timeout:
                print(f'Data from server is: {data}')
        end_time = time.time()
        print(f'Elapsed time was {end_time - start_time}')
        print("c.close()")

        # close the connection
        c.close()
        return data
