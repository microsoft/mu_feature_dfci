==================================
Project Mu Feature Dfci Repository
==================================

============================= ================= =============== ===================
 Host Type & Toolchain        Build Status      Test Status     Code Coverage
============================= ================= =============== ===================
Windows_VS2022_               |WindowsCiBuild|  |WindowsCiTest| |WindowsCiCoverage|
Ubuntu_GCC5_                  |UbuntuCiBuild|   |UbuntuCiTest|  |UbuntuCiCoverage|
============================= ================= =============== ===================

This repository is part of Project Mu.  Please see Project Mu for details https://microsoft.github.io/mu

Branch Status - main
==============================

:Status:
  In Development

:Entered Development:
  Nov 2022

:Anticipated Stabilization:
  Mar 2023

Branch Changes - release/202208
===============================

Breaking Changes-dev
--------------------

- None

Main Changes-dev
----------------

Bug Fixes-dev
-------------

New Repo from mu_plus branch release/202208
--------------------

mu_plus at Commit: c90b62452dd00af4d835d1d79dd2dd4f735cd3fd

ran:

git filter-repo --path DfciPkg/ --path ZeroTouchPkg/ --path LICENSE.txt
                --path pull_request_template.md --path pip-requirements.txt
                --path .gitignore


Code of Conduct
===============

This project has adopted the Microsoft Open Source Code of Conduct https://opensource.microsoft.com/codeofconduct/

For more information see the Code of Conduct FAQ https://opensource.microsoft.com/codeofconduct/faq/
or contact `opencode@microsoft.com <mailto:opencode@microsoft.com>`_. with any additional questions or comments.

Contributions
=============

Contributions are always welcome and encouraged!
Please open any issues in the Project Mu GitHub tracker and read https://microsoft.github.io/mu/How/contributing/


Copyright & License
===================

| Copyright (C) Microsoft Corporation
| SPDX-License-Identifier: BSD-2-Clause-Patent

.. ===================================================================
.. This is a bunch of directives to make the README file more readable
.. ===================================================================

.. CoreCI

.. _Windows_VS2022: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20WIndows%20VS?repoName=microsoft%2Fmu_feature_dfci&branchName=main
.. |WindowsCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20WIndows%20VS?repoName=microsoft%2Fmu_feature_dfci&branchName=main
.. |WindowsCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue


.. _Ubuntu_GCC5: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=139&repoName=microsoft%2Fmu_feature_dfci&branchName=main
.. |UbuntuCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20GCC5?repoName=microsoft%2Fmu_feature_dfci&branchName=main
.. |UbuntuCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue