# DFCI Threat Model

This document describes the security model for the Device Firmware Configuration Interface (DFCI) feature. It is intended for firmware developers, platform integrators, security reviewers, and technical users who need to understand what DFCI protects, where the trust boundaries are, and which assumptions must hold for the feature to provide strong protection.

DFCI moves selected high-value device configuration from an operating-system policy store into UEFI firmware. Managed settings are applied before the OS boots and are intended to survive OS reinstall, disk reformat, alternate-boot attempts, and routine malware tampering. This threat model is based on the DFCI documentation and implementation in this repository. Platform-specific statements are written as assumptions, integration requirements, or review questions because they cannot be proven from this package alone.

## Feature Summary

DFCI is a DXE-phase UEFI feature that processes management packets delivered through UEFI variables. In the Microsoft cloud-management scenario, Microsoft Intune obtains device-specific DFCI packets from the Autopilot Service (APS), Windows writes those packets through the UEFI CSP into DFCI mailbox variables, and firmware processes them on the next boot.

DFCI separates management into three packet families:

| Packet type | Purpose | Primary protections |
| --- | --- | --- |
| Identity | Enroll, roll, or unenroll management certificates for DFCI identities. | PKCS7 signature verification, certificate self-test signature, device targeting, identity permission checks, optional physical-presence confirmation. |
| Permission | Assign which identities can write each DFCI setting and which identities may delegate that permission. | PKCS7 signature verification, permission and delegation masks, version and lowest-supported-version checks, last-known-good handling. |
| Settings | Apply setting values through registered platform setting providers. | PKCS7 signature verification or constrained unsigned identity, per-setting permission checks, XML validation, version and lowest-supported-version checks. |

DFCI identities are represented as permission-mask bits. The important identities are:

| Identity | Typical role |
| --- | --- |
| Zero Touch | Built-in certificate used only for zero-touch enrollment and selected recovery paths. It has no day-to-day use after enrollment. |
| Owner | The system owner or owner delegate. In the Microsoft scenario this is APS. |
| User | Delegated day-to-day manager. In the Microsoft scenario this is Intune. |
| User1/User2 | Additional delegated identities for non-Microsoft or platform scenarios. |
| Local User | Physically present user authenticated by the platform password policy, or unauthenticated when no password is configured. |
| Unsigned | Reduced local identity for explicitly allowed unsigned settings packets. |

## Security Objectives

DFCI is designed to provide the following properties when correctly integrated by the platform:

1. **Authenticated management**: DFCI accepts identity, permission, and normal settings changes only from provisioned identities whose packets validate against their enrolled certificates.
2. **Authorization and least privilege**: Authentication alone is not sufficient. The signer must have permission for the identity, permission, or setting being changed.
3. **Device targeting**: Packets can be bound to SMBIOS manufacturer, product name, and serial number so packets created for one device do not apply to another.
4. **OS tamper resistance**: The OS is treated as an untrusted transport. Malware can write mailbox variables, but it should not be able to create packets that pass signature and permission checks.
5. **Persistence across OS changes**: Managed state is stored in UEFI non-volatile storage, not on the OS disk.
6. **Pre-OS enforcement**: Firmware applies managed boot, device, and security settings before boot device selection and before the OS can access the device.
7. **DFCI precedence**: Once an owner is enrolled, DFCI permissions take precedence over local setup UI, BIOS password workflows, and other firmware-management mechanisms.
8. **Recovery without weakening management**: A physically present user can recover a device that cannot boot, but recovery remains governed by signed packets, recovery permissions, certificate-based challenge/response, HTTPS validation, or explicit local authorization rules.
9. **Fail closed where possible**: Invalid packet structure, bad signatures, unsupported versions, oversized data, and permission failures produce explicit error status and do not apply the requested change.

## Non-Goals and Assumptions

DFCI does not by itself defend against every firmware or hardware attack. The following are outside the direct protection boundary:

| Area | Assumption or non-goal |
| --- | --- |
| Malicious firmware image | The platform firmware update chain must authenticate firmware updates and prevent rollback to vulnerable firmware. If an attacker can replace firmware with arbitrary code, they can remove or bypass DFCI. |
| Compromised privileged firmware component | Variable services, flash drivers, platform setting providers, and any other privileged firmware paths that can modify DFCI-managed state must be trusted and protected. |
| Sophisticated physical attack | DFCI is intended to resist malware, rootkits, OS reinstall, alternate boot, and casual physical tampering. Lab attacks against flash, debug ports, buses, or device hardware require separate platform mitigations. |
| Compromised management private keys or cloud tenant | DFCI trusts enrolled certificates. If a trusted authority's private key or management tenant is compromised, DFCI cannot distinguish attacker packets from legitimate management packets. |
| Incorrect platform enforcement | DFCI can authorize and store a policy, but the platform must actually disable boot paths, devices, buses, power rails, or setup UI entries in all relevant phases. |
| Privacy of public device identifiers | DFCI publishes device identifiers needed for Autopilot targeting. The platform and management stack must handle those identifiers as device-identifying information. |

## Assets

The main assets protected by DFCI are:

| Asset | Why it matters | Protection expectations |
| --- | --- | --- |
| DFCI code and policy logic | Controls packet validation, identity changes, permissions, and settings. | Protected by firmware update authentication, code integrity, and platform flash protections. |
| Built-in zero-touch certificate | Root of trust for automated enrollment. | Included in the firmware image; must be protected from unauthorized replacement or deletion except through defined opt-out behavior. |
| Enrolled owner/user certificates | Authorize management packets. | Stored in UEFI NV storage, locked before boot, changeable only through DFCI identity authorization.  Public keys only. |
| Permission store | Defines who can change each setting and who may delegate permissions. | Stored in UEFI NV storage, locked before boot, updated only by signed permission packets. |
| Managed setting values | Enforce boot, device, and security configuration. | Stored and enforced by platform setting providers; protected from alternate firmware or UI write paths. |
| Mailbox variables | OS-to-firmware transport for DFCI packets. | Treated as untrusted input; constrained by variable policy size and attributes, then authenticated in firmware. |
| Device identifiers | Bind packets to a physical device. | Must be stable, unique enough for fleet management, and not attacker-controlled from the OS. |
| Recovery challenge and response | Allows recovery without exposing management private keys. | Generated with secure RNG, encrypted for the recovery identity, limited by timeout and anti-hammering. |

## Actors

| Actor | Security posture |
| --- | --- |
| Microsoft Device Management Trust / Zero Touch authority | Trusted root for zero-touch enrollment and selected recovery delegation in the Microsoft scenario. |
| APS / Owner | Trusted owner authority that enrolls delegated management, configures recovery settings, and can unenroll or recover a device. |
| Intune / User | Trusted day-to-day manager for firmware settings and permissions delegated by the owner. |
| Physically present local user | May be legitimate owner, operator, attacker, or repair technician. Trust depends on enrollment state, BIOS password, and DFCI permissions. |
| OS, Windows CSP, and MDM agent | Transport and orchestration layer. Trusted for availability, not trusted for integrity of DFCI content. |
| OS malware/rootkit | Can attempt to write UEFI variables, trigger reboots, read public state, or change OS configuration. Should not be able to forge DFCI packets or bypass pre-OS enforcement. |
| Casual physical attacker | Can attempt alternate boot, OS reinstall, setup-menu changes, or device access. Should not bypass enrolled DFCI policy. |
| Platform firmware integrator | Trusted to correctly integrate DFCI, variable policy, setting providers, UI behavior, and hardware enforcement. |

## Data Flows and Trust Boundaries

### Enrollment

1. The device is registered in Windows Autopilot by an OEM or authorized Cloud Solution Provider.
2. Intune requests DFCI enrollment material for the managed device.
3. APS creates device-targeted packets that enroll the Intune management certificate and configure initial permissions and recovery settings.
4. Windows writes the packets to DFCI mailbox variables through the UEFI CSP.
5. On the next boot, DFCI decodes the packets, checks packet structure, verifies device targeting, validates signatures, checks authorization, and applies identity and permission changes.
6. If an unknown certificate is used on an unenrolled device, DFCI requires physical-presence confirmation and, when configured, BIOS password authentication.

**Trust boundary**: Cloud-to-OS delivery is not the final integrity boundary. The decisive checks happen in firmware, after the OS has placed packets in UEFI variables.

### Day-to-Day Management

1. Intune creates signed, device-specific settings or permission packets.
2. Windows writes packets to DFCI mailbox variables.
3. DFCI authenticates the signer, checks the signer's permission for each setting or delegation change, applies the change through the DFCI Setting Access Protocol, writes result variables, and deletes processed mailbox variables.

**Trust boundary**: Setting providers are part of the trusted computing base. They must not expose alternate write paths that bypass `HasWritePermissions`.

### Local Setup UI

The local firmware UI can display DFCI state, prompt for physical-presence authorization, support zero-touch opt-out on unenrolled devices, and provide recovery entry points. When a DFCI owner is enrolled, local UI and BIOS password flows must respect DFCI permissions and must not offer an override for managed settings.

**Trust boundary**: Human input and UI rendering are trusted only for explicit physical-presence decisions. They are not a substitute for packet signatures or DFCI permissions.

### Recovery

Recovery exists because managed firmware settings can prevent OS boot, for example when external media and network boot are disabled and the internal disk is corrupted. Recovery may use signed recovery packets from APS, USB-delivered packets, or a certificate-based challenge/response flow.

DFCI protects the certificate-based recovery path with recovery permissions, challenge encryption using the provisioned recovery identity, secure RNG, a timeout watchdog, and a limited number of response attempts. For online recovery, the documentation requires server validation against the configured DFCI HTTPS certificate before transferring machine identities. Bootstrap retrieval of an updated HTTPS certificate may occur without authenticating the server, but the downloaded settings packet must be signed before DFCI consumes it.

## STRIDE Threat Analysis

### Spoofing

| Threat | Example | DFCI protections |
| --- | --- | --- |
| Spoofed management authority | Malware writes a packet claiming to be Intune. | Normal packets must validate as PKCS7 signatures against an enrolled identity certificate. The resulting auth token maps to the signer identity used for permission checks. |
| Spoofed zero-touch enrollment | A user self-registers or attacker tries to enroll arbitrary ownership. | Autopilot self-registration is not trusted for DFCI management. Zero-touch enrollment depends on the built-in Microsoft Device Management Trust path. Unknown certificates require a red physical-presence prompt on unenrolled systems. |
| Spoofed device target | A valid packet for device A is delivered to device B. | DFCI compares packet target strings against platform-provided manufacturer, product, and serial number. Integrators must ensure those values are stable and not OS-controlled. |
| Spoofed local user | Attacker uses local UI to change settings after enrollment. | When an owner is enrolled, DFCI permissions take precedence over local user access. BIOS password authentication does not override DFCI permissions. |

### Tampering

| Threat | Example | DFCI protections |
| --- | --- | --- |
| Packet payload modification | Malware changes XML or target strings in a mailbox variable. | DFCI authenticates signed data before use. Packet validation ensures payload, identity fields, and signatures are inside the signed packet bounds. |
| Signature substitution or truncation | Attacker appends, removes, or resizes signature data. | DFCI checks `WIN_CERTIFICATE` type, revision, PKCS7 GUID, declared lengths, packet max size, and signature location at the end of signed data. |
| Certificate enrollment of unusable key | A malformed or non-signing certificate is enrolled, bricking future management. | Identity packets include a test signature over the certificate to prove the corresponding private key can sign data verifiable by that certificate. |
| UEFI variable tampering | OS rewrites current identity, permission, or setting variables. | Current/result/internal DFCI variables have variable policies and are locked at ReadyToBoot. Mailbox variables remain writable but are constrained by size and attributes and treated as untrusted input. |
| Alternate setting path | Setup UI or platform code changes hardware state without DFCI permission. | Integration requirement: all DFCI-managed settings must flow through DFCI Setting Access or enforce equivalent permission checks. Managed settings must be locked before boot device selection. |

### Repudiation

| Threat | Example | DFCI protections and limitations |
| --- | --- | --- |
| Manager denies sending a packet | A bad configuration is applied and the sender disputes it. | Packets are signed by enrolled identities, and result variables include session IDs and status codes. DFCI provides technical attribution to a certificate identity, not a complete audit log. Fleet audit logging should be handled by the management service. |
| Local recovery action is disputed | A device is recovered from the pre-boot menu. | Recovery requires permitted identity material or signed packets, but firmware-local evidence is limited. Management services should log recovery packet issuance and device retirement state. |

### Information Disclosure

| Threat | Example | DFCI protections and limitations |
| --- | --- | --- |
| Device identifier disclosure | OS or recovery service reads serial number, manufacturer, and model. | These identifiers are intentionally published for targeting and recovery. Treat them as device-identifying data and expose only to authorized management channels. |
| Recovery challenge disclosure | Attacker observes an online recovery request. | Recovery challenge data is encrypted to the recovery identity certificate. HTTPS is used after server authentication against the configured DFCI HTTPS certificate. |
| Packet contents disclosure | OS can read mailbox variables before reboot. | DFCI focuses on integrity and authorization of management packets. Do not place secrets in DFCI XML payloads unless separately protected. |

### Denial of Service

| Threat | Example | DFCI protections and limitations |
| --- | --- | --- |
| Oversized or malformed mailbox variables | Malware writes large or malformed variables to consume memory or break boot. | DFCI enforces maximum apply/result/current sizes, validates offsets and lengths, reports errors, and deletes mailboxes after processing or on manager failure. |
| Bad but signed configuration | Authorized manager disables boot paths and the OS later cannot boot. | DFCI provides pre-boot recovery mechanisms and owner-driven unenrollment. Management policy design remains responsible for avoiding unrecoverable configurations. |
| Recovery response hammering | Attacker brute-forces recovery response bytes. | Recovery permits one active recovery session per boot, limits attempts, sets a watchdog timeout, and shuts down after hammering is detected. |
| Packet rollback causing stale policy | Attacker replays old but valid signed settings or permissions. | Settings and permissions carry `Version` and `LowestSupportedVersion` values. Firmware rejects packets below the stored LSV. Managers should monotonically advance LSV when retiring old policy. |

### Elevation of Privilege

| Threat | Example | DFCI protections |
| --- | --- | --- |
| Delegated manager grants itself more authority | User identity tries to modify Owner-only permissions. | Permission packets are authorized by `PMask` and `DMask`. Existing permissions can be changed only by identities in the relevant delegation mask. |
| Local user bypasses enrolled owner | Physical attacker changes boot or device settings in setup. | Enrolled DFCI owner permissions override local setup UI. Local user access must be greyed, blocked, or routed through DFCI permission checks. |
| Unsigned settings alter security posture | Attacker deploys unsigned packet to enable external boot. | Unsigned packets authenticate as the reduced Unsigned identity. Platforms must explicitly allow unsigned settings or use a disallow list, and unsigned settings are available only before owner enrollment unless the owner grants that permission. Security-sensitive settings should not be unsigned. |
| Setting provider ignores DFCI permissions | Provider exposes a private setter or late boot code re-enables a disabled device. | Integration requirement: providers must register with Settings Manager, enforce values in all boot phases that matter, and avoid alternate unauthenticated write paths. |

## Important Protection Mechanisms

### Signed Packets

Identity, permission, and normal settings packets are signed using PKCS7 data carried in UEFI `WIN_CERTIFICATE_UEFI_GUID` structures. DFCI verifies signatures against currently provisioned certificates and derives an auth token for the identity that signed the packet. Packet validation checks data size, offsets, signature placement, certificate format, and payload bounds before applying changes.

The packet `SessionId` is used for request/result correlation and is zeroed for signature verification. Anti-replay is provided by packet versions and lowest-supported-version state, not by `SessionId`.

### Device Targeting

DFCI packet headers include offsets for manufacturer, product, and serial strings. Firmware compares those strings with values returned by the platform `DfciDeviceIdSupportLib`. Empty strings are treated as wildcards. Wildcard use should be limited to trusted enrollment delegation flows, not routine per-device management.

### Permission and Delegation Masks

DFCI separates write permission (`PMask`) from delegation authority (`DMask`). A signer may authenticate successfully and still be unable to change a setting, permission, or identity. Group settings apply permissions across multiple platform-specific settings, and explicit group denial prevents a member setting from being changed through that group.

### UEFI Variable Policy

DFCI uses UEFI variables as its mailbox and persistent state channel. The mailbox variables are intentionally not locked because the OS must write requests. They are constrained by variable policy and authenticated later. Current state, result variables, internal DFCI variables, device ID variables, and zero-touch variables are registered with variable policies and locked at ReadyToBoot.

### Last Known Good for Identity and Permissions

DFCI processes identity and permission packets before settings packets. Identity and permission changes are committed or restored through last-known-good handling so a severe error can roll those stores back. Settings are applied after identities and permissions and do not have the same LKG rollback behavior, so setting providers must validate inputs carefully and report failures accurately.

### Physical Presence and Unknown Certificates

On an unenrolled system, an unknown owner certificate enrollment can require local authorization. The prompt asks the user to confirm the last two characters of the new certificate thumbprint, and a configured BIOS password must be entered first. This supports development and custom identity management while reducing accidental or malicious enrollment.

### Zero-Touch Opt-Out

An unenrolled platform should provide an opt-out path that removes or disables the zero-touch enrollment path. After opt-out, automated enrollment attempts require physical-presence confirmation. This is useful for users who do not want automated DFCI management on a device that is not currently enrolled.

### Recovery

Recovery authorization is controlled by DFCI permissions. The challenge/response flow creates a random nonce using a secure UEFI RNG algorithm, includes a timestamp and device-identifying strings, encrypts the challenge to a provisioned recovery certificate, and validates a fixed-size response. Recovery is limited to three attempts and guarded by a watchdog timeout.

Online recovery should validate the recovery server against the configured DFCI HTTPS certificate before transferring machine identities. The bootstrap URL may retrieve an updated HTTPS certificate without server authentication, but the returned settings packet must be signed before use.

## Developer Guidance

Platform security depends heavily on integration quality. Firmware developers should treat the following as requirements for a secure DFCI implementation:

1. **Protect DFCI storage before boot device selection.** Current identities, permissions, settings, zero-touch state, and platform setting storage must not remain writable by the OS or by untrusted pre-boot code after DFCI processing.
2. **Enforce settings in hardware or earliest practical firmware phase.** Disabling a camera, radio, network boot path, USB boot path, or virtualization feature must affect the actual device, bus, boot option, or power rail before the OS can use it.
3. **Eliminate bypass paths.** Setup UI, hotkeys, manufacturing tools, recovery menus, firmware update flows, debug interfaces, and private provider APIs must not modify DFCI-managed settings without equivalent DFCI permission checks.
4. **Keep device identifiers stable and authoritative.** Manufacturer, product, and serial values used for targeting must be consistent with Autopilot registration and unavailable for OS-level tampering.
5. **Use unsigned settings only for low-risk configuration.** Do not allow unsigned packets for boot control, device disablement, identity, recovery, password, or security-boundary settings.
6. **Advance LSV on policy retirement.** Management systems should use `LowestSupportedVersion` to retire old signed packets and reduce replay risk.
7. **Protect private keys outside firmware.** DFCI verifies signatures; it cannot protect cloud, tenant, build, or test private keys. Key compromise is a management-plane compromise.
8. **Harden local authentication.** BIOS password handling should include platform-appropriate anti-hammering and recovery behavior. Physical presence should never silently authorize enrollment of unknown management keys.
9. **Test from an attacker perspective.** Validate that OS malware can write mailbox variables but cannot apply unauthorized changes; that disabled boot paths remain disabled after OS reinstall; that setup UI respects DFCI permissions; and that recovery still works when the OS disk cannot boot.

## Residual Risks

| Risk | Mitigation strategy |
| --- | --- |
| Trusted manager signs harmful policy | Use management approvals, staged rollout, change auditing, and recovery planning. |
| Firmware update removes DFCI or weakens enforcement | Require authenticated firmware update, anti-rollback, release signing, and security review for DFCI-related changes. |
| Platform provider partially enforces a group setting | Test each group member independently and verify hardware-level disablement where applicable. |
| Weak or mutable SMBIOS identifiers break targeting | Source identifiers from platform-controlled storage and validate uniqueness in manufacturing. |
| Unsigned setting list is too broad | Prefer allow-list mode and limit entries to non-security-sensitive settings. |
| Authorized recovery is unavailable due to network failure | Provide offline recovery packet paths, documented support workflows, and owner-driven retirement/unenrollment flows. |

## Platform-Specific Information Needed

The following items cannot be fully determined from this package alone. A complete product threat model should answer them for the target platform:

1. Which hardware or firmware mechanism protects DFCI NV storage and setting-provider storage before boot device selection?
2. Which exact boot paths, buses, devices, power rails, setup UI controls, and debug/manufacturing interfaces are affected by each DFCI group setting?
3. Can any PEI, DXE, SMM, BDS, setup, recovery, capsule update, factory, or OS runtime path change a DFCI-managed setting without using DFCI Setting Access?
4. How are SMBIOS manufacturer, product, and serial values provisioned, protected, and kept consistent with Autopilot registration?
5. Which settings, if any, are intentionally allowed through unsigned packets, and why are they non-security-sensitive?
6. Which recovery paths are supported on the product, and how are recovery packet issuance, recovery attempts, and retirement audited?

## Review Checklist

Use this checklist when reviewing a DFCI platform port:

1. DFCI DXE drivers, variable policy, Identity/Auth Manager, Settings Manager, DFCI Manager, and DFCI UI components are included in the platform build.
2. The built-in zero-touch certificate is present only when the platform intends to support automated DFCI enrollment.
3. Current state and internal variables are locked before boot device selection; mailbox variables are size- and attribute-constrained.
4. All DFCI-managed settings are registered through setting providers and cannot be changed through alternate local or OS paths.
5. Group settings map to every platform-specific device that the abstract DFCI setting claims to control.
6. Boot, device, radio, camera, audio, and virtualization settings are enforced before the OS or alternate boot media can use them.
7. Local setup UI greys, blocks, or routes managed settings according to DFCI permissions when owner is enrolled.
8. Unknown certificate enrollment requires physical presence and BIOS password when configured.
9. Recovery is reachable from pre-boot UI even when OS boot paths are broken, but recovery still requires signed packets or permitted challenge/response.
10. Unsigned settings are disabled or strictly allow-listed to non-security-sensitive settings.
11. Version and LSV handling is tested for replayed old packets.
12. Malformed, oversized, wrong-target, wrong-signature, wrong-permission, and unsupported-version packets fail without applying changes.

---

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent
