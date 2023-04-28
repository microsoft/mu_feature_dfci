# @file
#
# PyRobotRemote - Runs on the System Under Test (DUT) providing
#                 functionality needed for DFCI testing
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import os
import glob
import json
import subprocess
import logging
import socketserver
import sys
import threading
import time
import traceback
import win32api
import win32con
import win32security
import winnt

from Lib.UefiVariablesSupportLib import UefiVariable

# update this whenever you make a change
RobotRemoteChangeDate = "2023-04-15 09:00"
RobotRemoteVersion = 1.08

HOST = '0.0.0.0'
ROBOT_PORT1 = 8270
ROBOT_PORT2 = 8271
STAGED_ACTIONS_FILE = 'staged_actions.json'

_ExitFlag = False
_PyRobotResponse = 'No Status'


class UefiRemoteTesting(object):
    """Library to be used with Robot Framework's remote server.

    This supports Robot Framework Remote Interface.  This lets a remote test system perform local operations using
    remote framework and built in remote "Keywords"
    """
    def __init__(self):
        self.filepath = None
        self.lines = []
        self.reboot_complete = True

    def Run_PowerShell_And_Return_Output(self, cmdline):
        completed = subprocess.run(["powershell", "-Command", cmdline], capture_output=True)

        if completed.returncode != 0:
            return "Error"
        else:
            return completed.stdout.decode('utf-8').strip()

    #
    # String variables are designed to have a NULL.  This does
    # confuse Python, so get rid of the NULL when it is expected
    #
    def GetUefiVariable(self, name, guid, trim):
        uefi_var = UefiVariable()
        logging.info("Calling GetUefiVar(name='%s', GUID='%s')" % (name, "{%s}" % guid))
        (rc, var, errorstring) = uefi_var.GetUefiVar(name, guid)
        var2 = var
        if (var is not None) and (trim == 'trim'):
            varlen = len(var)
            if varlen > 1:
                var2 = var[0:varlen-1]
        return (rc, var2, errorstring)

    def SetUefiVariable(self, name, guid, attrs=None, contents=None):
        uefi_var = UefiVariable()
        (rc, err, errorstring) = uefi_var.SetUefiVar(name, guid, contents, attrs)
        return rc

    def ReadStagedFile(self, staged_file_name):
        try:
            staged_file_name = os.path.join(os.getcwd(), staged_file_name)
            with open(staged_file_name, "rb") as fp:
                data = fp.read()
            return ('1', data)
        except Exception as e:
            return ('0', str(e))

    def WriteStagedFile(self, staged_file_name, data):
        try:
            staged_file_name = os.path.join(os.getcwd(), staged_file_name)
            with open(staged_file_name, "wb") as fp:
                fp.write(data)
            return ('1', None)
        except Exception as e:
            return ('0', str(e))

    def write_staged_action(self, cmd, arg1=None, arg2=None, arg3=None, arg4=None):
        staged_actions = []
        staged_actions_file = os.path.join(os.getcwd(), STAGED_ACTIONS_FILE)

        if cmd == "ClearStagedFiles":
            if os.path.exists(staged_actions_file):
                try:
                    os.remove(staged_actions_file)
                    print(f"Deleted {staged_actions_file}")
                except OSError:
                    print(f"Unable to remove {staged_actions_file}")

            bin_files = os.path.join(os.getcwd(), "*.bin")
            for file in glob.glob(bin_files):
                try:
                    os.remove(file)
                    print(f"Deleted {file}")
                except OSError:
                    print(f"Unable to remove {file}")

            xml_files = os.path.join(os.getcwd(), "*.xml")
            for file in glob.glob(xml_files):
                try:
                    os.remove(file)
                    print(f"Deleted {file}")
                except OSError:
                    print(f"Unable to remove {file}")
            return

        if os.path.exists(staged_actions_file):
            with open(staged_actions_file, 'r') as fp:
                staged_actions = json.load(fp)

            if type(staged_actions) is not list:
                os.remove(staged_actions_file)

        staged_actions.append([cmd, arg1, arg2, arg3, arg4])
        with open(staged_actions_file, 'w') as fp:
            json.dump(staged_actions, fp)
            fp.truncate()

    def remote_ack(self):
        return True

    def is_reboot_complete(self):
        return self.reboot_complete

    def remote_get_version(self):
        return RobotRemoteVersion

    def remote_warm_reboot(self):
        self.reboot_complete = False
        os.system("shutdown -r -t 1")

    def remote_reboot_to_firmware(self):
        token_handle = win32security.OpenProcessToken(win32api.GetCurrentProcess(),
                                                      win32con.TOKEN_ADJUST_PRIVILEGES | win32con.TOKEN_QUERY)
        new_privilege = [(win32security.LookupPrivilegeValue(None, winnt.SE_SHUTDOWN_NAME),
                          winnt.SE_PRIVILEGE_ENABLED)]
        win32security.AdjustTokenPrivileges(token_handle, False, new_privilege)
        os.system("shutdown -r -fw -t 0")


class RobotHandleTcpServer(socketserver.BaseRequestHandler):
    def handle(self):
        global _ExitFlag
        global _PyRobotResponse

        #
        # this function runs on another thread
        #
        # establish connection with test cases on robot client
        data = self.request.recv(1024).decode().strip()
        addr = self.client_address
        print(f'Message from: {addr[0]}:{addr[1]} : {data}, response = {_PyRobotResponse}')

        self.request.sendall(_PyRobotResponse.encode())
        _ExitFlag = True


class RobotRemoteServer2():
    _Tcp_Server = None

    def _tcp_server_thread(self):
        self._Tcp_Server = socketserver.TCPServer((HOST, ROBOT_PORT2), RobotHandleTcpServer)
        self._Tcp_Server.serve_forever()

    def send_response_status(self, status):
        global _ExitFlag
        global _PyRobotResponse

        _PyRobotResponse = status

        t = threading.Thread(target=self._tcp_server_thread)
        t.start()

        try:
            print(f"Waiting for connection to robot client testcases on {HOST}:{ROBOT_PORT2}")
            count = 20  # wait for 10 seconds to client to respond
            while t.is_alive():
                t.join(0.5)
                count = count - 1
                if _ExitFlag or count == 0:
                    self._Tcp_Server.shutdown()
                    _ExitFlag = False
                    count = 0
        except KeyboardInterrupt:
            pass
        except Exception:
            traceback.print_exc()


def _print_error(msg):
    print('EEEEEE  RRRRR   RRRRR    OOOOO   RRRRR ')
    print('E       R    R  R    R  O     O  R    R')
    print('E       R    R  R    R  O     O  R    R')
    print('EEEE    RRRRR   RRRRR   O     O  RRRRR ')
    print('E       R  R    R  R    O     O  R  R  ')
    print('E       R   R   R   R   O     O  R   R ')
    print('EEEEEE  R    R  R    R   OOOOO   R    R')
    print('\nStaged actions terminated\n')
    print(msg)
    print("\n")
    print("Continuing with Robot Remote Server\n")


def _set_uefi_variable_from_file(variable_name, variable_guid, varialbe_attributes, data_file_name):
    with open(data_file_name, "rb") as fp:
        variable_data = fp.read()
        u = UefiRemoteTesting()
        u.SetUefiVariable(variable_name, variable_guid, attrs=varialbe_attributes, contents=variable_data)


def _read_uefi_variable_to_file(variable_name, variable_guid, data_file_name):
    u = UefiRemoteTesting()
    rc, var, errorstring = u.GetUefiVariable(variable_name, variable_guid, 'None')
    if rc != 0:
        _print_error(f"Unable to process GetVariable for {variable_name}, error={errorstring}\n")
        return
    with open(data_file_name, "wb") as fp:
        fp.write(var)
        fp.truncate()


def send_response(status):
    robotremotererver2 = RobotRemoteServer2()
    robotremotererver2.send_response_status(status)
    del robotremotererver2


def process_staged_actions():

    #
    # This function will process a list of things to do.
    #

    staged_actions_file = os.path.join(os.getcwd(), STAGED_ACTIONS_FILE)

    staged_actions = []

    try:
        with open(staged_actions_file, "r") as fp:
            staged_actions = json.load(fp)
        os.remove(staged_actions_file)
    except FileNotFoundError:
        print("No staged actions to process")
        return

    if not isinstance(staged_actions, list):
        _print_error("Staged actions is not a list.")
        send_response('Continue')
        return

    staged_actions_copy = staged_actions.copy()
    if len(staged_actions_copy) == 0:
        print("No staged actions left")
        send_response('Continue')
        return

    for element in staged_actions_copy:
        if len(element) != 5:
            _print_error(f"Invalid number of elements({len(element)}) in a staged action")
            send_response('Continue')
            return

        print(f"Processing staged action {element[0]} {element[1]} {element[2]} {element[3]} {element[4]}")
        staged_actions.remove(element)

        if element[0] == 'GetVariable':
            _read_uefi_variable_to_file(element[1], element[2], element[3])

        elif element[0] == 'SetVariable':
            _set_uefi_variable_from_file(element[1], element[2], element[3], element[4])

        elif element[0] == 'ResetSystem':
            send_response(element[0])
            with open(staged_actions_file, "w") as fp:
                json.dump(staged_actions, fp)
                fp.truncate()

            time.sleep(2)
            os.system("shutdown -r -t 1")
            while True:
                time.sleep(10)
                print('Waiting for restart')

        else:
            _print_error(f"Error processing staged actions. Invalid action {element[0]}")
            send_response('Continue')
            return

    if len(staged_actions) != 0:
        _print_error(f"Staged actions not empty\nStaged actions = {staged_actions}")

    send_response('Continue')

    return


def pre_robot_framework_notifications():
    try:
        process_staged_actions()
    except KeyboardInterrupt:
        pass
    except Exception as e:
        traceback.print_exc()
        _print_error(e)


if __name__ == '__main__':
    from robotremoteserver import RobotRemoteServer
    print(f'PyRobotRemote Version {RobotRemoteVersion} - {RobotRemoteChangeDate}')

    # setup main console as logger
    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(levelname)s - %(message)s')
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)
    console.setFormatter(formatter)
    logger.addHandler(console)

    # Display IP address for convenience of tester
    os.system('ipconfig | findstr IPv4')

    pre_robot_framework_notifications()

    RobotRemoteServer(UefiRemoteTesting(), host=HOST, port=ROBOT_PORT1)

    logging.shutdown()
    sys.exit(0)
