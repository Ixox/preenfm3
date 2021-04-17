EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A2 23386 16535
encoding utf-8
Sheet 1 1
Title "preenfm3 176 pins / ID : 0001"
Date "2020-10-11"
Rev "v1.3"
Comp "Xavier Hosxe"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text Notes 7300 1150 0    118  ~ 0
TFT
$Comp
L Mechanical:MountingHole H1
U 1 1 5D1417A7
P 20050 1800
F 0 "H1" H 20150 1846 50  0000 L CNN
F 1 "MountingHole" H 20150 1755 50  0000 L CNN
F 2 "A_PFM3:FMHole3.5mm" H 20050 1800 50  0001 C CNN
F 3 "~" H 20050 1800 50  0001 C CNN
	1    20050 1800
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole H2
U 1 1 5D1437FC
P 20050 2000
F 0 "H2" H 20150 2046 50  0000 L CNN
F 1 "MountingHole" H 20150 1955 50  0000 L CNN
F 2 "A_PFM3:FMHole3.5mm" H 20050 2000 50  0001 C CNN
F 3 "~" H 20050 2000 50  0001 C CNN
	1    20050 2000
	1    0    0    -1  
$EndComp
Text Notes 20000 1500 0    118  ~ 0
Mounting Holes
Text Notes 12650 1200 0    118  ~ 0
To Control Board\n
$Comp
L Connector:Conn_01x18_Female J13
U 1 1 5E4A25E7
P 7050 2500
F 0 "J13" H 6942 1375 50  0000 C CNN
F 1 "TFT / SD" H 6942 1466 50  0000 C CNN
F 2 "A_PFM3:TFT_Ili_9341_TS" H 7050 2500 50  0001 C CNN
F 3 "~" H 7050 2500 50  0001 C CNN
	1    7050 2500
	-1   0    0    1   
$EndComp
$Comp
L Mechanical:MountingHole H3
U 1 1 5ED04A22
P 20050 2250
F 0 "H3" H 20150 2296 50  0000 L CNN
F 1 "MountingHole" H 20150 2205 50  0000 L CNN
F 2 "A_PFM3:FMHole3.5mm" H 20050 2250 50  0001 C CNN
F 3 "~" H 20050 2250 50  0001 C CNN
	1    20050 2250
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole H4
U 1 1 5ED04A2C
P 20050 2450
F 0 "H4" H 20150 2496 50  0000 L CNN
F 1 "MountingHole" H 20150 2405 50  0000 L CNN
F 2 "A_PFM3:FMHole3.5mm" H 20050 2450 50  0001 C CNN
F 3 "~" H 20050 2450 50  0001 C CNN
	1    20050 2450
	1    0    0    -1  
$EndComp
$Comp
L 0_pfm3:Logo L1
U 1 1 5ECBBE9E
P 20150 2800
F 0 "L1" H 20175 2846 50  0000 L CNN
F 1 "Logo" H 20175 2755 50  0000 L CNN
F 2 "A_PFM3:pfm_logo" H 20150 2800 50  0001 C CNN
F 3 "" H 20150 2800 50  0001 C CNN
	1    20150 2800
	1    0    0    -1  
$EndComp
$Comp
L 0_pfm3:Logo L2
U 1 1 5EE6C7BE
P 20850 2800
F 0 "L2" H 20875 2846 50  0000 L CNN
F 1 "Logo" H 20875 2755 50  0000 L CNN
F 2 "A_PFM3:pfm_logo" H 20850 2800 50  0001 C CNN
F 3 "" H 20850 2800 50  0001 C CNN
	1    20850 2800
	1    0    0    -1  
$EndComp
$Comp
L 0_pfm3:pfm3ControlBoard CB1
U 1 1 5EEB6021
P 13100 2400
F 0 "CB1" H 12933 3165 50  0000 C CNN
F 1 "pfm3ControlBoard" H 12933 3074 50  0000 C CNN
F 2 "A_PFM3:ControlBoard_To_TS" H 13100 2400 50  0001 C CNN
F 3 "" H 13100 2400 50  0001 C CNN
	1    13100 2400
	1    0    0    -1  
$EndComp
$EndSCHEMATC
