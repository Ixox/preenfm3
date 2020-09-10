/*
 * Copyright 2019
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "usbd_midi_if.h"
#include "preenfm3.h"



extern USBD_HandleTypeDef hUsbDeviceFS;

// Only one function to register
static int8_t dataReceived(uint8_t* buffer);

USBD_MIDI_ItfTypeDef USBD_MIDI_fops_FS =
{
		dataReceived
};


static int8_t dataReceived(uint8_t* buffer) {
	preenfm3_usbDataReceive(buffer);

	return USBD_OK;
}

