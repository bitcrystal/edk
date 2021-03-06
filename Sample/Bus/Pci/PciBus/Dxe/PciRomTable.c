/*++

Copyright (c) 2004 - 2008, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  PciRomTable.c
  
Abstract:

  Option Rom Support for PCI Bus Driver

Revision History

--*/

#include "pcibus.h"
#include "PciRomTable.h"

typedef struct {
  EFI_HANDLE  ImageHandle;
  UINTN       Seg;
  UINT8       Bus;
  UINT8       Dev;
  UINT8       Func;
  UINT64      RomAddress;
  UINT64      RomLength;
} EFI_PCI_ROM_IMAGE_MAPPING;

static UINTN                      mNumberOfPciRomImages     = 0;
static UINTN                      mMaxNumberOfPciRomImages  = 0;
static EFI_PCI_ROM_IMAGE_MAPPING  *mRomImageTable           = NULL;

VOID
PciRomAddImageMapping (
  IN EFI_HANDLE  ImageHandle,
  IN UINTN       Seg,
  IN UINT8       Bus,
  IN UINT8       Dev,
  IN UINT8       Func,
  IN UINT64      RomAddress,
  IN UINT64      RomLength
  )
/*++

Routine Description:

  TODO: Add function description

Arguments:

  ImageHandle - TODO: add argument description
  Seg         - TODO: add argument description
  Bus         - TODO: add argument description
  Dev         - TODO: add argument description
  Func        - TODO: add argument description
  RomAddress  - TODO: add argument description
  RomLength   - TODO: add argument description

Returns:

  TODO: add return values

--*/
{
  EFI_PCI_ROM_IMAGE_MAPPING *TempMapping;

  if (mNumberOfPciRomImages >= mMaxNumberOfPciRomImages) {

    mMaxNumberOfPciRomImages += 0x20;

    TempMapping = NULL;
    TempMapping = EfiLibAllocatePool (mMaxNumberOfPciRomImages * sizeof (EFI_PCI_ROM_IMAGE_MAPPING));
    if (TempMapping == NULL) {
      return ;
    }

    EfiCopyMem (TempMapping, mRomImageTable, mNumberOfPciRomImages * sizeof (EFI_PCI_ROM_IMAGE_MAPPING));

    if (mRomImageTable != NULL) {
      gBS->FreePool (mRomImageTable);
    }

    mRomImageTable = TempMapping;
  }

  mRomImageTable[mNumberOfPciRomImages].ImageHandle = ImageHandle;
  mRomImageTable[mNumberOfPciRomImages].Seg         = Seg;
  mRomImageTable[mNumberOfPciRomImages].Bus         = Bus;
  mRomImageTable[mNumberOfPciRomImages].Dev         = Dev;
  mRomImageTable[mNumberOfPciRomImages].Func        = Func;
  mRomImageTable[mNumberOfPciRomImages].RomAddress  = RomAddress;
  mRomImageTable[mNumberOfPciRomImages].RomLength   = RomLength;
  mNumberOfPciRomImages++;
}


BOOLEAN
PciRomGetImageMapping (
  PCI_IO_DEVICE                       *PciIoDevice
  )
/*++

Routine Description:

Arguments:

Returns:

--*/
// TODO:    PciIoDevice - add argument and description to function comment
// TODO:    EFI_SUCCESS - add return value to function comment
{
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRootBridgeIo;
  UINTN                           Index;
  BOOLEAN                         Found;

  PciRootBridgeIo = PciIoDevice->PciRootBridgeIo;
  Found           = FALSE;

  for (Index = 0; Index < mNumberOfPciRomImages; Index++) {
    if (mRomImageTable[Index].Seg  == PciRootBridgeIo->SegmentNumber &&
        mRomImageTable[Index].Bus  == PciIoDevice->BusNumber         &&
        mRomImageTable[Index].Dev  == PciIoDevice->DeviceNumber      &&
        mRomImageTable[Index].Func == PciIoDevice->FunctionNumber    ) {

      Found = TRUE;

      //
      // There would be more than one entry in mRomImageTable for a specified PCI device
      //   if its OpROM contains EFI drivers.
      // One is an entry with ImageHandle = NULL inserted by LoadOpRom,
      //   the others are entries with ImageHandle != NULL inserted by ProcessOpRom.
      // Their RomAddress and RomLength are equal.
      //
      if (mRomImageTable[Index].ImageHandle != NULL) {
        AddDriver (PciIoDevice, mRomImageTable[Index].ImageHandle);
      } else {
        PciIoDevice->PciIo.RomImage = (VOID *) (UINTN) mRomImageTable[Index].RomAddress;
        PciIoDevice->PciIo.RomSize  = (UINTN) mRomImageTable[Index].RomLength;
        PciIoDevice->RomSize        = (UINTN) mRomImageTable[Index].RomLength;
      }
    }
  }

  return Found;
}
