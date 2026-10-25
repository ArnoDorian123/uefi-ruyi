##
#  ruyi (Xiaomi MIX Flip, SM8650 / Lanai) platform description.
#
#  This file lives in Platforms/Xiaomi/shengPkg on purpose: the Include/*.inc files of that
#  package hardcode shengPkg/... INF paths, so keeping the directory name avoids rewriting
#  them. Only the platform identity below differs from sheng.dsc.
#
#  Required base: a fork of UniXNU/uefi-sheng (branch sheng). The Binaries submodule of that
#  tree provides Binaries/sheng, which the included FDF fragments reference.
#
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = ruyi
  PLATFORM_GUID                  = 7E3C2B5A-9F18-4C6D-8A21-5D4E3F2B1C07
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/ruyiPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = shengPkg/ruyi.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0 #
  # Advanced Logger exporter: Tools -> Advanced Logger shows the firmware log as QR frames.
  # Set to 0 if the Mu tree lacks AdvLoggerPkg / QrEncoderLib.
  #
  RUYI_ADV_LOG                   = 1

  #
  # 0 = Mu drivers only. ABL keeps scanning the splash buffer out, so the boot manager is
  #     visible without any vendor hardware driver. This is the M1 configuration.
  # 1 = also load the stock vendor hardware drivers (SMEM, clocks, RPMh, UFS, USB, charger).
  #
  # The name is kept as SHENG_VENDOR_HW because the inherited Include/*.inc files test it.
  #
  SHENG_VENDOR_HW                = 0

  #
  # SM8650 variant selector. 0 = SM8650-AB (ruyi).
  #
  SOC_TYPE                       = 0

!include LanaiPkg/LanaiPkg.dsc.inc

!if ($(USE_CUSTOM_DISPLAY_DRIVER) == 1) && ($(SHENG_VENDOR_HW) == 0)
  !error The stock DisplayDxe chain needs SHENG_VENDOR_HW = 1
!endif

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack: right above the FD window declared in Resources/Configs/ruyi.toml
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0xA760D000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Xiaomi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"ruyi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"ruyi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"ruyi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"ruyi"

  #
  # Simple Frame Buffer
  #
  # The ruyi primary (inner) panel is 1224 x 2912. ABL splash geometry for this board is not
  # recorded yet; if the debug log does not appear, try the secondary panel (1392 x 1208).
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1224
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2912
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferFontScale|3
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferDelay|30000

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"
!if $(SHENG_VENDOR_HW) == 0
  gQcomPkgTokenSpaceGuid.PcdBuildXblHobs|FALSE
!endif

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|shengPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|shengPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[Components]
  #
  # ACPI Tables
  #
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  shengPkg/Drivers/ShengDisplayDepexDxe/ShengDisplayDepexDxe.inf
  shengPkg/Drivers/ShengGopLogDxe/ShengGopLogDxe.inf
  shengPkg/Drivers/ShengBootMemoryDxe/ShengBootMemoryDxe.inf
  shengPkg/Drivers/ShengKeyInputDxe/ShengKeyInputDxe.inf
  shengPkg/Drivers/RuyiPaintDxe/RuyiPaintDxe.inf
  shengPkg/Drivers/RuyiGopDxe/RuyiGopDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf

[LibraryClasses]
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
