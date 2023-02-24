# @file GenerateStdSettingList.py
#
#
# Process STD setting include files and generate a settings list
#
#
import argparse
import logging
import sys
import traceback

import xml.etree.ElementTree as ET

def main():


    try:
        xmlp = ET.XMLParser(encoding="utf-8")
        tree = ET.parse('Restore.xml', parser=xmlp)
        tested_settings_database = tree.getroot()
    except:
        traceback.print_exc()
        print('Unable to load tested settings database.')
        return None

    with open('DfciStdSettings.list') as f:
        std_settings_list = f.readlines()

    # for line in std_settings_list:
    search = ".//*[Id='Dfci.CpuAndIoVirtualization.Enable']"

    my_namespaces = dict([node for _, node in ET.iterparse('Restore.xml', events=['start-ns'])])

    print(my_namespaces)
    result = tested_settings_database.find(search, my_namespaces)
    if result is not None:
        print ("Match")
    else:
        print("No match")
    search = ".//*[Id='Dfci4.CpuAndIoVirtualization.Enable']"
    result = tested_settings_database.find(search, my_namespaces)
    if result is not None:
        print ("Match")
    else:
        print("No match")
    return 0


# --------------------------------------------------------------------------- #
#
#   Entry point
#
# --------------------------------------------------------------------------- #
if __name__ == '__main__':
    # setup main console as logger

    print("Generate Std Settings List V1.0")

    retcode = 0
    try:
        # Run main application
        retcode = main()

    except Exception:
        logging.exception("Exception occurred")
        traceback.print_exc()
        retcode = 8

    if retcode != 0:
        logging.critical(f"Failed.  Return Code: {retcode}")

    # end logging
    logging.shutdown()
    sys.exit(retcode)
