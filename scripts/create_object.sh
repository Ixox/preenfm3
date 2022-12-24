#!/bin/bash


OBJCOPY_BIN="/c/ST/STM32CubeIDE_1.5.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.7-2018-q2-update.win32_1.5.0.202011040924/tools/bin/arm-none-eabi-objcopy.exe"


FIRMWARE_DIR=../firmware
FIRMWARE_RELEASE=Release


# BOOTLOADER_DIR=../bootloader
# BOOTLOADER_RELEASE=Release



versionFile="./Inc/version.h"

elfFile="${FIRMWARE_DIR}/${FIRMWARE_RELEASE}/preenfm3.elf"
# elfBootloaderFile=${BOOTLOADER_DIR}/${BOOTLOADER_RELEASE}/preenfm3\ bootloader.elf

read -r firmwareVersionLine < "${FIRMWARE_DIR}/${versionFile}"
# read -r bootloaderVersionLine < "${BOOTLOADER_DIR}/${versionFile}"

# firmware has a v
regexFirmwareVersion='\"v([0-9])\.([0-9]+[a-z0-9]*)\"'
# regexBootloader='\"([0-9])\.([0-9]*[b-z]*)\"'

[[ $firmwareVersionLine =~ $regexFirmwareVersion ]]
firmwareVersion=${BASH_REMATCH[1]}_${BASH_REMATCH[2]}

# [[ $bootloaderVersionLine =~ $regexBootloader ]]
# bootloaderVersion=${BASH_REMATCH[1]}_${BASH_REMATCH[2]}


buildFolder="pfm3_firmware_${firmwareVersion}"
mkdir -p $buildFolder 
rm -f ${buildFolder}/*

binFirmware="p3_${firmwareVersion}.bin"
binFirmwareWithPath="${buildFolder}/${binFirmware}"


binBootloaderFolder=pfm3_firmware_0_109
binBootloader=p3_boot_1_09.bin
binBootloaderWithPath="${binBootloaderFolder}/${binBootloader}"

# binBootloader="p3_boot_${bootloaderVersion}.bin"
# binBootloaderWithPath="${buildFolder}/${binBootloader}"

echo "Build                : ${FIRMWARE_RELEASE}"
echo "Firmware version     : ${firmwareVersion}"
echo "Build folder         : ${buildFolder}"
echo "Firmare file name    : ${binFirmware}"
echo "Bootloader file name : ${binBootloaderWithPath}"

${OBJCOPY_BIN} -R .ram_d2b -R .ram_d2 -R .ram_d1 -R .ram_d3 -R .instruction_ram -O binary ${elfFile} ${binFirmwareWithPath}
cp ${binBootloaderWithPath}  ${buildFolder}

echo ""
echo "bin created in ${buildFolder}"
ls -l ${buildFolder}
echo ""
echo "dfu-util -a0 -d 0x0483:0xdf11 -D ${binBootloader} -s 0x8000000" > ${buildFolder}/flash_bootloader.cmd
echo "dfu-util -a0 -d 0x0483:0xdf11 -D ${binFirmware} -s 0x8020000" > ${buildFolder}/flash_firmware.cmd



tee -a ${buildFolder}/README.txt <<EOF
In this folder you can find the following preenfm3 binaries:
- preenfm3 firmware   : ${binFirmware}
- preenfm3 bootloader : ${binBootloader}

In most situation the bootloder is not needed.
More information about flashing here :
https://github.com/Ixox/preenfm3/wiki/Upgrade-firmware-(Flash-bootloader)

Latest release : 
https://github.com/Ixox/preenfm3/releases

Xavier Hosxe
EOF

/f/dev/cygwin/bin/zip ${buildFolder}.zip ${buildFolder}/*.bin ${buildFolder}/*.cmd


