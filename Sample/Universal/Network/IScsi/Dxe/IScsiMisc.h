/*++

Copyright (c) 2007, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. 

Module Name:

  IScsiMisc.h

Abstract:

  Miscellaneous definitions for iSCSI driver.

--*/

#ifndef _ISCSI_MISC_H_
#define _ISCSI_MISC_H_

#pragma pack(1)
typedef struct _ISCSI_SESSION_CONFIG_NVDATA {
  UINT16            TargetPort;
  BOOLEAN           Enabled;

  BOOLEAN           InitiatorInfoFromDhcp;
  EFI_IPv4_ADDRESS  LocalIp;
  EFI_IPv4_ADDRESS  SubnetMask;
  EFI_IPv4_ADDRESS  Gateway;

  BOOLEAN           TargetInfoFromDhcp;
  CHAR8             TargetName[ISCSI_NAME_MAX_SIZE];
  EFI_IPv4_ADDRESS  TargetIp;
  UINT8             BootLun[8];
} ISCSI_SESSION_CONFIG_NVDATA;
#pragma pack()

typedef struct _ISCSI_SESSION_CONFIG_DATA {
  ISCSI_SESSION_CONFIG_NVDATA NvData;

  EFI_IPv4_ADDRESS            PrimaryDns;
  EFI_IPv4_ADDRESS            SecondaryDns;
  EFI_IPv4_ADDRESS            DhcpServer;
} ISCSI_SESSION_CONFIG_DATA;

UINT8
IScsiGetSubnetMaskPrefixLength (
  IN EFI_IPv4_ADDRESS  *SubnetMask
  );

EFI_STATUS
IScsiAsciiStrToLun (
  IN  CHAR8  *Str,
  OUT UINT8  *Lun
  );

VOID
IScsiLunToUnicodeStr (
  IN UINT8    *Lun,
  OUT CHAR16  *String
  );

CHAR16                    *
IScsiAsciiStrToUnicodeStr (
  IN  CHAR8   *Source,
  OUT CHAR16  *Destination
  );

CHAR8                     *
IScsiUnicodeStrToAsciiStr (
  IN  CHAR16  *Source,
  OUT CHAR8   *Destination
  );

VOID
IScsiMacAddrToStr (
  IN  EFI_MAC_ADDRESS  *Mac,
  IN  UINT32           Len,
  OUT CHAR16           *Str
  );

EFI_STATUS
IScsiAsciiStrToIp (
  IN  CHAR8             *Str,
  OUT EFI_IPv4_ADDRESS  *Ip
  );

EFI_STATUS
IScsiBinToHex (
  IN     UINT8  *BinBuffer,
  IN     UINT32 BinLength,
  IN OUT CHAR8  *HexStr,
  IN OUT UINT32 *HexLength
  );

EFI_STATUS
IScsiHexToBin (
  IN OUT UINT8  *BinBuffer,
  IN OUT UINT32 *BinLength,
  IN     CHAR8  *HexStr
  );

VOID
IScsiGenRandom (
  IN OUT UINT8  *Rand,
  IN     UINTN  RandLength
  );

ISCSI_DRIVER_DATA         *
IScsiCreateDriverData (
  IN EFI_HANDLE  Image,
  IN EFI_HANDLE  Controller
  );

VOID
IScsiCleanDriverData (
  IN ISCSI_DRIVER_DATA  *Private
  );

EFI_STATUS
IScsiGetConfigData (
  IN ISCSI_DRIVER_DATA  *Private
  );

EFI_DEVICE_PATH_PROTOCOL  *
IScsiGetTcpConnDevicePath (
  IN ISCSI_DRIVER_DATA  *Private
  );

VOID
EFIAPI
IScsiOnExitBootService (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

#endif
