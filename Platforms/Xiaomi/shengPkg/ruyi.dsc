##
#  ruyi (Xiaomi MIX Flip, SM8650 / Lanai) platform description.
#
#  Derived from UniXNU/uefi-sheng Platform/Xiaomi/shengPkg/sheng.dsc.
#  Differences from sheng:
#    - SoC include is LanaiPkg (SM8650) instead of KailuaPkg (SM8550)
#    - package name, output directory and SMBIOS strings
#    - frame buffer geometry is the ruyi primary panel (1224 x 2912)
#
#  The Drivers/ tree is copied verbatim from shengPkg, so the INF paths below still use the
#  Sheng* directory names. Do not sed them.
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
  FLASH_DEFINITION               = ruyiPkg/ruyi.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # 0 = Mu drivers only. ABL keeps scanning the splash buffer out, so the boot manager is
  #     visible without any vendor hardware driver. This is the M1 configuration.
  # 1 = also load the stock vendor hardware drivers (SMEM, clocks, RPMh, UFS, USB, charger).
  #     Their initialization currently blanks the panel on sheng; not attempted for ruyi yet.
  #
  RUYI_VENDOR_HW                 = 0

  #
  # SM8650 variant selector. 0 = SM8650-AB (ruyi). Confirm against LanaiPkg before enabling
  # anything that depends on the -AC binning.
  #
  SOC_TYPE                       = 0

!include LanaiPkg/LanaiPkg.dsc.inc

!if ($(USE_CUSTOM_DISPLAY_DRIVER) == 1) && ($(RUYI_VENDOR_HW) == 0)
  !error The stock DisplayDxe chain needs RUYI_VENDOR_HW = 1
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
  # recorded anywhere yet; if the debug log does not appear, try the secondary panel
  # (1392 x 1208) or scale the values by 4 (the sheng values are the panel XML actives).
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1224
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2912
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32
  # PcdFrameBufferFontScale and PcdFrameBufferDelay are declared only by the Silicium tree
  # that ships with uefi-sheng. The Mu-Silicium upstream DEC does not declare them, and EDK II
  # rejects a DSC PCD that no DEC in the FDF dependency chain declares (error F001). Both are
  # debug-log cosmetics, so they stay out until the base switches to uefi-sheng.

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"
!if $(RUYI_VENDOR_HW) == 0
  # PlatformPeim otherwise scans the stock UEFI's leftover FD window and calls into its
  # scheduler protocol; with no vendor drivers nothing consumes those HOBs.
  gQcomPkgTokenSpaceGuid.PcdBuildXblHobs|FALSE
!endif

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|ruyiPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|ruyiPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[Components]
  #
  # ACPI Tables
  #
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  ruyiPkg/Drivers/ShengDisplayDepexDxe/ShengDisplayDepexDxe.inf
  ruyiPkg/Drivers/ShengGopLogDxe/ShengGopLogDxe.inf
  ruyiPkg/Drivers/ShengBootMemoryDxe/ShengBootMemoryDxe.inf
  ruyiPkg/Drivers/ShengKeyInputDxe/ShengKeyInputDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf

[LibraryClasses]
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
