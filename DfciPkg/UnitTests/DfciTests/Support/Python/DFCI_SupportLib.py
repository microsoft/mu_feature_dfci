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
import re
import socket
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

DfciTest_Template = 'DfciTests.Template'
DfciTest_Config = 'DfciTests.ini'

class DFCI_SupportLib(object):
    _cert_mgr_path = None
    _current_config = None
    _sign_tool_path = None

    def get_test_config(self):
        if self._current_config is not None:
            return self._current_config

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
        if config['DfciConfig'] is None or config['DfciConfig']['version'] is None:
            raise Exception('Unable to verify DfciTest.Template version.')

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

        if not os.path.exists(ini_path):
            raise Exception(f'Unable to locate test configuration({ini_path}).')

        config.read(ini_path)
        if config['DfciConfig'] is None or config['DfciConfig']['version'] is None:
            raise Exception('Unable to verify DfciTest.ini version.')

        current_ver = int(config["DfciConfig"]["version"])
        if current_ver != template_ver:
            raise Exception(f'{ini_path} version is out of context with {template_path}).')

        if current_ver != DfciTest_Version:
            raise Exception(f'{ini_path} version should be {DfciTest_Version}).')

        if config['DfciTest']['server_host_name'] is None:
            raise Exception(f'Version 1 requires server_host_name in {ini_path}.')

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

    def _return_session_id_value(self, input_string):
        # Session Id:       0xF08A4
        return int(input_string.partition(":")[2].strip(), base=0)

    def compare_session_id_match(self, resultfile, applyfile):
        resultsid = 0
        applysid = 1

        r = open(resultfile, "r")
        for line in r.readlines():
            if ("SessionId:" in line):
                resultsid = self._return_session_id_value(line)
                break
        r.close()

        a = open(applyfile, "r")
        for line in a.readlines():
            if ("SessionId:" in line):
                applysid = self._return_session_id_value(line)
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

    def get_signtool_path(self):
        if self._sign_tool_path is None:
            self._sign_tool_path = FindToolInWinSdk("signtool.exe")

            # check if exists
            if self._sign_tool_path is None or not os.path.exists(self._sign_tool_path):
                raise Exception("Can't find signtool.exe on this machine.  Please install the Windows 10 WDK - "
                                "https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit")

        return self._sign_tool_path

    def get_certmgr_path(self):
        if self._cert_mgr_path is None:
            self._cert_mgr_path = FindToolInWinSdk("certmgr.exe")

            # check if exists
            if self._cert_mgr_path is None or not os.path.exists(self._cert_mgr_path):
                raise Exception("Can't find certmgr.exe on this machine.  Please install the Windows 10 WDK - "
                                "https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit")

        return self._cert_mgr_path
