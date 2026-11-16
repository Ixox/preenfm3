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

enum PRESET_VERSION {
    PRESET_VERSION1 = 0,
    PRESET_VERSION2 = 292928062, // random 32 bits number for the first preenfm3
    PRESET_MSFA = 981344537    // random 32 bits number for MSFA presets
};

// Let's stick to VERSION1 : VERSION2 seems dangerous :)
#define PRESET_CURRENT_VERSION PRESET_VERSION1

class PatchBank: public PreenFMFileType {
public:
    PatchBank();
    virtual ~PatchBank();

    void createPatchBank(const char *name);
    void savePatch(const struct PFM3File *bank, int patchNumber, const struct OneSynthParams *params,
                   SynthEngineType engine = SYNTH_ENGINE_PFM3, const uint8_t *msfaProgram = nullptr);
    void setArpeggiatorPartOfThePreset(uint8_t *pointer) {
        arpeggiatorPartOfThePreset_ = pointer;
    }
    bool loadPatch(const struct PFM3File *bank, int patchNumber, struct OneSynthParams *params,
                   SynthEngineType *engine = nullptr, uint8_t *msfaProgram = nullptr);
    const char* loadPatchName(const struct PFM3File *bank, int patchNumber);

    bool decodeBufferAndApplyPreset(uint8_t *buffer, struct OneSynthParams *params);
    void copyNewPreset(struct OneSynthParams *params);

protected:
    const char* getFolderName();
    bool isCorrectFile(char *name, int size);

private:
    uint8_t *arpeggiatorPartOfThePreset_;
    char presetName_[13];
};

#endif /* PATCHBANK_H_ */
