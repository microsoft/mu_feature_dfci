# @file
#
# Script to Generate the DFCI_HTTPS certificate for Refresh From Network.
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import argparse
import glob
import logging
import os
import subprocess
import sys
import time
import traceback


def check_for_files(filemask):
    file_list = glob.glob(filemask)
    return len(file_list) != 0


def delete_cert_files(filemask):
    file_list = glob.glob(filemask)
    # Iterate over the list of file paths & remove each file.
    for file_path in file_list:
        os.remove(file_path)


def run_makecert(name):

    openssl_path = 'openssl.exe'

    command1 = [
        openssl_path,
        'req',
        '-x509',
        '-nodes',
        '-sha256',
        '-days', '3652',
        '-newkey', 'rsa:2048',
        '-keyout', f'{name}.key',
        '-out', f'{name}.pem',
        '-config', 'httpreq.cnf'
        ]


 # openssl req -x509 -nodes -sha256 -days 3652  -newkey rsa:2048 -keyout server.key -out server.cert -config httpreq.cnf


    command2 = [
        openssl_path,
        'pkcs12',
        '-nodes',
        '-inkey', f'{name}.key',
        '-in', f'{name}.pem',
        '-export',
        '-out', f'{name}.pfx',
        '-passout',  'pass:'
        ]

    command3 = [
        openssl_path,
        'x509',
        '-in', f'{name}.pem',
        '-outform', 'der',
        '-out', f'{name}.cer'
        ]

    print(f'Making cert {name}')

    logging.info(f'Running {command1}')
    output = subprocess.run(command1,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if output.returncode != 0:
        raise Exception(f'Error running openssl_1:{output.stdout}')

    logging.info(f'Running {command2}')
    output = subprocess.run(command2,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if output.returncode != 0:
        raise Exception(f'Error running openssl_2:{output.stdout}')


    logging.info(f'Running {command3}')
    output = subprocess.run(command3,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if output.returncode != 0:
        raise Exception(f'Error running openssl_3:{output.stdout}')

    return 0


def main(console):

    parser = argparse.ArgumentParser(description='Make HTTPS Test certificates')

    parser.add_argument('-v', '--verbose', action='store_true', dest='verbose', help='Print verbose messages', default=False)
    options = parser.parse_args()

    if options.verbose:
        console.setLevel(logging.INFO)

    https_files_exist = check_for_files('DFCI_HTTPS.*')

    if https_files_exist:
        delete_cert_files('DFCI_HTTPS.*')

    rc = 0

    try:
        run_makecert('DFCI_HTTPS')
        run_makecert('DFCI_HTTPS2')

    except KeyboardInterrupt:
        pass

    except Exception:
        traceback.print_exc()

    return rc


if __name__ == '__main__':
    # setup main console as logger
    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)
    console.setFormatter(formatter)
    logger.addHandler(console)

    # call main worker function
    retcode = main(console)

    if retcode != 0:
        logging.critical("Failed.  Return Code: %i" % retcode)

    # end logging
    logging.shutdown()
    sys.exit(retcode)
