/*
 * Copyright 2019 Xavier Hosxe
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

#ifndef PATCHBANK_H_
#define PATCHBANK_H_

#include "PreenFMFileType.h"
#include "Common.h"
#include "PersistenceMagic.h"
#include "Pfm3Preset.h"

class Timbre;

class PatchBank: public PreenFMFileType {
public:
    PatchBank();
    virtual ~PatchBank();

    void createPatchBank(const char *name);
    void savePatch(const struct PFM3File *bank, int patchNumber, Timbre *timbre);
    void setArpeggiatorPartOfThePreset(uint8_t *pointer) {
        arpeggiatorPartOfThePreset_ = pointer;
    }
    void setTimbre(Timbre *timbre) {
        timbre_ = timbre;
    }
    bool loadPatch(const struct PFM3File *bank, int patchNumber, Timbre *timbre);
    const char* loadPatchName(const struct PFM3File *bank, int patchNumber);

    bool decodeBufferAndApplyPreset(uint8_t *buffer, struct Pfm3Preset *params);
    void copyNewPreset(struct Pfm3Preset *params);

protected:
    const char* getFolderName();
    bool isCorrectFile(char *name, int size);

private:
    uint8_t *arpeggiatorPartOfThePreset_;
    Timbre *timbre_;
    char presetName_[13];
};

#endif /* PATCHBANK_H_ */
