/*
 * Copyright 2021 Xavier Hosxe
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


#ifndef MidiControllerFile_H_
#define MIDICONTROLLERFILE_H

#include "PreenFMFileType.h"
#include "MidiControllerState.h"

class MidiControllerFile: public PreenFMFileType {
public:
	MidiControllerFile();
	virtual ~MidiControllerFile();

	void loadConfig(MidiControllerState* midiControllerState);
	void saveConfig(MidiControllerState* midiControllerState);
protected:
	const char* getFolderName() { return 0; }
    bool isCorrectFile(char *name, int size) { return false; }
};

#endif /* MidiControllerFile_H_ */
