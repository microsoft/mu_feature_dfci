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


def main():

    parser = argparse.ArgumentParser(description="Parse DfciSettings.h to produce the current list of DFCI settings")

    parser.add_argument("-v", "--Verbose", dest="Verbose", action="store_true", default=False,
                        help="set debug logging level")
    parser.add_argument("-i", dest="InputFile", default='..\\..\\..\\..\\Include\\Settings\\DfciSettings.h',
                        help="Create an output log file: ie: -l out.txt")
    parser.add_argument("-o", dest="OutputFile", default='DfciStdSettings.list',
                        help="Create an output log file: ie: -l out.txt")

    options = parser.parse_args()

    with open(options.OutputFile, "w") as outfile:
        with open(options.InputFile, "r") as infile:
            for line in infile.readlines():
                parts = line.split(" ", 4)
                if parts is None:
                    continue
                if type(parts) is not list:
                    continue
                if len(parts) < 4:
                    continue
                if parts[0] != '#define':
                    continue
                if parts[3] is None:
                    continue
                setting = parts[3].replace('"', '')
                setting = setting.strip()
                outfile.write(setting)
                outfile.write("\n")
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
