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


#ifndef DX7SYSEXFILE_H_
#define DX7SYSEXFILE_H_

#include "PreenFMFileType.h"

class DX7SysexFile: public PreenFMFileType {
public:
	DX7SysexFile();
	virtual ~DX7SysexFile();

	uint8_t* dx7LoadPatch(const struct PFM3File* bank, int patchNumber);

protected:
	const char* getFolderName();
	bool isCorrectFile(char *name, int size);
	struct PFM3File *dx7Bank;

};

#endif /* DX7SYSEXFILE_H_ */
