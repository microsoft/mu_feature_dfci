==================================
Project Mu Feature Dfci Repository
==================================

============================= ================= ===================
 Host Type & Toolchain        Build Status      Code Coverage
============================= ================= ===================
Windows_VS2022_               |WindowsCiBuild|  |WindowsCiCoverage|
Ubuntu_GCC5_                  |UbuntuCiBuild|   |UbuntuCiCoverage|
============================= ================= ===================

This repository is part of Project Mu.  Please see Project Mu for details https://microsoft.github.io/mu

Branch Status - main
==============================

:Status:
  In Development

:Entered Development:
  Nov 2022

:Anticipated Stabilization:
  Mar 2023

Branch Changes - main
===============================

Breaking Changes-dev
--------------------

- None

Main Changes-dev
----------------

tag "From_mu_plus"

  New Repo from mu_plus branch release/202208 at commit: c69447e15f2b968abd5901c05d0c622650f10f89

    ran:

      git filter-repo --path DfciPkg/ --path ZeroTouchPkg/ --path LICENSE.txt
                      --path pull_request_template.md --path pip-requirements.txt
                      --path .gitignore

    tag "From_mu_plus" is the latest commit from mu_plus copied to mu_feature_dfci using the above
    git filter-repo command.

Bug Fixes-dev
-------------

- None

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

.. |WindowsCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20WIndows%20VS?repoName=microsoft%2Fmu_feature_dfci&branchName=main
   :target: https://dev.azure.com/projectmu/mu/_build?definitionId=142&_a=summary

.. _Windows_VS2022: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=142&repoName=microsoft%2Fmu_feature_dfci&branchName=main

.. |WindowsCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue

.. _Ubuntu_GCC5: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=139&repoName=microsoft%2Fmu_feature_dfci&branchName=main

.. |UbuntuCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20GCC5?repoName=microsoft%2Fmu_feature_dfci&branchName=main
  :target: https://dev.azure.com/projectmu/mu/_build?definitionId=141&_a=summary

.. |UbuntuCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue
