/** @file
  Colour-band probe and GOP presence marker for the ruyi display bring-up.

  Paints bands straight into the physical buffer ABL scans out, then registers a protocol
  notify on the graphics output protocol. When a GOP appears it paints a white bar across the
  top rows, which is the only way to tell "GOP installed" from "nothing drew" on a black panel.

  Remove this driver once a real display driver exists.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

//
// /reserved-memory/splash_region in the ruyi device tree:
//   reg = <0x0 0xFC400000 0x0 0x2B00000>
//
#define RUYI_FB_BASE  0xFC400000ULL

//
// Upper bound on painted rows: keeps the write inside the first megabyte or so.
//
#define RUYI_PAINT_MAX_ROWS  320

//
// Rows used by the GOP presence bar.
//
#define RUYI_MARKER_ROWS  16

STATIC VOID  *mGopNotifyToken = NULL;

STATIC
VOID
PaintRows (
  IN UINT32  StartRow,
  IN UINT32  RowCount,
  IN UINT32  Colour
  )
{
  UINT32  Width = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  *Fb   = (UINT32 *)(UINTN)RUYI_FB_BASE;
  UINT32  X;
  UINT32  Y;

  for (Y = StartRow; Y < StartRow + RowCount; Y++) {
    for (X = 0; X < Width; X++) {
      Fb[(UINTN)Y * Width + X] = Colour;
    }
  }
}

STATIC
VOID
EFIAPI
GopReady (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

  if (EFI_ERROR (gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop))) {
    return;
  }

  DEBUG ((
    EFI_D_ERROR,
    "RuyiPaintDxe: GOP present %ux%u format %u\n",
    Gop->Mode->Info->HorizontalResolution,
    Gop->Mode->Info->VerticalResolution,
    Gop->Mode->Info->PixelFormat
    ));

  //
  // White bar across the first rows: GOP is live.
  //
  PaintRows (0, RUYI_MARKER_ROWS, 0x00FFFFFFU);
}

EFI_STATUS
EFIAPI
RuyiPaintEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT32     Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32     Height = FixedPcdGet32 (PcdFrameBufferHeight);
  UINT32     Rows;
  EFI_EVENT  Event;

  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: bad geometry %ux%u\n", Width, Height));
    return EFI_SUCCESS;
  }

  Rows = Height / 8;
  if (Rows > RUYI_PAINT_MAX_ROWS) {
    Rows = RUYI_PAINT_MAX_ROWS;
  }

  //
  // Three bands, painted before any GOP exists.
  //
  PaintRows (0,           Rows / 3,             0x000000FFU);   // red
  PaintRows (Rows / 3,    Rows / 3,             0x0000FF00U);   // green
  PaintRows (2 * Rows / 3, Rows - 2 * (Rows / 3), 0x00FF0000U); // blue

  //
  // Black bar on top: overwritten white by GopReady() when a GOP shows up.
  //
  PaintRows (0, RUYI_MARKER_ROWS, 0x00000000U);

  if (!EFI_ERROR (gBS->CreateEvent (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, GopReady, NULL, &Event))) {
    gBS->RegisterProtocolNotify (&gEfiGraphicsOutputProtocolGuid, Event, &mGopNotifyToken);
  }

  DEBUG ((
    EFI_D_ERROR,
    "RuyiPaintDxe: painted %u rows of %ux%u at 0x%lx\n",
    Rows,
    Width,
    Height,
    RUYI_FB_BASE
    ));

  return EFI_SUCCESS;
}
