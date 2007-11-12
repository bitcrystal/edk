/*++

Copyright (c) 2006, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             


Module Name:

 SmartTimer.c

Abstract:

  Timer Architectural Protocol as defined in the DXE CIS

--*/

#include "SmartTimer.h"

//
// The handle onto which the Timer Architectural Protocol will be installed
//
EFI_HANDLE                mTimerHandle = NULL;

//
// The Timer Architectural Protocol that this driver produces
//
EFI_TIMER_ARCH_PROTOCOL   mTimer = {
  TimerDriverRegisterHandler,
  TimerDriverSetTimerPeriod,
  TimerDriverGetTimerPeriod,
  TimerDriverGenerateSoftInterrupt
};

//
// Pointer to the CPU Architectural Protocol instance
//
EFI_CPU_ARCH_PROTOCOL     *mCpu;

//
// Pointer to the CPU I/O Protocol instance
//
EFI_CPU_IO_PROTOCOL       *mCpuIo;

EFI_ACPI_DESCRIPTION      *mAcpiDescription;

//
// Pointer to the Legacy 8259 Protocol instance
//
EFI_LEGACY_8259_PROTOCOL  *mLegacy8259;

//
// The notification function to call on every timer interrupt.
// A bug in the compiler prevents us from initializing this here.
//
volatile EFI_TIMER_NOTIFY mTimerNotifyFunction;

//
// The current period of the timer interrupt
//
volatile UINT64           mTimerPeriod = 0;

//
// The time of twice timer interrupt duration
//
volatile UINTN            mPreAcpiTick = 0;

//
// Worker Functions
//
VOID
SetPitCount (
  IN UINT16  Count
  )
/*++

Routine Description:

  Sets the counter value for Timer #0 in a legacy 8254 timer.

Arguments:

  Count - The 16-bit counter value to program into Timer #0 of the legacy 8254 timer.

Returns: 

  None

--*/
{
  UINT8 Data;

  Data = 0x36;
  mCpuIo->Io.Write (mCpuIo, EfiCpuIoWidthUint8, TIMER_CONTROL_PORT, 1, &Data);
  mCpuIo->Io.Write (mCpuIo, EfiCpuIoWidthFifoUint8, TIMER0_COUNT_PORT, 2, &Count);
}

UINT32
GetAcpiTick (
  VOID
  )
/*++

Routine Description:

  Get the current ACPI counter's value

Arguments:

  None

Returns: 

  The value of the counter

--*/
{
  UINT32  Tick;

  switch (mAcpiDescription->PM_TMR_BLK.AddressSpaceId) {
  case ACPI_ADDRESS_ID_IO:
    mCpuIo->Io.Read (mCpuIo, EfiCpuIoWidthUint32, mAcpiDescription->PM_TMR_BLK.Address, 1, &Tick);
    break;
  case ACPI_ADDRESS_ID_MEMORY:
    mCpuIo->Mem.Read (mCpuIo, EfiCpuIoWidthUint32, mAcpiDescription->PM_TMR_BLK.Address, 1, &Tick);
    break;
  default:
    ASSERT (FALSE);
    Tick = 0;
    break;
  }
  
  //
  // Only 23:0 bit is true value
  //
  if (mAcpiDescription->TMR_VAL_EXT == 0) {
    Tick &= 0xffffff;
  }
  return Tick;
}

UINT64
MeasureTimeLost (
  IN UINT64             TimePeriod
  )
/*++

Routine Description:

  Measure the 8254 timer interrupt use the ACPI time counter

Arguments:

    TimePeriod - 8254 timer period

Returns: 

  The real system time pass between the sequence 8254 timer interrupt

--*/
{
  UINT32  CurrentTick;
  UINT64  EndTick;
  UINT64  LostTime;
  UINT64  MaxValue;

  CurrentTick = GetAcpiTick ();
  EndTick     = (UINT64)CurrentTick;

  if (CurrentTick < mPreAcpiTick) {
    if (mAcpiDescription->TMR_VAL_EXT == 0) {
      MaxValue = 0x1000000;
    } else {
      MaxValue = 0x100000000;
    }
    EndTick = (UINT64)CurrentTick + MaxValue;
  }
  //
  // The calculation of the lost system time should be very accurate, we use
  // the shift calcu to make sure the value's accurate:
  // the origenal formula is:
  //                      (EndTick - mPreAcpiTick) * 10,000,000
  //      LostTime = -----------------------------------------------
  //                   (3,579,545 Hz / 1,193,182 Hz) * 1,193,182 Hz
  //
  // Note: the 3,579,545 Hz is the ACPI timer's clock;
  //       the 1,193,182 Hz is the 8254 timer's clock;
  //
  LostTime = RShiftU64 (
              MultU64x32 ((UINT64) (EndTick - mPreAcpiTick),
              46869689) + 0x00FFFFFF,
              24
              );

  if (LostTime != 0) {
    mPreAcpiTick = CurrentTick;
  }

  return LostTime;
}

VOID
EFIAPI
TimerInterruptHandler (
  IN EFI_EXCEPTION_TYPE   InterruptType,
  IN EFI_SYSTEM_CONTEXT   SystemContext
  )
/*++

Routine Description:

  8254 Timer #0 Interrupt Handler

Arguments:

  InterruptType - The type of interrupt that occured

  SystemContext - A pointer to the system context when the interrupt occured

Returns: 

  None

--*/
{
  EFI_TPL OriginalTPL;

  OriginalTPL = gBS->RaiseTPL (EFI_TPL_HIGH_LEVEL);

  mLegacy8259->EndOfInterrupt (mLegacy8259, Efi8259Irq0);

  if (mTimerNotifyFunction) {
    //
    // If we have the timer interrupt miss, then we use
    // the platform ACPI time counter to retrieve the time lost
    //
    mTimerNotifyFunction (MeasureTimeLost (mTimerPeriod));
  }

  gBS->RestoreTPL (OriginalTPL);
}

EFI_STATUS
EFIAPI
TimerDriverRegisterHandler (
  IN EFI_TIMER_ARCH_PROTOCOL  *This,
  IN EFI_TIMER_NOTIFY         NotifyFunction
  )
/*++

Routine Description:

  This function registers the handler NotifyFunction so it is called every time 
  the timer interrupt fires.  It also passes the amount of time since the last 
  handler call to the NotifyFunction.  If NotifyFunction is NULL, then the 
  handler is unregistered.  If the handler is registered, then EFI_SUCCESS is 
  returned.  If the CPU does not support registering a timer interrupt handler, 
  then EFI_UNSUPPORTED is returned.  If an attempt is made to register a handler 
  when a handler is already registered, then EFI_ALREADY_STARTED is returned.  
  If an attempt is made to unregister a handler when a handler is not registered, 
  then EFI_INVALID_PARAMETER is returned.  If an error occurs attempting to 
  register the NotifyFunction with the timer interrupt, then EFI_DEVICE_ERROR 
  is returned.

Arguments:

  This           - The EFI_TIMER_ARCH_PROTOCOL instance.

  NotifyFunction - The function to call when a timer interrupt fires.  This 
                   function executes at TPL_HIGH_LEVEL.  The DXE Core will 
                   register a handler for the timer interrupt, so it can know 
                   how much time has passed.  This information is used to 
                   signal timer based events.  NULL will unregister the handler.

Returns: 

  EFI_SUCCESS           - The timer handler was registered.

  EFI_UNSUPPORTED       - The platform does not support timer interrupts.

  EFI_ALREADY_STARTED   - NotifyFunction is not NULL, and a handler is already 
                          registered.

  EFI_INVALID_PARAMETER - NotifyFunction is NULL, and a handler was not 
                          previously registered.

  EFI_DEVICE_ERROR      - The timer handler could not be registered.

--*/
{
  //
  // Check for invalid parameters
  //
  if (NotifyFunction == NULL && mTimerNotifyFunction == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (NotifyFunction != NULL && mTimerNotifyFunction != NULL) {
    return EFI_ALREADY_STARTED;
  }

  mTimerNotifyFunction = NotifyFunction;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TimerDriverSetTimerPeriod (
  IN EFI_TIMER_ARCH_PROTOCOL  *This,
  IN UINT64                   TimerPeriod
  )
/*++

Routine Description:

  This function adjusts the period of timer interrupts to the value specified 
  by TimerPeriod.  If the timer period is updated, then the selected timer 
  period is stored in EFI_TIMER.TimerPeriod, and EFI_SUCCESS is returned.  If 
  the timer hardware is not programmable, then EFI_UNSUPPORTED is returned.  
  If an error occurs while attempting to update the timer period, then the 
  timer hardware will be put back in its state prior to this call, and 
  EFI_DEVICE_ERROR is returned.  If TimerPeriod is 0, then the timer interrupt 
  is disabled.  This is not the same as disabling the CPU's interrupts.  
  Instead, it must either turn off the timer hardware, or it must adjust the 
  interrupt controller so that a CPU interrupt is not generated when the timer 
  interrupt fires. 

Arguments:

  This        - The EFI_TIMER_ARCH_PROTOCOL instance.

  TimerPeriod - The rate to program the timer interrupt in 100 nS units.  If 
                the timer hardware is not programmable, then EFI_UNSUPPORTED is 
                returned.  If the timer is programmable, then the timer period 
                will be rounded up to the nearest timer period that is supported 
                by the timer hardware.  If TimerPeriod is set to 0, then the 
                timer interrupts will be disabled.

Returns: 

  EFI_SUCCESS      - The timer period was changed.

  EFI_UNSUPPORTED  - The platform cannot change the period of the timer interrupt.

  EFI_DEVICE_ERROR - The timer period could not be changed due to a device error.

--*/
{
  UINT64  TimerCount;

  //
  //  The basic clock is 1.19318 MHz or 0.119318 ticks per 100 ns.
  //  TimerPeriod * 0.119318 = 8254 timer divisor. Using integer arithmetic
  //  TimerCount = (TimerPeriod * 119318)/1000000.
  //
  //  Round up to next highest integer. This guarantees that the timer is
  //  equal to or slightly longer than the requested time.
  //  TimerCount = ((TimerPeriod * 119318) + 500000)/1000000
  //
  // Note that a TimerCount of 0 is equivalent to a count of 65,536
  //
  // Since TimerCount is limited to 16 bits for IA32, TimerPeriod is limited
  // to 20 bits.
  //
  if (TimerPeriod == 0) {
    //
    // Disable timer interrupt for a TimerPeriod of 0
    //
    mLegacy8259->DisableIrq (mLegacy8259, Efi8259Irq0);
  } else {
    //
    // Convert TimerPeriod into 8254 counts
    //
    TimerCount = DivU64x32 (MultU64x32 (119318, (UINTN) TimerPeriod) + 500000, 1000000, 0);

    //
    // Check for overflow
    //
    if (TimerCount >= 65536) {
      TimerCount = 0;
      if (TimerPeriod >= DEFAULT_TIMER_TICK_DURATION) {
        TimerPeriod = DEFAULT_TIMER_TICK_DURATION;
      }
    }
    //
    // Program the 8254 timer with the new count value
    //
    SetPitCount ((UINT16) TimerCount);

    //
    // Enable timer interrupt
    //
    mLegacy8259->EnableIrq (mLegacy8259, Efi8259Irq0, FALSE);
  }
  //
  // Save the new timer period
  //
  mTimerPeriod = TimerPeriod;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TimerDriverGetTimerPeriod (
  IN EFI_TIMER_ARCH_PROTOCOL   *This,
  OUT UINT64                   *TimerPeriod
  )
/*++

Routine Description:

  This function retrieves the period of timer interrupts in 100 ns units, 
  returns that value in TimerPeriod, and returns EFI_SUCCESS.  If TimerPeriod 
  is NULL, then EFI_INVALID_PARAMETER is returned.  If a TimerPeriod of 0 is 
  returned, then the timer is currently disabled.

Arguments:

  This        - The EFI_TIMER_ARCH_PROTOCOL instance.

  TimerPeriod - A pointer to the timer period to retrieve in 100 ns units.  If 
                0 is returned, then the timer is currently disabled.

Returns: 

  EFI_SUCCESS           - The timer period was returned in TimerPeriod.

  EFI_INVALID_PARAMETER - TimerPeriod is NULL.

--*/
{
  if (TimerPeriod == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *TimerPeriod = mTimerPeriod;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TimerDriverGenerateSoftInterrupt (
  IN EFI_TIMER_ARCH_PROTOCOL  *This
  )
/*++

Routine Description:

  This function generates a soft timer interrupt. If the platform does not support soft 
  timer interrupts, then EFI_UNSUPPORTED is returned. Otherwise, EFI_SUCCESS is returned. 
  If a handler has been registered through the EFI_TIMER_ARCH_PROTOCOL.RegisterHandler() 
  service, then a soft timer interrupt will be generated. If the timer interrupt is 
  enabled when this service is called, then the registered handler will be invoked. The 
  registered handler should not be able to distinguish a hardware-generated timer 
  interrupt from a software-generated timer interrupt.

Arguments:

  This  -  The EFI_TIMER_ARCH_PROTOCOL instance.

Returns: 

  EFI_SUCCESS       - The soft timer interrupt was generated.

  EFI_UNSUPPORTEDT  - The platform does not support the generation of soft timer interrupts.

--*/
{
  EFI_STATUS  Status;
  UINT16      IRQMask;
  EFI_TPL     OriginalTPL;

  //
  // If the timer interrupt is enabled, then the registered handler will be invoked.
  //
  Status = mLegacy8259->GetMask (mLegacy8259, NULL, NULL, &IRQMask, NULL);
  ASSERT_EFI_ERROR (Status);
  if ((IRQMask & 0x1) == 0) {
    //
    // Invoke the registered handler
    //
    OriginalTPL = gBS->RaiseTPL (EFI_TPL_HIGH_LEVEL);

    if (mTimerNotifyFunction) {
      //
      // We use the platform ACPI time counter to determine
      // the amount of time that has passed
      //
      mTimerNotifyFunction (MeasureTimeLost (mTimerPeriod));
    }

    gBS->RestoreTPL (OriginalTPL);
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_ACPI_DESCRIPTION *
GetAcpiDescription (
  VOID
  )
{
  VOID                                     *HobList;
  EFI_STATUS                               Status;
  EFI_ACPI_DESCRIPTION                     *AcpiDescription;
  UINTN                                    BufferSize;

  //
  // Get Hob List from configuration table
  //
  Status = EfiLibGetSystemConfigurationTable (&gEfiHobListGuid, &HobList);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Get AcpiDescription Hob
  //
  AcpiDescription = NULL;
  Status = GetNextGuidHob (&HobList, &gEfiAcpiDescriptionGuid, &AcpiDescription, &BufferSize);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return AcpiDescription;
}

EFI_DRIVER_ENTRY_POINT (TimerDriverInitialize)

EFI_STATUS
EFIAPI
TimerDriverInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
/*++

Routine Description:

  Initialize the Timer Architectural Protocol driver

Arguments:

  ImageHandle - ImageHandle of the loaded driver

  SystemTable - Pointer to the System Table

Returns:

  EFI_SUCCESS           - Timer Architectural Protocol created

  EFI_OUT_OF_RESOURCES  - Not enough resources available to initialize driver.
  
  EFI_DEVICE_ERROR      - A device error occured attempting to initialize the driver.

--*/
{
  EFI_STATUS  Status;
  UINT32      TimerVector;

  EfiInitializeDriverLib (ImageHandle, SystemTable);

  //
  // Initialize AcpiDescription
  //
  mAcpiDescription = GetAcpiDescription ();
  if (mAcpiDescription == NULL) {
    return EFI_UNSUPPORTED;
  }

  if ((mAcpiDescription->PM_TMR_LEN != 4) ||
      (mAcpiDescription->PM_TMR_BLK.Address == 0) ||
      ((mAcpiDescription->PM_TMR_BLK.AddressSpaceId != ACPI_ADDRESS_ID_IO) &&
       (mAcpiDescription->PM_TMR_BLK.AddressSpaceId != ACPI_ADDRESS_ID_MEMORY))) {
    return EFI_UNSUPPORTED;
  }

  DEBUG ((EFI_D_ERROR, "ACPI Timer Base - %lx\n", mAcpiDescription->PM_TMR_BLK.Address));

  //
  // Initialize the pointer to our notify function.
  //
  mTimerNotifyFunction = NULL;

  //
  // Make sure the Timer Architectural Protocol is not already installed in the system
  //
  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gEfiTimerArchProtocolGuid);

  //
  // Find the CPU I/O Protocol.  ASSERT if not found.
  //
  Status = gBS->LocateProtocol (&gEfiCpuIoProtocolGuid, NULL, (VOID **) &mCpuIo);
  ASSERT_EFI_ERROR (Status);

  //
  // Find the CPU architectural protocol.  ASSERT if not found.
  //
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **) &mCpu);
  ASSERT_EFI_ERROR (Status);

  //
  // Find the Legacy8259 protocol.  ASSERT if not found.
  //
  Status = gBS->LocateProtocol (&gEfiLegacy8259ProtocolGuid, NULL, (VOID **) &mLegacy8259);
  ASSERT_EFI_ERROR (Status);

  //
  // Force the timer to be disabled
  //
  Status = TimerDriverSetTimerPeriod (&mTimer, 0);
  ASSERT_EFI_ERROR (Status);

  //
  // Get the interrupt vector number corresponding to IRQ0 from the 8259 driver
  //
  TimerVector = 0;
  Status      = mLegacy8259->GetVector (mLegacy8259, Efi8259Irq0, (UINT8 *) &TimerVector);
  ASSERT_EFI_ERROR (Status);

  //
  // Install interrupt handler for 8254 Timer #0 (ISA IRQ0)
  //
  Status = mCpu->RegisterInterruptHandler (mCpu, TimerVector, TimerInterruptHandler);
  ASSERT_EFI_ERROR (Status);

  //
  // Force the timer to be enabled at its default period
  //
  Status = TimerDriverSetTimerPeriod (&mTimer, DEFAULT_TIMER_TICK_DURATION);
  ASSERT_EFI_ERROR (Status);

  //
  // Begin the ACPI timer counter
  //
  mPreAcpiTick = GetAcpiTick ();

  //
  // Install the Timer Architectural Protocol onto a new handle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mTimerHandle,
                  &gEfiTimerArchProtocolGuid,
                  &mTimer,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
