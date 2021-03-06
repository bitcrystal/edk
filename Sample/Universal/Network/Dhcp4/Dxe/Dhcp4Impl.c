/*++

Copyright (c) 2006 - 2009, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. 

Module Name:

  Dhcp4Impl.c

Abstract:

  This file implement the EFI_DHCP4_PROTOCOL interface.
  
--*/

#include "Dhcp4Impl.h"

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4GetModeData (
  IN  EFI_DHCP4_PROTOCOL    *This,
  OUT EFI_DHCP4_MODE_DATA   *Dhcp4ModeData
  )
/*++

Routine Description:

  Get the current operation parameter and lease for the network interface.

Arguments:

  This          - The DHCP protocol instance
  Dhcp4ModeData - The variable to save the DHCP mode data. 

Returns:

  EFI_INVALID_PARAMETER - The parameter is invalid
  EFI_SUCCESS           - The Dhcp4ModeData is updated with the current
                          operation parameter.

--*/
{
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  DHCP_PARAMETER            *Para;
  EFI_TPL                   OldTpl;
  IP4_ADDR                  Ip;

  //
  // First validate the parameters.
  //
  if ((This == NULL) || (Dhcp4ModeData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);
  
  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  DhcpSb  = Instance->Service;

  //
  // Caller can use GetModeData to retrieve current DHCP states
  // no matter whether it is the active child or not.
  //
  Dhcp4ModeData->State                     = DhcpSb->DhcpState;
  Dhcp4ModeData->ConfigData                = DhcpSb->ActiveConfig;
  Dhcp4ModeData->ClientMacAddress          = DhcpSb->Mac;

  Ip = HTONL (DhcpSb->ClientAddr);
  NetCopyMem (&Dhcp4ModeData->ClientAddress, &Ip, sizeof (EFI_IPv4_ADDRESS));

  Ip = HTONL (DhcpSb->Netmask);
  NetCopyMem (&Dhcp4ModeData->SubnetMask, &Ip, sizeof (EFI_IPv4_ADDRESS));

  Ip = HTONL (DhcpSb->ServerAddr);
  NetCopyMem (&Dhcp4ModeData->ServerAddress, &Ip, sizeof (EFI_IPv4_ADDRESS));
  
  Para = DhcpSb->Para;

  if (Para != NULL) {
    Ip = HTONL (Para->Router);
    NetCopyMem (&Dhcp4ModeData->RouterAddress, &Ip, sizeof (EFI_IPv4_ADDRESS));
    Dhcp4ModeData->LeaseTime               = Para->Lease;
  } else {
    NetZeroMem (&Dhcp4ModeData->RouterAddress, sizeof (EFI_IPv4_ADDRESS));
    Dhcp4ModeData->LeaseTime               = 0xffffffff;
  }

  Dhcp4ModeData->ReplyPacket = DhcpSb->Selected;
  
  NET_RESTORE_TPL (OldTpl);
  return EFI_SUCCESS;
}

VOID
DhcpCleanConfigure (
  IN EFI_DHCP4_CONFIG_DATA  *Config
  )
/*++

Routine Description:

  Free the resource related to the configure parameters. 
  DHCP driver will make a copy of the user's configure 
  such as the time out value.

Arguments:

  Config  - The DHCP configure data

Returns:

  None

--*/
{
  UINT32                    Index;

  if (Config->DiscoverTimeout != NULL) {
    NetFreePool (Config->DiscoverTimeout);
  }

  if (Config->RequestTimeout != NULL) {
    NetFreePool (Config->RequestTimeout);
  }

  if (Config->OptionList != NULL) {
    for (Index = 0; Index < Config->OptionCount; Index++) {
      if (Config->OptionList[Index] != NULL) {
        NetFreePool (Config->OptionList[Index]);
      }
    }

    NetFreePool (Config->OptionList);
  }

  NetZeroMem (Config, sizeof (EFI_DHCP4_CONFIG_DATA));
}

EFI_STATUS
DhcpCopyConfigure (
  IN EFI_DHCP4_CONFIG_DATA  *Dst,
  IN EFI_DHCP4_CONFIG_DATA  *Src
  )
/*++

Routine Description:

  Allocate memory for configure parameter such as timeout value for Dst, 
  then copy the configure parameter from Src to Dst.

Arguments:

  Dst - The destination DHCP configure data.
  Src - The source DHCP configure data.

Returns:

  EFI_OUT_OF_RESOURCES - Failed to allocate memory.
  EFI_SUCCESS          - The configure is copied.

--*/
{
  EFI_DHCP4_PACKET_OPTION   **DstOptions;
  EFI_DHCP4_PACKET_OPTION   **SrcOptions;
  INTN                      Len;
  UINT32                    Index;

  *Dst                  = *Src;
  Dst->DiscoverTimeout  = NULL;
  Dst->RequestTimeout   = NULL;
  Dst->OptionList       = NULL;

  //
  // Allocate a memory then copy DiscoverTimeout to it
  //
  if (Src->DiscoverTimeout != NULL) {
    Len                   = Src->DiscoverTryCount * sizeof (UINT32);
    Dst->DiscoverTimeout  = NetAllocatePool (Len);

    if (Dst->DiscoverTimeout == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    for (Index = 0; Index < Src->DiscoverTryCount; Index++) {
      Dst->DiscoverTimeout[Index] = NET_MAX (Src->DiscoverTimeout[Index], 1);
    }
  }
  
  //
  // Allocate a memory then copy RequestTimeout to it
  //
  if (Src->RequestTimeout != NULL) {
    Len                 = Src->RequestTryCount * sizeof (UINT32);
    Dst->RequestTimeout = NetAllocatePool (Len);

    if (Dst->RequestTimeout == NULL) {
      goto ON_ERROR;
    }

    for (Index = 0; Index < Src->RequestTryCount; Index++) {
      Dst->RequestTimeout[Index] = NET_MAX (Src->RequestTimeout[Index], 1);
    }
  }
  
  //
  // Allocate an array of dhcp option point, then allocate memory
  // for each option and copy the source option to it
  //
  if (Src->OptionList != NULL) {
    Len             = Src->OptionCount * sizeof (EFI_DHCP4_PACKET_OPTION *);
    Dst->OptionList = NetAllocateZeroPool (Len);

    if (Dst->OptionList == NULL) {
      goto ON_ERROR;
    }

    DstOptions  = Dst->OptionList;
    SrcOptions  = Src->OptionList;

    for (Index = 0; Index < Src->OptionCount; Index++) {
      Len = sizeof (EFI_DHCP4_PACKET_OPTION) + NET_MAX (SrcOptions[Index]->Length - 1, 0);

      DstOptions[Index] = NetAllocatePool (Len);
      
      if (DstOptions[Index] == NULL) {
        goto ON_ERROR;
      }

      NetCopyMem (DstOptions[Index], SrcOptions[Index], Len);
    }
  }

  return EFI_SUCCESS;

ON_ERROR:
  DhcpCleanConfigure (Dst);
  return EFI_OUT_OF_RESOURCES;
}

VOID
DhcpYieldControl (
  IN DHCP_SERVICE           *DhcpSb
  )
/*++

Routine Description:

  Give up the control of the DHCP service to let other child
  resume. Don't change the service's DHCP state and the Client
  address and option list configure as required by RFC2131.

Arguments:

  DhcpSb  - The DHCP service instance.

Returns:

  None

--*/
{
  EFI_DHCP4_CONFIG_DATA     *Config;
  DHCP_PROTOCOL             *Instance;

  Instance  = DhcpSb->ActiveChild;
  Config    = &DhcpSb->ActiveConfig;

  DhcpSb->ServiceState  = DHCP_UNCONFIGED;
  DhcpSb->ActiveChild   = NULL;

  if (Config->DiscoverTimeout != NULL) {
    NetFreePool (Config->DiscoverTimeout);

    Config->DiscoverTryCount  = 0;
    Config->DiscoverTimeout   = NULL;
  }

  if (Config->RequestTimeout != NULL) {
    NetFreePool (Config->RequestTimeout);

    Config->RequestTryCount = 0;
    Config->RequestTimeout  = NULL;
  }

  Config->Dhcp4Callback   = NULL;
  Config->CallbackContext = NULL;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Configure (
  IN EFI_DHCP4_PROTOCOL     *This,
  IN EFI_DHCP4_CONFIG_DATA  *Dhcp4CfgData       OPTIONAL
  )
/*++

Routine Description:

  Configure the DHCP protocol instance and its underlying DHCP service
  for operation. If Dhcp4CfgData is NULL and the child is currently 
  controlling the DHCP service, release the control.

Arguments:

  This          - The DHCP protocol instance
  Dhcp4CfgData  - The DHCP configure data.

Returns:

  EFI_INVALID_PARAMETER - The parameters are invalid.
  EFI_ACCESS_DENIED     - The service isn't in one of configurable states, or
                          there is already an active child.
  EFI_OUT_OF_RESOURCE   - Failed to allocate some resources.
  EFI_SUCCESS           - The child is configured.
  
--*/
{
  EFI_DHCP4_CONFIG_DATA     *Config;
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  EFI_STATUS                Status;
  EFI_TPL                   OldTpl;
  UINT32                    Index;
  IP4_ADDR                  Ip;

  //
  // First validate the parameters
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Dhcp4CfgData != NULL) {
    if (Dhcp4CfgData->DiscoverTryCount && (Dhcp4CfgData->DiscoverTimeout == NULL)) {
      return EFI_INVALID_PARAMETER;
    }

    if (Dhcp4CfgData->RequestTryCount && (Dhcp4CfgData->RequestTimeout == NULL)) {
      return EFI_INVALID_PARAMETER;
    }

    if (Dhcp4CfgData->OptionCount && (Dhcp4CfgData->OptionList == NULL)) {
      return EFI_INVALID_PARAMETER;
    }

    NetCopyMem (&Ip, &Dhcp4CfgData->ClientAddress, sizeof (IP4_ADDR));

    if ((Ip != 0) && !Ip4IsUnicast (NTOHL (Ip), 0)) {
        
      return EFI_INVALID_PARAMETER;
    }
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);

  if (Instance->Signature != DHCP_PROTOCOL_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  
  DhcpSb  = Instance->Service;
  Config  = &DhcpSb->ActiveConfig;

  Status  = EFI_ACCESS_DENIED;

  if ((DhcpSb->DhcpState != Dhcp4Stopped) &&
      (DhcpSb->DhcpState != Dhcp4Init) &&
      (DhcpSb->DhcpState != Dhcp4InitReboot) &&
      (DhcpSb->DhcpState != Dhcp4Bound)) {
      
    goto ON_EXIT;
  }

  if ((DhcpSb->ActiveChild != NULL) && (DhcpSb->ActiveChild != Instance)) {
    goto ON_EXIT;
  }

  if (Dhcp4CfgData != NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DhcpCleanConfigure (Config);

    if (EFI_ERROR (DhcpCopyConfigure (Config, Dhcp4CfgData))) {
      goto ON_EXIT;
    }

    DhcpSb->UserOptionLen = 0;

    for (Index = 0; Index < Dhcp4CfgData->OptionCount; Index++) {
      DhcpSb->UserOptionLen += Dhcp4CfgData->OptionList[Index]->Length + 2;
    }

    DhcpSb->ActiveChild = Instance;
    
    if (DhcpSb->DhcpState == Dhcp4Stopped) {
      DhcpSb->ClientAddr = EFI_NTOHL (Dhcp4CfgData->ClientAddress);

      if (DhcpSb->ClientAddr != 0) {
        DhcpSb->DhcpState = Dhcp4InitReboot;
      } else {
        DhcpSb->DhcpState = Dhcp4Init;
      }
    }

    DhcpSb->ServiceState  = DHCP_CONFIGED;
    Status                = EFI_SUCCESS;
    
  } else if (DhcpSb->ActiveChild == Instance) {
    Status = EFI_SUCCESS;
    DhcpYieldControl (DhcpSb);
  }

ON_EXIT:
  NET_RESTORE_TPL (OldTpl);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Start (
  IN EFI_DHCP4_PROTOCOL     *This,
  IN EFI_EVENT              CompletionEvent   OPTIONAL
  )
/*++

Routine Description:

  Start the DHCP process.

Arguments:

  This            - The DHCP protocol instance
  CompletionEvent - The event to signal is address is acquired.

Returns:

  EFI_INVALID_PARAMETER - The parameters are invalid.
  EFI_NOT_STARTED       - The protocol hasn't been configured.
  EFI_ALREADY_STARTED   - The DHCP process has already been started.
  EFI_SUCCESS           - The DHCP process is started.

--*/
{
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  EFI_STATUS                Status;
  EFI_TPL                   OldTpl;

  //
  // First validate the parameters
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);

  if (Instance->Signature != DHCP_PROTOCOL_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  DhcpSb  = Instance->Service;

  if (DhcpSb->DhcpState == Dhcp4Stopped) {
    Status = EFI_NOT_STARTED;
    goto ON_ERROR;
  }

  if ((DhcpSb->DhcpState != Dhcp4Init) && (DhcpSb->DhcpState != Dhcp4InitReboot)) {
    Status = EFI_ALREADY_STARTED;
    goto ON_ERROR;
  }

  DhcpSb->IoStatus = EFI_ALREADY_STARTED;

  if (EFI_ERROR (Status = DhcpInitRequest (DhcpSb))) {
    goto ON_ERROR;
  }
  
  //
  // Start/Restart the receiving.
  //
  Status = UdpIoRecvDatagram (DhcpSb->UdpIo, DhcpInput, DhcpSb, 0);

  if (EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
    goto ON_ERROR;
  }
  
  Instance->CompletionEvent = CompletionEvent;
  
  //
  // Restore the TPL now, don't call poll function at NET_TPL_LOCK.
  //
  NET_RESTORE_TPL (OldTpl);

  if (CompletionEvent == NULL) {
    while (DhcpSb->IoStatus == EFI_ALREADY_STARTED) {
      DhcpSb->UdpIo->Udp->Poll (DhcpSb->UdpIo->Udp);
    }

    return DhcpSb->IoStatus;
  }

  return EFI_SUCCESS;

ON_ERROR:
  NET_RESTORE_TPL (OldTpl);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4RenewRebind (
  IN EFI_DHCP4_PROTOCOL     *This,
  IN BOOLEAN                RebindRequest,
  IN EFI_EVENT              CompletionEvent   OPTIONAL
  )
/*++

Routine Description:

  Request an extra manual renew/rebind.

Arguments:

  This            - The DHCP protocol instance
  RebindRequest   - TRUE if request a rebind, otherwise renew it
  CompletionEvent - Event to signal when complete

Returns:

  EFI_INVALID_PARAMETER - The parameters are invalid
  EFI_NOT_STARTED       - The DHCP protocol hasn't been started.
  EFI_ACCESS_DENIED     - The DHCP protocol isn't in Bound state.
  EFI_SUCCESS           - The DHCP is renewed/rebound.

--*/
{
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  EFI_STATUS                Status;
  EFI_TPL                   OldTpl;

  //
  // First validate the parameters
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);

  if (Instance->Signature != DHCP_PROTOCOL_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  DhcpSb  = Instance->Service;

  if (DhcpSb->DhcpState == Dhcp4Stopped) {
    Status = EFI_NOT_STARTED;
    goto ON_ERROR;
  }

  if (DhcpSb->DhcpState != Dhcp4Bound) {
    Status = EFI_ACCESS_DENIED;
    goto ON_ERROR;
  }

  if (DHCP_IS_BOOTP (DhcpSb->Para)) {
    return EFI_SUCCESS;
  }
  
  //
  // Transit the states then send a extra DHCP request
  //
  if (!RebindRequest) {
    DhcpSetState (DhcpSb, Dhcp4Renewing, FALSE);
  } else {
    DhcpSetState (DhcpSb, Dhcp4Rebinding, FALSE);
  }

  Status = DhcpSendMessage (
             DhcpSb,
             DhcpSb->Selected,
             DhcpSb->Para,
             DHCP_MSG_REQUEST,
             "Extra renew/rebind by the application"
             );

  if (EFI_ERROR (Status)) {
    DhcpSetState (DhcpSb, Dhcp4Bound, FALSE);
    goto ON_ERROR;
  }

  DhcpSb->ExtraRefresh        = TRUE;
  DhcpSb->IoStatus            = EFI_ALREADY_STARTED;
  Instance->RenewRebindEvent  = CompletionEvent;
  
  NET_RESTORE_TPL (OldTpl);

  if (CompletionEvent == NULL) {
    while (DhcpSb->IoStatus == EFI_ALREADY_STARTED) {
      DhcpSb->UdpIo->Udp->Poll (DhcpSb->UdpIo->Udp);
    }

    return DhcpSb->IoStatus;
  }

  return EFI_SUCCESS;

ON_ERROR:
  NET_RESTORE_TPL (OldTpl);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Release (
  IN EFI_DHCP4_PROTOCOL     *This
  )
/*++

Routine Description:

  Release the current acquired lease.

Arguments:

  This  - The DHCP protocol instance

Returns:

  EFI_INVALID_PARAMETER - The parameter is invalid
  EFI_DEVICE_ERROR      - Failed to transmit the DHCP release packet
  EFI_ACCESS_DENIED     - The DHCP service isn't in one of the connected state.
  EFI_SUCCESS           - The lease is released.

--*/
{
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  EFI_STATUS                Status;
  EFI_TPL                   OldTpl;

  //
  // First validate the parameters
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);

  if (Instance->Signature != DHCP_PROTOCOL_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  Status  = EFI_SUCCESS;
  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  DhcpSb  = Instance->Service;

  if ((DhcpSb->DhcpState != Dhcp4InitReboot) && (DhcpSb->DhcpState != Dhcp4Bound)) {
    Status = EFI_ACCESS_DENIED;
    goto ON_EXIT;
  }

  if (!DHCP_IS_BOOTP (DhcpSb->Para) && (DhcpSb->DhcpState == Dhcp4Bound)) {
    Status = DhcpSendMessage (
               DhcpSb,
               DhcpSb->Selected,
               DhcpSb->Para,
               DHCP_MSG_RELEASE,
               NULL
               );

    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
    }
  }

  DhcpCleanLease (DhcpSb);

ON_EXIT:
  NET_RESTORE_TPL (OldTpl);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Stop (
  IN EFI_DHCP4_PROTOCOL     *This
  )
/*++

Routine Description:

  Stop the current DHCP process. After this, other DHCP child
  can gain control of the service, configure and use it.

Arguments:

  This  - The DHCP protocol instance

Returns:

  EFI_INVALID_PARAMETER - The parameter is invalid.
  EFI_SUCCESS           - The DHCP process is stopped.

--*/
{
  DHCP_PROTOCOL             *Instance;
  DHCP_SERVICE              *DhcpSb;
  EFI_TPL                   OldTpl;

  //
  // First validate the parameters
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);

  if (Instance->Signature != DHCP_PROTOCOL_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
  DhcpSb  = Instance->Service;

  DhcpCleanLease (DhcpSb);

  DhcpSb->DhcpState     = Dhcp4Stopped;
  DhcpSb->ServiceState  = DHCP_UNCONFIGED;

  NET_RESTORE_TPL (OldTpl);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Build (
  IN EFI_DHCP4_PROTOCOL       *This,
  IN EFI_DHCP4_PACKET         *SeedPacket,
  IN UINT32                   DeleteCount,
  IN UINT8                    *DeleteList OPTIONAL,
  IN UINT32                   AppendCount,
  IN EFI_DHCP4_PACKET_OPTION  *AppendList[] OPTIONAL,
  OUT EFI_DHCP4_PACKET        **NewPacket
  )
/*++

Routine Description:

  Build a new DHCP packet from the seed packet. Options may be deleted or
  appended. The caller should free the NewPacket when finished using it.

Arguments:

  This        - The DHCP protocol instance.
  SeedPacket  - The seed packet to start with
  DeleteCount - The number of options to delete
  DeleteList  - The options to delete from the packet 
  AppendCount - The number of options to append
  AppendList  - The options to append to the packet 
  NewPacket   - The new packet, allocated and built by this function.

Returns:

  EFI_INVALID_PARAMETER - The parameters are invalid.
  EFI_OUT_OF_RESOURCES  - Failed to allocate memory
  EFI_SUCCESS           - The packet is build.
  
--*/
{
  //
  // First validate the parameters
  //
  if ((This == NULL) || (NewPacket == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((SeedPacket == NULL) || (SeedPacket->Dhcp4.Magik != DHCP_OPTION_MAGIC) ||
      EFI_ERROR (DhcpValidateOptions (SeedPacket, NULL))) {
      
    return EFI_INVALID_PARAMETER;
  }

  if (((DeleteCount == 0) && (AppendCount == 0)) ||
      ((DeleteCount != 0) && (DeleteList == NULL)) ||
      ((AppendCount != 0) && (AppendList == NULL))) {
      
    return EFI_INVALID_PARAMETER;
  }

  return DhcpBuild (
           SeedPacket,
           DeleteCount,
           DeleteList,
           AppendCount,
           AppendList,
           NewPacket
           );
}

STATIC
EFI_STATUS
Dhcp4InstanceConfigUdpIo (
  IN UDP_IO_PORT  *UdpIo,
  IN VOID         *Context
  )
{
  DHCP_PROTOCOL                     *Instance;
  DHCP_SERVICE                      *DhcpSb;
  EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN  *Token;
  EFI_UDP4_CONFIG_DATA              UdpConfigData;
  IP4_ADDR                          Ip;

  Instance = (DHCP_PROTOCOL *) Context;
  DhcpSb   = Instance->Service;
  Token    = Instance->Token;

  NetZeroMem (&UdpConfigData, sizeof (EFI_UDP4_CONFIG_DATA));

  UdpConfigData.AcceptBroadcast    = TRUE;
  UdpConfigData.AllowDuplicatePort = TRUE;
  UdpConfigData.TimeToLive         = 64;
  UdpConfigData.DoNotFragment      = TRUE;

  Ip = HTONL (DhcpSb->ClientAddr);
  NetCopyMem (&UdpConfigData.StationAddress, &Ip, sizeof (EFI_IPv4_ADDRESS));

  Ip = HTONL (DhcpSb->Netmask);
  NetCopyMem (&UdpConfigData.SubnetMask, &Ip, sizeof (EFI_IPv4_ADDRESS));

  if ((Token->ListenPointCount == 0) || (Token->ListenPoints[0].ListenPort == 0)) {
    UdpConfigData.StationPort = DHCP_CLIENT_PORT;
  } else {
    UdpConfigData.StationPort = Token->ListenPoints[0].ListenPort;
  }

  return UdpIo->Udp->Configure (UdpIo->Udp, &UdpConfigData);
}

STATIC
EFI_STATUS
Dhcp4InstanceCreateUdpIo (
  IN DHCP_PROTOCOL  *Instance
  )
{
  DHCP_SERVICE  *DhcpSb;

  ASSERT (Instance->Token != NULL);

  DhcpSb          = Instance->Service;
  Instance->UdpIo = UdpIoCreatePort (DhcpSb->Controller, DhcpSb->Image, Dhcp4InstanceConfigUdpIo, Instance);
  if (Instance->UdpIo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  } else {
    return EFI_SUCCESS;
  }
}

STATIC
VOID
DhcpDummyExtFree (
  IN VOID                   *Arg
  )
/*++

Routine Description:

  Release the packet.

Arguments:

  Arg - The packet to release

Returns:

  None

--*/
{  
}

VOID
PxeDhcpInput (
  NET_BUF                   *UdpPacket,
  UDP_POINTS                *Points,
  EFI_STATUS                IoStatus,
  VOID                      *Context
  )
{
  DHCP_PROTOCOL                     *Instance;
  DHCP_SERVICE                      *DhcpSb;
  EFI_DHCP4_HEADER                  *Head;
  NET_BUF                           *Wrap;
  EFI_DHCP4_PACKET                  *Packet;
  EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN  *Token;
  UINT32                            Len;
  EFI_STATUS                        Status;

  Wrap     = NULL;
  Instance = (DHCP_PROTOCOL *) Context;
  Token    = Instance->Token;
  DhcpSb   = Instance->Service;

  //
  // Don't restart receive if error occurs or DHCP is destoried.
  //
  if (EFI_ERROR (IoStatus)) {
    return ;
  }

  ASSERT (UdpPacket != NULL);
  
  //
  // Validate the packet received
  //
  if (UdpPacket->TotalSize < sizeof (EFI_DHCP4_HEADER)) {
    goto RESTART;
  }
  
  //
  // Copy the DHCP message to a continuous memory block, make the buffer size
  // of the EFI_DHCP4_PACKET a multiple of 4-byte.
  //
  Len  = NET_ROUNDUP (sizeof (EFI_DHCP4_PACKET) + UdpPacket->TotalSize - sizeof (EFI_DHCP4_HEADER), 4);
  Wrap = NetbufAlloc (Len);

  if (Wrap == NULL) {
    goto RESTART;
  }

  Packet         = (EFI_DHCP4_PACKET *) NetbufAllocSpace (Wrap, Len, NET_BUF_TAIL);
  Packet->Size   = Len;
  Head           = &Packet->Dhcp4.Header;
  Packet->Length = NetbufCopy (UdpPacket, 0, UdpPacket->TotalSize, (UINT8 *) Head);

  if (Packet->Length != UdpPacket->TotalSize) {
    goto RESTART;
  }
  
  //
  // Is this packet the answer to our packet?
  //
  if ((Head->OpCode != BOOTP_REPLY) ||
      (Head->Xid != Token->Packet->Dhcp4.Header.Xid) ||
      !NET_MAC_EQUAL (&DhcpSb->Mac, Head->ClientHwAddr, DhcpSb->HwLen)) {
    goto RESTART;
  }
  
  //
  // Validate the options and retrieve the interested options
  //
  if ((Packet->Length > sizeof (EFI_DHCP4_HEADER) + sizeof (UINT32)) &&
      (Packet->Dhcp4.Magik == DHCP_OPTION_MAGIC) &&
      EFI_ERROR (DhcpValidateOptions (Packet, NULL))) {

    goto RESTART;
  }

  //
  // Keep this packet in the ResponseQueue.
  //
  NET_GET_REF (Wrap);
  NetbufQueAppend (&Instance->ResponseQueue, Wrap);

RESTART:

  NetbufFree (UdpPacket);

  if (Wrap != NULL) {
    NetbufFree (Wrap);
  }

  Status = UdpIoRecvDatagram (Instance->UdpIo, PxeDhcpInput, Instance, 0);
  if (EFI_ERROR (Status)) {
    PxeDhcpDone (Instance);
  }
}

VOID
PxeDhcpDone (
  IN DHCP_PROTOCOL  *Instance
  )
{
  EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN  *Token;

  Token = Instance->Token;

  Token->ResponseCount = Instance->ResponseQueue.BufNum;
  if (Token->ResponseCount != 0) {
    Token->ResponseList = (EFI_DHCP4_PACKET *) NetAllocatePool (Instance->ResponseQueue.BufSize);
    if (Token->ResponseList == NULL) {
      Token->Status = EFI_OUT_OF_RESOURCES;
      goto SIGNAL_USER;
    }

    //
    // Copy the recieved DHCP responses.
    //
    NetbufQueCopy (&Instance->ResponseQueue, 0, Instance->ResponseQueue.BufSize, (UINT8 *) Token->ResponseList);
    Token->Status = EFI_SUCCESS;
  } else {
    Token->ResponseList = NULL;
    Token->Status       = EFI_TIMEOUT;
  }

SIGNAL_USER:
  //
  // Clean the resources dedicated for this transmit receive transaction.
  //
  NetbufQueFlush (&Instance->ResponseQueue);
  UdpIoCleanPort (Instance->UdpIo);
  UdpIoFreePort (Instance->UdpIo);
  Instance->UdpIo = NULL;
  Instance->Token = NULL;

  if (Token->CompletionEvent != NULL) {
    gBS->SignalEvent (Token->CompletionEvent);
  }  
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4TransmitReceive (
  IN EFI_DHCP4_PROTOCOL                *This,
  IN EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN  *Token
  )
/*++

Routine Description:

  Transmit and receive a packet through this DHCP service. 
  This is unsupported.

Arguments:

  This  - The DHCP protocol instance
  Token - The transmit and receive instance

Returns:

  EFI_UNSUPPORTED - It always returns unsupported.

--*/
{
  DHCP_PROTOCOL  *Instance;
  EFI_TPL        OldTpl;
  EFI_STATUS     Status;
  NET_FRAGMENT   Frag;
  NET_BUF        *Wrap;
  UDP_POINTS     EndPoint;
  IP4_ADDR       Ip;
  DHCP_SERVICE   *DhcpSb;
  IP4_ADDR       Gateway;
  IP4_ADDR       SubnetMask;

  if ((This == NULL) || (Token == NULL) || (Token->Packet == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = DHCP_INSTANCE_FROM_THIS (This);
  DhcpSb   = Instance->Service;

  if (Instance->Token != NULL) {
    //
    // The previous call to TransmitReceive is not finished.
    //
    return EFI_NOT_READY;
  }

  if ((Token->Packet->Dhcp4.Magik != DHCP_OPTION_MAGIC) ||
    (NTOHL (Token->Packet->Dhcp4.Header.Xid) == Instance->Service->Xid) ||
    (Token->TimeoutValue == 0) ||
    ((Token->ListenPointCount != 0) && (Token->ListenPoints == NULL)) ||
    EFI_ERROR (DhcpValidateOptions (Token->Packet, NULL)) ||
    EFI_IP4_EQUAL (Token->RemoteAddress, mZeroIp4Addr)) {
    //
    // The DHCP packet isn't well-formed, the Transaction ID is already used
    // , the timeout value is zero, the ListenPoint is invalid,
    // or the RemoteAddress is zero.
    //
    return EFI_INVALID_PARAMETER;
  }

  if (DhcpSb->ClientAddr == 0) {

    return EFI_NO_MAPPING;
  }

  OldTpl = NET_RAISE_TPL (NET_TPL_LOCK);

  //
  // Save the token and the timeout value.
  //
  Instance->Token   = Token;
  Instance->Timeout = Token->TimeoutValue;

  //
  // Create a UDP IO for this transmit receive transaction.
  //
  Status = Dhcp4InstanceCreateUdpIo (Instance);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Wrap the DHCP packet into a net buffer.
  //
  Frag.Bulk = (UINT8 *) &Token->Packet->Dhcp4;
  Frag.Len  = Token->Packet->Length;
  Wrap      = NetbufFromExt (&Frag, 1, 0, 0, DhcpDummyExtFree, NULL);
  if (Wrap == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  //
  // Set the local address and local port.
  //
  EndPoint.LocalAddr = 0;
  EndPoint.LocalPort = 0;

  //
  // Set the destination address and destination port.
  //
  NetCopyMem (&Ip, &Token->RemoteAddress, sizeof (EFI_IPv4_ADDRESS));
  EndPoint.RemoteAddr = NTOHL (Ip);

  if (Token->RemotePort == 0) {
    EndPoint.RemotePort = DHCP_SERVER_PORT;
  } else {
    EndPoint.RemotePort = Token->RemotePort;
  }

  //
  // Get the gateway.
  //
  SubnetMask = DhcpSb->Netmask;
  Gateway    = 0;
  if (!IP4_NET_EQUAL (DhcpSb->ClientAddr, EndPoint.RemoteAddr, SubnetMask)) {
    NetCopyMem (&Gateway, &Token->GatewayAddress, sizeof (EFI_IPv4_ADDRESS));
    Gateway = NTOHL (Gateway);
  }

  //
  // Transmit the DHCP packet.
  //
  Status = UdpIoSendDatagram (Instance->UdpIo, Wrap, &EndPoint, Gateway, DhcpOnPacketSent, NULL);
  if (EFI_ERROR (Status)) {
    NetbufFree (Wrap);
    goto ON_ERROR;
  }

  //
  // Start to receive the DHCP response.
  //
  Status = UdpIoRecvDatagram (Instance->UdpIo, PxeDhcpInput, Instance, 0);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

ON_ERROR:

  if (EFI_ERROR (Status) && (Instance->UdpIo != NULL)) {
    UdpIoCleanPort (Instance->UdpIo);
    UdpIoFreePort (Instance->UdpIo);
    Instance->UdpIo = NULL;
    Instance->Token = NULL;
  }

  NET_RESTORE_TPL (OldTpl);

  if (!EFI_ERROR (Status) && (Token->CompletionEvent == NULL)) {
    //
    // Keep polling until timeout if no error happens and the CompletionEvent
    // is NULL.
    //
    while (1) {
      OldTpl  = NET_RAISE_TPL (NET_TPL_LOCK);
	  //
	  // Raise TPL to protect the UDPIO in instance, in case that DhcpOnTimerTick
	  // free it when timeout.
	  //
      if (Instance->Timeout > 0) {
        Instance->UdpIo->Udp->Poll (Instance->UdpIo->Udp);
        NET_RESTORE_TPL (OldTpl);
      } else {
        NET_RESTORE_TPL (OldTpl);
        break;
      }
    }
  }

  return Status;
}

STATIC
EFI_STATUS
Dhcp4ParseCheckOption (
  IN UINT8                  Tag,
  IN UINT8                  Len,
  IN UINT8                  *Data,
  IN VOID                   *Context
  )
/*++

Routine Description:

  Callback function for DhcpIterateOptions. This callback sets the 
  EFI_DHCP4_PACKET_OPTION array in the DHCP_PARSE_CONTEXT to point
  the individual DHCP option in the packet.

Arguments:

  Tag     - The DHCP option type
  Len     - length of the DHCP option data
  Data    - The DHCP option data
  Context - The context, to pass several parameters in.

Returns:

  EFI_SUCCESS - It always returns EFI_SUCCESS
  
--*/
{
  DHCP_PARSE_CONTEXT        *Parse;

  Parse = (DHCP_PARSE_CONTEXT *) Context;
  Parse->Index++;
  
  if (Parse->Index <= Parse->OptionCount) {
    //
    // Use _CR to get the memory position of EFI_DHCP4_PACKET_OPTION for
    // the EFI_DHCP4_PACKET_OPTION->Data because DhcpIterateOptions only
    // pass in the point to option data.
    //
    Parse->Option[Parse->Index - 1] = _CR (Data, EFI_DHCP4_PACKET_OPTION, Data);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
EfiDhcp4Parse (
  IN EFI_DHCP4_PROTOCOL       *This,
  IN EFI_DHCP4_PACKET         *Packet,
  IN OUT UINT32               *OptionCount,
  OUT EFI_DHCP4_PACKET_OPTION *PacketOptionList[] OPTIONAL
  )
/*++

Routine Description:

  Parse the DHCP options in the Packet into the PacketOptionList.
  User should allocate this array of EFI_DHCP4_PACKET_OPTION points.

Arguments:

  This             - The DHCP protocol instance
  Packet           - The DHCP packet to parse
  OptionCount      - On input, the size of the PacketOptionList; On output, 
                     the actual number of options processed.
  PacketOptionList - The array of EFI_DHCP4_PACKET_OPTION points

Returns:

  EFI_INVALID_PARAMETER - The parameters are invalid.
  EFI_BUFFER_TOO_SMALL  - A bigger array of points is needed.
  EFI_SUCCESS           - The options are parsed.

--*/
{
  DHCP_PARSE_CONTEXT        Context;
  EFI_STATUS                Status;

  //
  // First validate the parameters
  //
  if ((This == NULL) || (Packet == NULL) || (OptionCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->Size < Packet->Length + 2 * sizeof (UINT32)) ||
      (Packet->Dhcp4.Magik != DHCP_OPTION_MAGIC) ||
      EFI_ERROR (DhcpValidateOptions (Packet, NULL))) {
      
    return EFI_INVALID_PARAMETER;
  }

  if ((*OptionCount != 0) && (PacketOptionList == NULL)) {
    return EFI_BUFFER_TOO_SMALL;
  }
  
  NetZeroMem (PacketOptionList, *OptionCount * sizeof (EFI_DHCP4_PACKET_OPTION *));

  Context.Option      = PacketOptionList;
  Context.OptionCount = *OptionCount;
  Context.Index       = 0;

  Status              = DhcpIterateOptions (Packet, Dhcp4ParseCheckOption, &Context);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  *OptionCount = Context.Index;

  if (Context.Index > Context.OptionCount) {
    return EFI_BUFFER_TOO_SMALL;
  }

  return EFI_SUCCESS;
}

EFI_DHCP4_PROTOCOL  mDhcp4ProtocolTemplate = {
  EfiDhcp4GetModeData,
  EfiDhcp4Configure,
  EfiDhcp4Start,
  EfiDhcp4RenewRebind,
  EfiDhcp4Release,
  EfiDhcp4Stop,
  EfiDhcp4Build,
  EfiDhcp4TransmitReceive,
  EfiDhcp4Parse
};
