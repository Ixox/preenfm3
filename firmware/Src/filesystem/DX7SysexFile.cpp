/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
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


#include "DX7SysexFile.h"

__attribute__((section(".ram_d2b")))  uint8_t dx7PackedPatch[DX7_PACKED_PATCH_SIZED];
__attribute__((section(".ram_d2b"))) struct PFM3File dx7BankAlloc[NUMBEROFDX7BANKS];


DX7SysexFile::DX7SysexFile() {
	numberOfFilesMax_ = NUMBEROFDX7BANKS;
    dx7Bank = dx7BankAlloc;
	myFiles_ = dx7Bank;
}

DX7SysexFile::~DX7SysexFile() {
}

const char* DX7SysexFile::getFolderName() {
	return DX7_DIR;
}

uint8_t* DX7SysexFile::dx7LoadPatch(const struct PFM3File* bank, int patchNumber) {
	const char* fullBankName = getFullName(bank->name);
    int result = load(fullBankName, patchNumber * DX7_PACKED_PATCH_SIZED + 6,  (void*)dx7PackedPatch, DX7_PACKED_PATCH_SIZED);
    if (result != DX7_PACKED_PATCH_SIZED) {
    	return (uint8_t*)0;
    }
    return dx7PackedPatch;
}

bool DX7SysexFile::isCorrectFile(char *name, int size)  {
	// DX7 Dump sysex size is 4104
	if (size != 4104) {
		return false;
	}

	int pointPos = -1;
    for (int k=1; k<9 && pointPos == -1; k++) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1) return false;
    if (name[pointPos+1] != 's' && name[pointPos+1] != 'S') return false;
    if (name[pointPos+2] != 'y' && name[pointPos+2] != 'Y') return false;
    if (name[pointPos+3] != 'x' && name[pointPos+3] != 'X') return false;

    return true;
}


