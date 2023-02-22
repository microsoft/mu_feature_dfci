/** @file
DfciVarPolicies.c

This module implements the variable policy settings for the DFCI Variables.


Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DfciManager.h"

#include <DfciVariablePolicies.h>

STATIC EDKII_VARIABLE_POLICY_PROTOCOL  *mVariablePolicy = NULL;

/**
 * Event callback for Ready To Boot.
 * This is needed to lock certain variables
 *
 * @param Event
 * @param Context
 *
 * @return VOID EFIAPI
 */
VOID
EFIAPI
ReadyToBootCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINT8       Lock;
  EFI_STATUS  Status;

  gBS->CloseEvent (Event);

  if (mVariablePolicy == NULL) {
    ASSERT (mVariablePolicy != NULL);
    return;
  }

  //
  // Lock most variables at ReadyToBoot
  //
  Lock   = 1;
  Status = gRT->SetVariable (
                  DFCI_LOCK_VAR_NAME,
                  &gDfciLockVariableGuid,
                  DFCI_LOCK_VAR_ATTRIBUTES,
                  sizeof (Lock),
                  &Lock
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %a: Set DfciLock at ReadyToBoot failed. Code=%r!\n", _DBGMSGID_, __FUNCTION__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a %a: - Dfci Lock variable(%s) set to 1, and locked\n", _DBGMSGID_, __FUNCTION__, DFCI_LOCK_VAR_NAME));
  }
}

/**
  InitializeAndSetPolicyForAllDfciVariables

**/
EFI_STATUS
InitializeAndSetPolicyForAllDfciVariables (
  VOID
  )
{
  UINTN       i;
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  //
  // Request notification of ReadyToBoot.
  //
  // NOTE: If this fails, the variables are not locked
  //       DELAYED_PROCESSING state.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK - 1,  // Lock the Dfci Variables after all other ReadyToBoot handlers
                  ReadyToBootCallback,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &Event
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %a: ReadyToBoot callback registration failed! %r\n", _DBGMSGID_, __FUNCTION__, Status));
    // Continue with rest of variable locks if possible.
  }

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, (VOID **)&mVariablePolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %a: - Locating Variable Policy failed - Code=%r\n", _DBGMSGID_, __FUNCTION__, Status));
    goto Done;
  }

  //
  // Lock most variables when DfciLock variable is created.
  //
  for (i = 0; i < ARRAY_SIZE (gReadyToBootPolicies); i++) {
    Status = RegisterVarStateVariablePolicy (
               mVariablePolicy,
               gReadyToBootPolicies[i].Namespace,
               gReadyToBootPolicies[i].Name,
               gReadyToBootPolicies[i].MinSize,
               gReadyToBootPolicies[i].MaxSize,
               gReadyToBootPolicies[i].AttributesMustHave,
               gReadyToBootPolicies[i].AttributesCantHave,
               &gDfciLockVariableGuid,
               DFCI_LOCK_VAR_NAME,
               1
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a %a: - RegisterVarStateVariablePolicy() ReadyToBoot[%d] returned %r!\n", _DBGMSGID_, __FUNCTION__, i, Status));
      DEBUG ((DEBUG_ERROR, "%a %a: - Error registering %g:%s\n", _DBGMSGID_, __FUNCTION__, gReadyToBootPolicies[i].Namespace, gReadyToBootPolicies[i].Name));
    }
  }

  //
  // Make sure there is no existing lock variable
  //
  Status = gRT->SetVariable (
                  DFCI_LOCK_VAR_NAME,
                  &gDfciLockVariableGuid,
                  0,
                  0,
                  NULL
                  );

  if (Status == EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a %a: Existing variable found, and deleted  This should not happen.\n", _DBGMSGID_, __FUNCTION__));
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a %a: Error deleting Dfci Lock variable(%s). Code=%r\n", _DBGMSGID_, __FUNCTION__, DFCI_LOCK_VAR_NAME, Status));
  }

  for (i = 0; i < ARRAY_SIZE (gDfciLockPolicy); i++) {
    Status = RegisterBasicVariablePolicy (
               mVariablePolicy,
               gDfciLockPolicy[i].Namespace,
               gDfciLockPolicy[i].Name,
               gDfciLockPolicy[i].MinSize,
               gDfciLockPolicy[i].MaxSize,
               gDfciLockPolicy[i].AttributesMustHave,
               gDfciLockPolicy[i].AttributesCantHave,
               VARIABLE_POLICY_TYPE_LOCK_ON_CREATE
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a %a: - RegisterBasicVariablePolicy() DfciLock[%d] returned %r!\n", _DBGMSGID_, __FUNCTION__, i, Status));
      DEBUG ((DEBUG_ERROR, "%a %a: - Error registering %g:%s\n", _DBGMSGID_, __FUNCTION__, gDfciLockPolicy[i].Namespace, gDfciLockPolicy[i].Name));
    }
  }

  //
  // The mailboxes are not locked, but set restrictions for variable sizes and attributes
  //
  for (i = 0; i < ARRAY_SIZE (gMailBoxPolicies); i++) {
    Status = RegisterBasicVariablePolicy (
               mVariablePolicy,
               gMailBoxPolicies[i].Namespace,
               gMailBoxPolicies[i].Name,
               gMailBoxPolicies[i].MinSize,
               gMailBoxPolicies[i].MaxSize,
               gMailBoxPolicies[i].AttributesMustHave,
               gMailBoxPolicies[i].AttributesCantHave,
               VARIABLE_POLICY_TYPE_NO_LOCK
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a %a: - RegisterBasicVariablePolicy() ReadyToBoot[%d] returned %r!\n", _DBGMSGID_, __FUNCTION__, i, Status));
      DEBUG ((DEBUG_ERROR, "%a %a: - Error registering %g:%s\n", _DBGMSGID_, __FUNCTION__, gMailBoxPolicies[i].Namespace, gMailBoxPolicies[i].Name));
    }
  }

Done:

  return Status;
}

/**
  DelateAllMailboxes

  Delete all mailboxes in the error case when DfciManager cannot process variables

**/
EFI_STATUS
DeleteAllMailboxes (
  VOID
  )
{
  UINTN       i;
  EFI_STATUS  Status;
  EFI_STATUS  ReturnStatus;

  ReturnStatus = EFI_SUCCESS;
  for (i = 0; i < ARRAY_SIZE (gMailBoxPolicies); i++) {
    Status = gRT->SetVariable (
                    (CHAR16 *)gMailBoxPolicies[i].Name,
                    (EFI_GUID *)gMailBoxPolicies[i].Namespace,
                    0,
                    0,
                    NULL
                    );
    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_ERROR, "%a %a: - Unable to delete mailbox %g:%s. Code=%r\n", _DBGMSGID_, __FUNCTION__, gMailBoxPolicies[i].Namespace, gMailBoxPolicies[i].Name, Status));
      ReturnStatus = Status;
    }
  }

  return ReturnStatus;
}
