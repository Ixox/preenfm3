#!/bin/bash

BINPATH=/C/ST/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe

echo "=============================="
echo "  Flash preenfm2 bootloader"
echo "=============================="

${BINPATH} -c port=SWD -w ./p3_boot_1_09.bin 0x8000000

echo ""
echo ""
echo "=============================="
echo "  Flash preenfm2 firmware..."
echo "=============================="
 
${BINPATH} -c port=SWD -w ./p3_0_110.bin 0x8020000

