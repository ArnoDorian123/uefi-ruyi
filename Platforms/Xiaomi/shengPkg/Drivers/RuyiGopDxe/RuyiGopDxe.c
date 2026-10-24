/** @file
  Minimal GOP on the frame buffer the ABL leaves scanning out.

  Only the operations GraphicsConsole actually issues are implemented: video fill, buffer to
  video, video to buffer and video to video. Everything is a plain pixel/CopyMem loop, so there
  is no library state that can spin.

  The frame buffer is reserved RAM mapped as normal cacheable memory, so every write is followed
  by a cache write-back for the touched rows. Without it the first render of a screen appears as
  scattered dots and only becomes correct once the cache drains by itself.

  A one second heartbeat near the top-left corner tells a hung boot manager apart from a kernel
  that died without printing anything.

  On load it paints a dark background plus one 100x100 square in each corner and then installs
  the protocol. Four squares exactly in the corners prove the frame buffer geometry is right;
  any displacement tells how far off the declared width or height is.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>

#define RUYI_FB_BASE  0xFC400000ULL

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH        Vendor;
  EFI_DEVICE_PATH_PROTOCOL  End;
} RUYI_DISPLAY_DEVICE_PATH;
#pragma pack()

STATIC RUYI_DISPLAY_DEVICE_PATH mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
    { 0x1B7E5C2A, 0x5F3D, 0x4B9A, { 0x9C, 0x21, 0x7D, 0x6E, 0x4F, 0x8A, 0x0B, 0x13 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  mModeInfo;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    mMode;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL         mGop;

STATIC EFI_EVENT  mBlinkEvent = NULL;
STATIC BOOLEAN    mBlinkOn    = FALSE;

STATIC
UINT32 *
FrameBuffer (
  VOID
  )
{
  return (UINT32 *)(UINTN)RUYI_FB_BASE;
}

/**
  Write back the given rows so the display engine sees them.

  @param[in]  FirstRow  First row that was written.
  @param[in]  RowCount  Number of rows that were written.
**/
STATIC
VOID
FlushRows (
  IN UINTN  FirstRow,
  IN UINTN  RowCount
  )
{
  UINTN  Pitch  = mModeInfo.PixelsPerScanLine * sizeof (UINT32);
  UINTN  Offset = FirstRow * Pitch;
  UINTN  Length = RowCount * Pitch;

  if ((Length == 0) || (Offset >= mMode.FrameBufferSize)) {
    return;
  }

  if (Offset + Length > mMode.FrameBufferSize) {
    Length = mMode.FrameBufferSize - Offset;
  }

  WriteBackDataCacheRange ((VOID *)(UINTN)(RUYI_FB_BASE + Offset), Length);
}

STATIC
EFI_STATUS
EFIAPI
GopQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  if (ModeNumber != 0) {
    return EFI_UNSUPPORTED;
  }

  *Info = AllocateCopyPool (sizeof (mModeInfo), &mModeInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (mModeInfo);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GopSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN UINT32                        ModeNumber
  )
{
  return (ModeNumber == 0) ? EFI_SUCCESS : EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
EFIAPI
GopBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta OPTIONAL
  )
{
  UINT32  *Fb    = FrameBuffer ();
  UINTN   Pitch  = mModeInfo.PixelsPerScanLine;
  UINTN   SrcDelta;
  UINTN   X;
  UINTN   Y;

  if ((Width == 0) || (Height == 0)) {
    return EFI_SUCCESS;
  }

  if ((DestinationX + Width) > mModeInfo.HorizontalResolution) {
    return EFI_INVALID_PARAMETER;
  }

  if ((DestinationY + Height) > mModeInfo.VerticalResolution) {
    return EFI_INVALID_PARAMETER;
  }

  SrcDelta = (Delta == 0) ? Width * sizeof (UINT32) : Delta;

  switch (BltOperation) {
    case EfiBltVideoFill:
    {
      UINT32  Colour = *(UINT32 *)BltBuffer;

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
        for (X = 0; X < Width; X++) {
          Row[X] = Colour;
        }
      }

      FlushRows (DestinationY, Height);
      break;
    }

    case EfiBltBufferToVideo:
    {
      UINT8  *Src = (UINT8 *)BltBuffer + SourceY * SrcDelta + SourceX * sizeof (UINT32);

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem (Row, Src + Y * SrcDelta, Width * sizeof (UINT32));
      }

      FlushRows (DestinationY, Height);
      break;
    }

    case EfiBltVideoToBltBuffer:
    {
      UINT8  *Dst = (UINT8 *)BltBuffer + DestinationY * SrcDelta + DestinationX * sizeof (UINT32);

      //
      // Only read from the frame buffer here. Invalidate the source rows first so the copy sees
      // what the display engine sees, then leave the cache clean rather than dirty.
      //
      InvalidateDataCacheRange (
        (VOID *)(UINTN)(RUYI_FB_BASE + SourceY * Pitch * sizeof (UINT32)),
        Height * Pitch * sizeof (UINT32)
        );

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (SourceY + Y) * Pitch + SourceX;
        CopyMem (Dst + Y * SrcDelta, Row, Width * sizeof (UINT32));
      }

      break;
    }

    case EfiBltVideoToVideo:
    {
      for (Y = 0; Y < Height; Y++) {
        UINT32  *Src = Fb + (SourceY + Y) * Pitch + SourceX;
        UINT32  *Dst = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem (Dst, Src, Width * sizeof (UINT32));
      }

      FlushRows (DestinationY, Height);
      break;
    }

    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  One second heartbeat near the top-left corner.

  A 64x64 block alternating between red and white, below the colour bars the earlier geometry
  probe left behind. While it keeps toggling, the firmware is still executing. When it freezes,
  control has left UEFI (or interrupts are off), which tells a hung boot manager apart from a
  kernel that died without printing anything.
**/
STATIC
VOID
EFIAPI
BlinkTick (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINT32  *Fb    = FrameBuffer ();
  UINTN   Pitch  = mModeInfo.PixelsPerScanLine;
  UINT32  Colour = mBlinkOn ? 0x000000FFU : 0x00FFFFFFU;   // red / white
  UINTN   X;
  UINTN   Y;

  mBlinkOn = !mBlinkOn;

  for (Y = 0; Y < 64; Y++) {
    for (X = 0; X < 64; X++) {
      Fb[(1000 + Y) * Pitch + X] = Colour;
    }
  }

  WriteBackDataCacheRange (
    (VOID *)(UINTN)(RUYI_FB_BASE + 1000 * Pitch * sizeof (UINT32)),
    Pitch * 64 * sizeof (UINT32)
    );
}

/**
  Paint the geometry self test: dark background, one 100x100 square in each corner.
**/
STATIC
VOID
DrawGeometryTest (
  IN UINT32  Width,
  IN UINT32  Height
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Dark  = { 0x40, 0x20, 0x10, 0x00 };   // B,G,R,X
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  White = { 0xFF, 0xFF, 0xFF, 0x00 };
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Red   = { 0x00, 0x00, 0xFF, 0x00 };
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Green = { 0x00, 0xFF, 0x00, 0x00 };
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Blue  = { 0xFF, 0x00, 0x00, 0x00 };

  GopBlt (&mGop, &Dark,  EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);
  GopBlt (&mGop, &White, EfiBltVideoFill, 0, 0, 0, 0, 100, 100, 0);
  GopBlt (&mGop, &Red,   EfiBltVideoFill, 0, 0, Width - 100, 0, 100, 100, 0);
  GopBlt (&mGop, &Green, EfiBltVideoFill, 0, 0, 0, Height - 100, 100, 100, 0);
  GopBlt (&mGop, &Blue,  EfiBltVideoFill, 0, 0, Width - 100, Height - 100, 100, 100, 0);
}

EFI_STATUS
EFIAPI
RuyiGopEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT32      Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32      Height = FixedPcdGet32 (PcdFrameBufferHeight);

  if ((Width < 640) || (Height < 475)) {
    DEBUG ((EFI_D_ERROR, "RuyiGopDxe: geometry %ux%u too small\n", Width, Height));
    return EFI_SUCCESS;
  }

  ZeroMem (&mModeInfo, sizeof (mModeInfo));
  mModeInfo.Version              = 0;
  mModeInfo.HorizontalResolution = Width;
  mModeInfo.VerticalResolution   = Height;
  mModeInfo.PixelFormat          = PixelBlueGreenRedReserved8BitPerColor;
  mModeInfo.PixelsPerScanLine    = Width;

  ZeroMem (&mMode, sizeof (mMode));
  mMode.MaxMode         = 1;
  mMode.Mode            = 0;
  mMode.Info            = &mModeInfo;
  mMode.SizeOfInfo      = sizeof (mModeInfo);
  mMode.FrameBufferBase = RUYI_FB_BASE;
  mMode.FrameBufferSize = (UINTN)Width * Height * sizeof (UINT32);

  mGop.QueryMode = GopQueryMode;
  mGop.SetMode   = GopSetMode;
  mGop.Blt       = GopBlt;
  mGop.Mode      = &mMode;

  //
  // Geometry proof, painted before the protocol is published so it is visible even if a
  // consumer later clears the screen.
  //
  DrawGeometryTest (Width, Height);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDevicePathProtocolGuid, &mDevicePath,
                  &gEfiGraphicsOutputProtocolGuid, &mGop,
                  NULL
                  );
  DEBUG ((EFI_D_ERROR, "RuyiGopDxe: install %ux%u -> %r\n", Width, Height, Status));

  //
  // Heartbeat: 64x64 pixels near the top-left corner, toggling once per second.
  //
  if (!EFI_ERROR (gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, BlinkTick, NULL, &mBlinkEvent))) {
    gBS->SetTimer (mBlinkEvent, TimerPeriodic, 10000000);
    DEBUG ((EFI_D_ERROR, "RuyiGopDxe: heartbeat armed\n"));
  } else {
    DEBUG ((EFI_D_ERROR, "RuyiGopDxe: heartbeat timer failed\n"));
  }

  return EFI_SUCCESS;
}
