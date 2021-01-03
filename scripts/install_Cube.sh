#!/bin/bash

BINPATH=/home/xavier/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer.sh

echo "=============================="
echo "  Flash preenfm2 bootloader"
echo "=============================="

${BINPATH} -c port=SWD -w ./p3_boot_1.07.bin 0x8000000

echo ""
echo ""
echo "=============================="
echo "  Flash preenfm2 firmware..."
echo "=============================="
 
${BINPATH} -c port=SWD -w ./p3_0101z.bin 0x8020000

