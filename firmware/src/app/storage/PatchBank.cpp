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

#include "PatchBank.h"
#include <string.h>

__attribute__((section(".ram_d2b"))) struct PFM3File preenFMBankAlloc[NUMBEROFPREENFMBANKS];

PatchBank::PatchBank() {
    numberOfFilesMax_ = NUMBEROFPREENFMBANKS;
    myFiles_ = preenFMBankAlloc;
}

PatchBank::~PatchBank() {
}

const char* PatchBank::getFolderName() {
    return PREENFM_DIR;
}

bool PatchBank::isCorrectFile(char *name, int size) {
    if (size < 100000) {
        return false;
    }
    int pointPos = -1;
    for (int k = 1; k < 9 && pointPos == -1; k++) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1)
        return false;
    if (name[pointPos + 1] != 'b' && name[pointPos + 1] != 'B')
        return false;
    if (name[pointPos + 2] != 'n' && name[pointPos + 2] != 'N')
        return false;
    if (name[pointPos + 3] != 'k' && name[pointPos + 3] != 'K')
        return false;

    return true;
}

void PatchBank::createPatchBank(const char *name) {
    UINT byteWritten;

    if (!isInitialized_) {
        initFiles();
    }

    const struct PFM3File *newBank = addEmptyFile(name);
    const char *fullBankName = getFullName(name);
    if (newBank == 0) {
        return;
    }

    FIL bankFile = createFile(fullBankName);
    if (bankFile.err > 0) {
        return;
    }

    for (uint32_t s = 0; s < ALIGNED_PATCH_SIZE; s++) {
        storageBuffer[s] = 0;
    }
    convertParamsToFlash(&preenMainPreset, (struct FlashSynthParams*) storageBuffer, *arpeggiatorPartOfThePreset_ > 0);
    *(uint32_t*) (&storageBuffer[ALIGNED_PATCH_SIZE - 5]) = PRESET_CURRENT_VERSION;

    for (int k = 0; k < 128; k++) {
        f_write(&bankFile, storageBuffer, ALIGNED_PATCH_SIZE, &byteWritten);
    }
    closeFile(bankFile);
}

bool PatchBank::loadPatch(const struct PFM3File *bank, int patchNumber, struct OneSynthParams *params,
                          SynthEngineType *engine, uint8_t *msfaProgram) {
    const char *fullBankName = getFullName(bank->name);

    int result = load(fullBankName, patchNumber * ALIGNED_PATCH_SIZE, (void*) storageBuffer, ALIGNED_PATCH_SIZE);

    if (result == ALIGNED_PATCH_SIZE) {
        uint32_t version = *(uint32_t*) (&storageBuffer[ALIGNED_PATCH_SIZE - 5]);

        if (version == PRESET_MSFA) {
            if (msfaProgram != nullptr) {
                memcpy(msfaProgram, &storageBuffer[0], MSFA_PROGRAM_SIZE);
            }
            if (params != nullptr) {
                for (int i = 0; i < 12; i++) {
                    params->presetName[i] = storageBuffer[160 + i];
                }
                params->presetName[12] = 0;
            }
            if (engine != nullptr) {
                *engine = SYNTH_ENGINE_MSFA;
            }
            return true;
        }

        switch (version) {
            case PRESET_VERSION2:
                // Direct copy
                for (uint32_t p = 0; p < PFM3_PATCH_FLASH_SIZE; p++) {
                    ((char*) params)[p] = storageBuffer[p];
                }
                break;
            default:
                // VERSION1 Needs a conversion
                convertFlashToParams((const struct FlashSynthParams*) storageBuffer, params, *arpeggiatorPartOfThePreset_ > 0);
                break;
        }
        if (engine != nullptr) {
            *engine = SYNTH_ENGINE_PFM3;
        }
        return true;
    }
    return false;
}

const char* PatchBank::loadPatchName(const struct PFM3File *bank, int patchNumber) {

    const char *fullBankName = getFullName(bank->name);
    uint32_t version;
    load(fullBankName, patchNumber * ALIGNED_PATCH_SIZE + ALIGNED_PATCH_SIZE - 5, (void*) &version, 4);

    if (version == PRESET_MSFA) {
        load(fullBankName, ALIGNED_PATCH_SIZE * patchNumber + 160, (void*) presetName_, 12);
        presetName_[12] = 0;
        if (presetName_[0] == 0) {
            presetName_[0] = 'M';
            presetName_[1] = 'S';
            presetName_[2] = 'F';
            presetName_[3] = 'A';
            presetName_[4] = 0;
        }
        return presetName_;
    }

    int namePosition;

    switch (version) {
        case PRESET_VERSION2: {
            OneSynthParams *version2Params = (OneSynthParams*) storageBuffer;
            namePosition = (int) (((unsigned int) version2Params->presetName) - (unsigned int) version2Params);
            break;
        }
        default: {
            // VERSION 1
            FlashSynthParams *flashSynthParams = (FlashSynthParams*) storageBuffer;
            namePosition = (int) (((unsigned int) flashSynthParams->presetName) - (unsigned int) flashSynthParams);
            break;
        }
    }

    load(fullBankName, ALIGNED_PATCH_SIZE * patchNumber + namePosition, (void*) presetName_, 12);
    presetName_[12] = 0;
    return presetName_;

}

void PatchBank::savePatch(const struct PFM3File *bank, int patchNumber, const struct OneSynthParams *params,
                          SynthEngineType engine, const uint8_t *msfaProgram) {
    const char *fullBankName = getFullName(bank->name);

    for (int p = 0; p < ALIGNED_PATCH_SIZE; p++) {
        storageBuffer[p] = 0;
    }

    if (engine == SYNTH_ENGINE_MSFA && msfaProgram != nullptr) {
        memcpy(&storageBuffer[0], msfaProgram, MSFA_PROGRAM_SIZE);
        for (int i = 0; i < 12; i++) {
            storageBuffer[160 + i] = params->presetName[i];
        }
        *(uint32_t*) (&storageBuffer[ALIGNED_PATCH_SIZE - 5]) = PRESET_MSFA;
    } else {
        convertParamsToFlash(params, (struct FlashSynthParams*) storageBuffer, *arpeggiatorPartOfThePreset_ > 0);
        *(uint32_t*) (&storageBuffer[ALIGNED_PATCH_SIZE - 5]) = PRESET_CURRENT_VERSION;
    }

    // Save patch
    save(fullBankName, patchNumber * ALIGNED_PATCH_SIZE, storageBuffer, ALIGNED_PATCH_SIZE);
}

void PatchBank::copyNewPreset(struct OneSynthParams *params) {
    char *source = (char*) &newPresetParams;
    char *dest = (char*) params;
    for (uint32_t k = 0; k < sizeof(struct OneSynthParams); k++) {
        dest[k] = source[k];
    }
}

