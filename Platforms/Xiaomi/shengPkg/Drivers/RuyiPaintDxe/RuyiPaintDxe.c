/** @file
  Liveness probe for the ruyi display bring-up.

  Arms a one second periodic timer whose callback paints a magenta block that moves down the
  screen. The boot manager clears the screen once when it binds the graphics output protocol,
  so a single paint would be wiped; a periodic repaint survives that and answers a question no
  static probe can: "is the firmware still alive and are timers still firing".

  If the block keeps moving, the platform is healthy and the missing piece is text output.
  If it stops, execution is blocked somewhere after this driver returned.

  Remove this driver once a real display driver exists.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

//
// /reserved-memory/splash_region in the ruyi device tree:
//   reg = <0x0 0xFC400000 0x0 0x2B00000>
//
#define RUYI_FB_BASE  0xFC400000ULL

//
// Height of the moving block in pixels.
//
#define RUYI_BLOCK_ROWS  40

STATIC EFI_EVENT  mTimerEvent = NULL;
STATIC UINT32     mTick       = 0;

STATIC
VOID
PaintBlock (
  IN UINT32  Row
  )
{
  UINT32  Width = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  *Fb   = (UINT32 *)(UINTN)RUYI_FB_BASE;
  UINT32  X;
  UINT32  Y;

  for (Y = Row; Y < Row + RUYI_BLOCK_ROWS; Y++) {
    for (X = 0; X < Width; X++) {
      Fb[(UINTN)Y * Width + X] = 0x00FF00FFU;   // magenta, BGRX
    }
  }
}

STATIC
VOID
EFIAPI
Tick (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINT32  Height = FixedPcdGet32 (PcdFrameBufferHeight);
  UINT32  Span   = (Height > 2 * RUYI_BLOCK_ROWS) ? (Height - 2 * RUYI_BLOCK_ROWS) : 1;

  PaintBlock ((mTick * RUYI_BLOCK_ROWS) % Span);
  mTick++;
}

EFI_STATUS
EFIAPI
RuyiPaintEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  if ((FixedPcdGet32 (PcdFrameBufferWidth) == 0) || (FixedPcdGet32 (PcdFrameBufferHeight) == 0)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: frame buffer geometry is zero\n"));
    return EFI_SUCCESS;
  }

  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, Tick, NULL, &mTimerEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: CreateEvent failed %r\n", Status));
    return EFI_SUCCESS;
  }

  //
  // 10 000 000 * 100 ns = 1 s.
  //
  Status = gBS->SetTimer (mTimerEvent, TimerPeriodic, 10000000);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: SetTimer failed %r\n", Status));
    return EFI_SUCCESS;
  }

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: 1 Hz liveness timer armed\n"));
  return EFI_SUCCESS;
}
