==================================
Project Mu Dfci Feature Repository
==================================

============================= ================= ===================
 Host Type & Toolchain        Build Status      Code Coverage
============================= ================= ===================
Windows_VS_                   |WindowsCiBuild|  |WindowsCiCoverage|
Ubuntu_GCC5_                  |UbuntuCiBuild|   |UbuntuCiCoverage|
============================= ================= ===================

This repository contains the Dfci feature portion of Project Mu.  For documentation on Dfci,
see `Device Firmware Configuration Interface <https://microsoft.github.io/mu/dyn/mu_feature_dfci/DfciPkg/Docs/Dfci_Feature/>`_.

Please see `Project Mu <https://microsoft.github.io/mu>`_ for general infomation on Project Mu.


Consuming the Dfci Feature Package
==================================
This project is intended to be included in a platform as a submodule at a particular release tag.


Releases
==============================

Releases of this repository will follow the
`Nuget versioning model <https://docs.microsoft.com/en-us/nuget/concepts/package-versioning>`_.


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

This project has adopted the Microsoft Open Source.
See the `Code of Conduct <https://opensource.microsoft.com/codeofconduct/>`_.
For more information see the Code of `Conduct FAQ <https://opensource.microsoft.com/codeofconduct/faq/>`_ or
 contact `opencode@microsoft.com <mailto:opencode@microsoft.com>`_ with any additional questions or comments.

Issues
======

Please open any issues in the Project Mu GitHub tracker.
`More Details <https://microsoft.github.io/mu/How/contributing/>`_

Contributions
=============

Contributions are always welcome and encouraged!

Please follow the general Project Mu Pull Request process.  `More
Details <https://microsoft.github.io/mu/How/contributing/>`_

* `Code Requirements <https://microsoft.github.io/mu/CodeDevelopment/requirements/>`_
* `Doc Requirements <https://microsoft.github.io/mu/DeveloperDocs/requirements/>`_

Builds
======

Please follow the steps in the Project Mu docs to build for CI and local
testing. `More Details <https://microsoft.github.io/mu/CodeDevelopment/compile/>`_

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

.. _Windows_VS: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=142&repoName=microsoft%2Fmu_feature_dfci&branchName=main

.. |WindowsCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue

.. _Ubuntu_GCC5: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=139&repoName=microsoft%2Fmu_feature_dfci&branchName=main

.. |UbuntuCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status/CI/Feature%20DFCI/Mu%20Feature%20DFCI%20-%20CI%20-%20GCC5?repoName=microsoft%2Fmu_feature_dfci&branchName=main
  :target: https://dev.azure.com/projectmu/mu/_build?definitionId=141&_a=summary

.. |UbuntuCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue
