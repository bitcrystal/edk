/*++

Copyright (c) 2005 - 2007, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. 

Module Name:

  IpIo.h

Abstract:

--*/

#ifndef _IP_IO_H_
#define _IP_IO_H_

#include "NetLib.h"
#include "NetBuffer.h"
#include "IcmpIo.h"

//
// Driver Consumed Protocol Prototypes
//
#include EFI_PROTOCOL_CONSUMER (Ip4)
#include EFI_PROTOCOL_CONSUMER (Udp4)

#define EFI_IP4_HEADER_LEN(HdrPtr) ((HdrPtr)->HeaderLength << 2)

extern EFI_IP4_CONFIG_DATA  mIpIoDefaultIpConfigData;

typedef struct _EFI_NET_SESSION_DATA {
  IP4_ADDR        Source;
  IP4_ADDR        Dest;
  EFI_IP4_HEADER  *IpHdr;
} EFI_NET_SESSION_DATA;

typedef
VOID
(*PKT_RCVD_NOTIFY) (
  IN EFI_STATUS           Status,  // rcvd pkt result
  IN ICMP_ERROR           IcmpErr, // if Status == EFI_ICMP_ERROR, this
                                  // field is valid for user
  IN EFI_NET_SESSION_DATA *NetSession, // the communication point
  IN NET_BUF              *Pkt,    // packet received
  IN VOID                 *Context // the Context provided by user for recive data
  );

typedef
VOID
(*PKT_SENT_NOTIFY) (
  IN EFI_STATUS  Status,      // sent pkt result
  IN VOID        *Context,    // the context provided by user for sending data
  IN VOID        *Sender,     // the sender to be notified
  IN VOID        *NotifyData  // sent pkt related data to notify
  );

typedef struct _IP_IO {

  //
  // the node used to link this IpIo to the active IpIo list.
  //
  NET_LIST_ENTRY                Entry;

  // the list used to maintain the IP instance for different sending purpose.
  //
  NET_LIST_ENTRY                IpList;

  //
  // the ip instance consumed by this IP IO
  //
  EFI_HANDLE                    Controller;
  EFI_HANDLE                    Image;
  EFI_HANDLE                    ChildHandle;
  EFI_IP4_PROTOCOL              *Ip;
  BOOLEAN                       IsConfigured;

  //
  // some ip config data can be changed
  //
  UINT8                         Protocol;

  //
  // token and event used to get data from IP
  //
  EFI_IP4_COMPLETION_TOKEN      RcvToken;

  //
  // list entry used to link the token passed to IP_IO
  //
  NET_LIST_ENTRY                PendingSndList;

  //
  // User interface used to get notify from IP_IO
  //
  VOID                          *RcvdContext;
  VOID                          *SndContext;
  PKT_RCVD_NOTIFY               PktRcvdNotify;
  PKT_SENT_NOTIFY               PktSentNotify;
} IP_IO;

typedef struct _IP_IO_OPEN_DATA {
  EFI_IP4_CONFIG_DATA IpConfigData;
  VOID                *RcvdContext;
  VOID                *SndContext;
  PKT_RCVD_NOTIFY     PktRcvdNotify;
  PKT_SENT_NOTIFY     PktSentNotify;
} IP_IO_OPEN_DATA;

typedef struct _IP_IO_SEND_ENTRY {
  NET_LIST_ENTRY            Entry;
  IP_IO                     *IpIo;
  VOID                      *Context;
  VOID                      *NotifyData;
  EFI_IP4_PROTOCOL          *Ip;
  NET_BUF                   *Pkt;
  EFI_IP4_COMPLETION_TOKEN  *SndToken;
} IP_IO_SEND_ENTRY;

typedef struct _EFI_IP4_OVERRIDE_DATA IP_IO_OVERRIDE;

typedef struct _IP_IO_IP_INFO {
  IP4_ADDR                  Addr;
  IP4_ADDR                  SubnetMask;
  NET_LIST_ENTRY            Entry;
  EFI_HANDLE                ChildHandle;
  EFI_IP4_PROTOCOL          *Ip;
  EFI_IP4_COMPLETION_TOKEN  DummyRcvToken;
  INTN                      RefCnt;
} IP_IO_IP_INFO;

IP_IO *
IpIoCreate (
  IN EFI_HANDLE Image,
  IN EFI_HANDLE Controller
  );

EFI_STATUS
IpIoDestroy (
  IN IP_IO *IpIo
  );

EFI_STATUS
IpIoStop (
  IN IP_IO *IpIo
  );

EFI_STATUS
IpIoOpen (
  IN IP_IO           *IpIo,
  IN IP_IO_OPEN_DATA *OpenData
  );

EFI_STATUS
IpIoSend (
  IN IP_IO           *IpIo,
  IN NET_BUF         *Pkt,
  IN IP_IO_IP_INFO   *Sender,
  IN VOID            *Context    OPTIONAL,
  IN VOID            *NotifyData OPTIONAL,
  IN IP4_ADDR        Dest,
  IN IP_IO_OVERRIDE  *OverrideData
  );

VOID
IpIoCancelTxToken (
  IN IP_IO  *IpIo,
  IN VOID   *Packet
  );

IP_IO_IP_INFO *
IpIoAddIp (
  IN IP_IO  *IpIo
  );

EFI_STATUS
IpIoConfigIp (
  IN     IP_IO_IP_INFO        *IpInfo,
  IN OUT EFI_IP4_CONFIG_DATA  *Ip4ConfigData OPTIONAL
  );

VOID
IpIoRemoveIp (
  IN IP_IO            *IpIo,
  IN IP_IO_IP_INFO    *IpInfo
  );

IP_IO_IP_INFO *
IpIoFindSender (
  IN OUT IP_IO     **IpIo,
  IN     IP4_ADDR  Src
  );

EFI_STATUS
IpIoGetIcmpErrStatus (
  IN  ICMP_ERROR  IcmpError,
  OUT BOOLEAN     *IsHard, OPTIONAL
  OUT BOOLEAN     *Notify OPTIONAL
  );

#endif
