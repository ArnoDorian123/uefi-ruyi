/** @file
  Minimal GOP on the frame buffer the ABL leaves scanning out.

  Only the operations GraphicsConsole actually issues are implemented: video fill, buffer to
  video, video to buffer and video to video. Everything is a plain pixel loop, so there is no
  library state that can spin.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
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

STATIC
UINT32 *
FrameBuffer (
  VOID
  )
{
  return (UINT32 *)(UINTN)RUYI_FB_BASE;
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
  UINTN   DstDelta;
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
  DstDelta = Pitch * sizeof (UINT32);

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
      break;
    }

    case EfiBltBufferToVideo:
    {
      UINT8  *Src = (UINT8 *)BltBuffer + SourceY * SrcDelta + SourceX * sizeof (UINT32);

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem (Row, Src + Y * SrcDelta, Width * sizeof (UINT32));
      }
      break;
    }

    case EfiBltVideoToBltBuffer:
    {
      UINT8  *Dst = (UINT8 *)BltBuffer + DestinationY * SrcDelta + DestinationX * sizeof (UINT32);

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
      break;
    }

    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
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

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDevicePathProtocolGuid, &mDevicePath,
                  &gEfiGraphicsOutputProtocolGuid, &mGop,
                  NULL
                  );
  DEBUG ((EFI_D_ERROR, "RuyiGopDxe: install %ux%u -> %r\n", Width, Height, Status));

  return EFI_SUCCESS;
}
