/*++

Copyright (c) 2005 - 2008, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  NetLib.h

Abstract:

  Library for the UEFI network stack. 

--*/

#ifndef _NET_LIB_H_
#define _NET_LIB_H_

#include "Tiano.h"
#include "EfiDriverLib.h"
#include "NetHeader.h"

#include EFI_PROTOCOL_CONSUMER (LoadedImage)
#include EFI_PROTOCOL_CONSUMER (ServiceBinding)
#include EFI_PROTOCOL_CONSUMER (SimpleNetwork)
#include EFI_PROTOCOL_CONSUMER (NicIp4Config)
#include EFI_PROTOCOL_CONSUMER (Dpc)

extern EFI_IPv4_ADDRESS  mZeroIp4Addr;

#define NET_IS_DIGIT(Ch)            (('0' <= (Ch)) && ((Ch) <= '9'))
#define NET_IS_LOWER_CASE_CHAR(Ch)  (('a' <= (Ch)) && ((Ch) <= 'z'))
#define NET_IS_UPPER_CASE_CHAR(Ch)  (('A' <= (Ch)) && ((Ch) <= 'Z'))
#define NET_ROUNDUP(size, unit)     (((size) + (unit) - 1) & (~((unit) - 1)))

//
// Wrap functions to ease the impact of EFI library changes.
//
#define NetAllocateZeroPool     EfiLibAllocateZeroPool
#define NetAllocatePool         EfiLibAllocatePool
#define NetFreePool             gBS->FreePool
#define NetCopyMem              EfiCopyMem
#define NetSetMem               EfiSetMem
#define NetZeroMem(Dest, Len)   EfiSetMem ((Dest), (Len), 0)
#define NetCompareMem           EfiCompareMem

//
// Lock primitives: the stack implements its lock primitives according
// to the standard EFI enviornment. It will NOT consider multiprocessor.
//
#define NET_TPL_LOCK            EFI_TPL_CALLBACK
#define NET_TPL_EVENT           EFI_TPL_NOTIFY
#define NET_TPL_RECYCLE         EFI_TPL_NOTIFY
#define NET_TPL_TIMER           NET_TPL_LOCK

#define NET_LOCK                 EFI_LOCK
#define NET_LOCK_INIT(x)         EfiInitializeLock (x, NET_TPL_LOCK)
#define NET_RECYCLE_LOCK_INIT(x) EfiInitializeLock (x, NET_TPL_RECYCLE)
#define NET_TRYLOCK(x)           EfiAcquireLockOrFail (x)
#define NET_UNLOCK(x)            EfiReleaseLock (x)

#define NET_RAISE_TPL(x)        (gBS->RaiseTPL (x))
#define NET_RESTORE_TPL(x)      (gBS->RestoreTPL (x))

#define TICKS_PER_MS            10000U
#define TICKS_PER_SECOND        10000000U

#define NET_MIN(a, b)           ((a) < (b) ? (a) : (b))
#define NET_MAX(a, b)           ((a) > (b) ? (a) : (b))
#define NET_RANDOM(Seed)        (((Seed) * 1103515245L + 12345) % 4294967295L)

UINTN
NetAtoi (
  IN CHAR8  *Str
  );

UINTN
NetXtoi (
  IN CHAR8  *Str
  );

UINT32
NetGetUint32 (
  IN UINT8                  *Buf
  );

VOID
NetPutUint32 (
  IN UINT8                  *Buf,
  IN UINT32                 Data
  );

UINT32
NetRandomInitSeed (
  VOID
  );


//
// Double linked list entry functions, this extends the
// EFI list functions.
//
typedef EFI_LIST_ENTRY  NET_LIST_ENTRY;

#define NetListInit(Head)              InitializeListHead(Head)
#define NetListInsertHead(Head, Entry) InsertHeadList((Head), (Entry))
#define NetListInsertTail(Head, Entry) InsertTailList((Head), (Entry))
#define NetListIsEmpty(List)           IsListEmpty(List)

#define NET_LIST_USER_STRUCT(Entry, Type, Field)        \
          _CR(Entry, Type, Field)
          
#define NET_LIST_USER_STRUCT_S(Entry, Type, Field, Sig)  \
          CR(Entry, Type, Field, Sig)

//
// Iterate through the doule linked list. It is NOT delete safe
//
#define NET_LIST_FOR_EACH(Entry, ListHead) \
  for(Entry = (ListHead)->ForwardLink; Entry != (ListHead); Entry = Entry->ForwardLink)
      
//
// Iterate through the doule linked list. This is delete-safe. 
// Don't touch NextEntry. Also, don't use this macro if list 
// entries other than the Entry may be deleted when processing
// the current Entry.
//
#define NET_LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead) \
  for(Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink; \
      Entry != (ListHead); \
      Entry = NextEntry, NextEntry = Entry->ForwardLink \
     )
      
//
// Make sure the list isn't empty before get the frist/last record.
//
#define NET_LIST_HEAD(ListHead, Type, Field)  \
          NET_LIST_USER_STRUCT((ListHead)->ForwardLink, Type, Field)

#define NET_LIST_TAIL(ListHead, Type, Field)  \
          NET_LIST_USER_STRUCT((ListHead)->BackLink, Type, Field)

#define NetListRemoveEntry(Entry) RemoveEntryList (Entry)

NET_LIST_ENTRY*
NetListRemoveHead (
  NET_LIST_ENTRY            *Head
  );

NET_LIST_ENTRY*
NetListRemoveTail (
  NET_LIST_ENTRY            *Head
  );

VOID
NetListInsertAfter (
  IN NET_LIST_ENTRY         *PrevEntry,
  IN NET_LIST_ENTRY         *NewEntry
  );

VOID
NetListInsertBefore (
  IN NET_LIST_ENTRY         *PostEntry,
  IN NET_LIST_ENTRY         *NewEntry
  );


//
// Object container: EFI network stack spec defines various kinds of
// tokens. The drivers can share code to manage those objects.
//
typedef struct {
  NET_LIST_ENTRY            Link;
  VOID                      *Key;
  VOID                      *Value;
} NET_MAP_ITEM;

typedef struct {
  NET_LIST_ENTRY            Used;
  NET_LIST_ENTRY            Recycled;
  UINTN                     Count;
} NET_MAP;

#define NET_MAP_INCREAMENT  64

VOID
NetMapInit (
  IN NET_MAP                *Map
  );

VOID
NetMapClean (
  IN NET_MAP                *Map
  );

BOOLEAN
NetMapIsEmpty (
  IN NET_MAP                *Map
  );

UINTN
NetMapGetCount (
  IN NET_MAP                *Map
  );

EFI_STATUS
NetMapInsertHead (
  IN NET_MAP                *Map,
  IN VOID                   *Key,
  IN VOID                   *Value    OPTIONAL
  );

EFI_STATUS
NetMapInsertTail (
  IN NET_MAP                *Map,
  IN VOID                   *Key,
  IN VOID                   *Value    OPTIONAL
  );

NET_MAP_ITEM  *
NetMapFindKey (
  IN  NET_MAP               *Map,
  IN  VOID                  *Key
  );

VOID *
NetMapRemoveItem (
  IN  NET_MAP               *Map,
  IN  NET_MAP_ITEM          *Item,
  OUT VOID                  **Value   OPTIONAL
  );

VOID *
NetMapRemoveHead (
  IN  NET_MAP               *Map,
  OUT VOID                  **Value   OPTIONAL
  );

VOID *
NetMapRemoveTail (
  IN  NET_MAP               *Map,
  OUT VOID                  **Value OPTIONAL
  );

typedef
EFI_STATUS
(*NET_MAP_CALLBACK) (
  IN NET_MAP                *Map,
  IN NET_MAP_ITEM           *Item,
  IN VOID                   *Arg
  );

EFI_STATUS
NetMapIterate (
  IN NET_MAP                *Map,
  IN NET_MAP_CALLBACK       CallBack,
  IN VOID                   *Arg      OPTIONAL
  );


//
// Helper functions to implement driver binding and service binding protocols.
//
EFI_STATUS
NetLibCreateServiceChild (
  IN  EFI_HANDLE            ControllerHandle,
  IN  EFI_HANDLE            ImageHandle,
  IN  EFI_GUID              *ServiceBindingGuid,
  OUT EFI_HANDLE            *ChildHandle
  );

EFI_STATUS
NetLibDestroyServiceChild (
  IN  EFI_HANDLE            ControllerHandle,
  IN  EFI_HANDLE            ImageHandle,
  IN  EFI_GUID              *ServiceBindingGuid,
  IN  EFI_HANDLE            ChildHandle
  );

EFI_STATUS
NetLibGetMacString (
  IN           EFI_HANDLE  SnpHandle,
  IN           EFI_HANDLE  ImageHandle,
  IN OUT CONST CHAR16      **MacString
  );

VOID
NetLibCreateIPv4DPathNode (
  IN OUT IPv4_DEVICE_PATH  *Node,
  IN EFI_HANDLE            Controller,
  IN IP4_ADDR              LocalIp,
  IN UINT16                LocalPort,
  IN IP4_ADDR              RemoteIp,
  IN UINT16                RemotePort,
  IN UINT16                Protocol,
  IN BOOLEAN               UseDefaultAddress
  );

EFI_HANDLE
NetLibGetNicHandle (
  IN EFI_HANDLE             Controller,
  IN EFI_GUID               *ProtocolGuid
  );

EFI_STATUS
NetLibQueueDpc (
  IN EFI_TPL            DpcTpl,
  IN EFI_DPC_PROCEDURE  DpcProcedure,
  IN VOID               *DpcContext    OPTIONAL
  );

EFI_STATUS
NetLibDispatchDpc (
  VOID
  );

typedef
EFI_STATUS
(EFIAPI *NET_LIB_DRIVER_UNLOAD) (
  IN EFI_HANDLE             ImageHandle
  );

EFI_STATUS
EFIAPI
NetLibDefaultUnload (
  IN EFI_HANDLE             ImageHandle
  );

EFI_STATUS
NetLibInstallAllDriverProtocolsWithUnload (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL        *DriverBinding,
  IN EFI_HANDLE                         DriverBindingHandle,
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName,       OPTIONAL
  IN EFI_DRIVER_CONFIGURATION2_PROTOCOL *DriverConfiguration, OPTIONAL
  IN EFI_DRIVER_DIAGNOSTICS2_PROTOCOL   *DriverDiagnostics,   OPTIONAL
#else
  IN EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration, OPTIONAL
  IN EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics,   OPTIONAL
#endif
  IN NET_LIB_DRIVER_UNLOAD              CustomizedUnload
  );

EFI_STATUS
NetLibInstallAllDriverProtocols (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL        *DriverBinding,
  IN EFI_HANDLE                         DriverBindingHandle,
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName,       OPTIONAL
  IN EFI_DRIVER_CONFIGURATION2_PROTOCOL *DriverConfiguration, OPTIONAL
  IN EFI_DRIVER_DIAGNOSTICS2_PROTOCOL   *DriverDiagnostics    OPTIONAL
#else
  IN EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration, OPTIONAL
  IN EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics    OPTIONAL
#endif
  );

#endif
