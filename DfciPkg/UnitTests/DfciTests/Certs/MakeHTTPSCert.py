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
import traceback
import pathlib


def check_for_files(filemask):
    file_list = glob.glob(filemask)
    return len(file_list) != 0


def delete_files(filemask):
    file_list = glob.glob(filemask)
    # Iterate over the list of file paths & remove each file.
    for file_path in file_list:
        os.remove(file_path)


def generate_httpreq_cnf(config):
    base_cnf = [
        '# @file',
        '#',
        '# httpreq.cnf',
        '#',
        '# Create HTTPS certificate with Subject Alternative Names',
        '#',
        '# Copyright (c), Microsoft Corporation',
        '# SPDX-License-Identifier: BSD-2-Clause-Patent',
        '##',
        '',
        '[req]',
        'string_mask = nombstr',
        'distinguished_name = req_distinguished_name',
        'x509_extensions = v3_req',
        'prompt = no',
        '',
        '[req_distinguished_name]',
        'C = US',
        'ST = WA',
        'L = Redmond',
        'O = Dfci Testing',
        'OU = DfciRecoveryTest',
        'CN = Dfci Test Shop',
        '',
        '[v3_req]',
        'keyUsage = digitalSignature, keyEncipherment',
        'subjectKeyIdentifier=hash',
        'subjectAltName = @alt_names',
        'basicConstraints=critical, CA:FALSE, pathlen:0',
        'extendedKeyUsage = serverAuth',
        'authorityKeyIdentifier = keyid',
        '',
        '[alt_names]',
        ]

    hostname = config['DfciTest']['server_host_name']
    with open('httpreq.cnf', 'w') as config_file:
        for line in base_cnf:
            config_file.write(line + os.linesep)

        config_file.write('DNS.1 = localhost' + os.linesep)
        config_file.write(f'DNS.2 = {hostname}' + os.linesep)
        config_file.close()


def run_makecert(name):

    #
    # Assume openssl.exe is on the path as Git FOr Windows will put it there, and Git For Windows should
    # have been installed.  See DfciPkg\UnitTests\DfciTests\readme.md, section Setting up the HOST system,
    # step 6.
    #
    openssl_path = 'openssl.exe'

    # openssl req -x509 -nodes -sha256 -days 3652  -newkey rsa:2048 -keyout server.key -out server.cert -config httpreq.cnf
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

    # openssl.exe pkcs12 -nodes -inkey DFCI_HTTPS.key -in DFCI_HTTPS.pem -export -out DFCI_HTTPS.pfx
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

    # openssl.exe x509 -in DFCI_HTTPS.pem -outform der -out DFCI_HTTPS.cer
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

    modpath = pathlib.Path(__file__)
    basename = os.path.basename(modpath)
    modpath = modpath.parent
    modpath = os.path.realpath(modpath)

    cwdpath = os.getcwd()
    cwdpath = os.path.realpath(cwdpath)

    if cwdpath != modpath:
        raise Exception("You must run " + basename + " from the DfciTest\\Certs directory\n")

    sys.path.append(r'..\Support\Python')
    from DFCI_SupportLib import DFCI_SupportLib
    config = DFCI_SupportLib().get_test_config()

    https_files_exist = check_for_files('DFCI_HTTPS.*')

    if https_files_exist:
        delete_files('DFCI_HTTPS.*')

    rc = 0

    generate_httpreq_cnf(config)

    try:
        run_makecert('DFCI_HTTPS')
        run_makecert('DFCI_HTTPS2')

    except KeyboardInterrupt:
        pass

    except Exception:
        traceback.print_exc()

    delete_files('httpreq.cnf')
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
