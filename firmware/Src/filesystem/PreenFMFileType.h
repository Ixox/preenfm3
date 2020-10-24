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


#ifndef PREENFMFILETYPE_H_
#define PREENFMFILETYPE_H_

#include "Common.h"
#include "FileSystemUtils.h"
#include "fatfs.h"

#define ARRAY_SIZE(x)  ( sizeof(x) / sizeof((x)[0]) )

enum FILE_ENUM {
    DEFAULT_MIXER = 0,
    PROPERTIES,
    FIRMWARE,
	DEFAULT_SEQUENCE
};

#define DEFAULT_MIXER_NAME       "0:/pfm3/mix.dfl"
#define DEFAULT_SEQUENCE_NAME    "0:/pfm3/seq.dfl"
#define PROPERTIES_NAME          "0:/pfm3/Settings.txt"
#define SCALA_CONFIG_NAME        "0:/pfm3/ScalaCfg.txt"

#define USERWAVEFORM_FILENAME_TXT            "0:/pfm3/waveform/usr#.txt"
#define USERWAVEFORM_FILENAME_BIN            "0:/pfm3/waveform/usr#.bin"

#define DX7_PACKED_PATCH_SIZED 128
#define DX7_UNPACKED_PATCH_SIZED 155

#define FIRMWARE_DIR             "0:/pfm3"
#define DX7_DIR                  "0:/pfm3/dx7"
#define PREENFM_DIR              "0:/pfm3"
#define SCALA_DIR                "0:/pfm3/scala"
#define USERWAVEFORM_DIR         "0:/pfm3/waveform"

#define SYSEX_NEW_PFM2_BYTE_PATCH 5

#ifndef BOOTLOADER
#define NUMBEROFDX7BANKS 256
#define NUMBEROFPREENFMBANKS 128
#define NUMBEROFPREENFMMIXERS 128
#define NUMBEROFPREENFMSEQUENCES 128

#define NUMBEROFSCALASCALEFILES 128
#define NUMBER_OF_MIXERS_PER_BANK 32
#define NUMBER_OF_SEQUENCES_PER_BANK 32
#endif

#ifdef BOOTLOADER
#define NUMBEROFDX7BANKS 1
#define NUMBEROFPREENFMBANKS 1
#define NUMBEROFPREENFMMIXERS 1
#define NUMBEROFSCALASCALEFILES 1
#endif

struct PFM3File {
    char name[13];
    FileType fileType;
    uint8_t version;
};

// Storage maping of the parameters
struct FlashSynthParams {
    struct Engine1Params engine1;
    struct FlashEngineIm1 flashEngineIm1;
    struct FlashEngineIm2 flashEngineIm2;
    struct EngineMix1 engineMix1;
    struct EngineMix2 engineMix2;
    struct EngineMix3 engineMix3;
    struct OscillatorParams osc1;
    struct OscillatorParams osc2;
    struct OscillatorParams osc3;
    struct OscillatorParams osc4;
    struct OscillatorParams osc5;
    struct OscillatorParams osc6;
    struct EnvelopeParamsA env1a;
    struct EnvelopeParamsB env1b;
    struct EnvelopeParamsA env2a;
    struct EnvelopeParamsB env2b;
    struct EnvelopeParamsA env3a;
    struct EnvelopeParamsB env3b;
    struct EnvelopeParamsA env4a;
    struct EnvelopeParamsB env4b;
    struct EnvelopeParamsA env5a;
    struct EnvelopeParamsB env5b;
    struct EnvelopeParamsA env6a;
    struct EnvelopeParamsB env6b;
    struct MatrixRowParams matrixRowState1;
    struct MatrixRowParams matrixRowState2;
    struct MatrixRowParams matrixRowState3;
    struct MatrixRowParams matrixRowState4;
    struct MatrixRowParams matrixRowState5;
    struct MatrixRowParams matrixRowState6;
    struct MatrixRowParams matrixRowState7;
    struct MatrixRowParams matrixRowState8;
    struct MatrixRowParams matrixRowState9;
    struct MatrixRowParams matrixRowState10;
    struct MatrixRowParams matrixRowState11;
    struct MatrixRowParams matrixRowState12;
    struct LfoParams lfoOsc1;
    struct LfoParams lfoOsc2;
    struct LfoParams lfoOsc3;
    struct EnvelopeParams lfoEnv1;
    struct Envelope2Params lfoEnv2;
    struct StepSequencerParams lfoSeq1;
    struct StepSequencerParams lfoSeq2;
    struct StepSequencerSteps lfoSteps1;
    struct StepSequencerSteps lfoSteps2;
    char presetName[13];
    struct EngineArp1 engineArp1;
    struct EngineArp2 engineArp2;
    struct FlashEngineVeloIm1 flashEngineVeloIm1;
    struct FlashEngineVeloIm2 flashEngineVeloIm2;
    struct EffectRowParams effect;
    struct EngineArpUserPatterns engineArpUserPatterns;
    struct LfoPhaseRowParams lfoPhases;
    struct MidiNoteCurveRowParams midiNote1Curve;
    struct MidiNoteCurveRowParams midiNote2Curve;
};

#define PFM_PATCH_SIZE sizeof(struct FlashSynthParams)

#define ALIGNED_PATCH_SIZE 1024
#define ALIGNED_PATCH_ZERO ALIGNED_PATCH_SIZE-PFM_PATCH_SIZE

#define MIXER_SIZE sizeof(struct MixerState)
#define ALIGNED_MIXER_SIZE 1024
#define FULL_MIXER_SIZE (ALIGNED_MIXER_SIZE + ALIGNED_PATCH_SIZE * NUMBER_OF_TIMBRES)

#define PROPERTY_FILE_SIZE 8192

extern char storageBuffer[PROPERTY_FILE_SIZE];
extern struct MixerState tmpMixerState;

class Storage;

class PreenFMFileType {
    friend Storage;
public:
    PreenFMFileType();
    virtual ~PreenFMFileType();

    bool exists(const char* name);
    void setFileSystemUtils(FileSystemUtils* fsu) {
        this->fsu = fsu;
    }

    const struct PFM3File* getFile(int fileNumber);
    int getFileIndex(const char* name);
    int getFileIndex(const struct PFM3File* file);
    int renameFile(const struct PFM3File* bank, const char* newName);
    bool nameExists(const char* bankName);
    const struct PFM3File* addEmptyFile(const char* fileName);

protected:
    virtual const char* getFolderName() = 0;
    virtual bool isCorrectFile(char *name, int size) = 0;

    int numberOfFiles;
    int numberOfFilesMax;

    int remove(FILE_ENUM file);
    const char* getFileName(FILE_ENUM file);
    const char* getFullName(const char* fileName);
    int load(FILE_ENUM file, int seek, void* bytes, int size);
    int load(const char* fileName, int seek, void* bytes, int size);
    int save(FILE_ENUM file, int seek, void* bytes, int size);
    int save(const char* fileName, int seek, void* bytes, int size);
    int checkSize(FILE_ENUM file);
    int checkSize(const char* fileName);

    FIL createFile(const char* fileName);
    bool closeFile(FIL &file);
    int saveData(FIL &file, void* bytes, uint32_t size);

    int initFiles();
    int readNextFile(struct PFM3File* bank);

    void swapFiles(struct PFM3File* bankFiles, int i, int j);
    void sortFiles(struct PFM3File* bankFiles, int numberOfFiles);

    void convertParamsToFlash(const struct OneSynthParams* params, struct FlashSynthParams* memory, bool saveArp);
    void convertFlashToParams(const struct FlashSynthParams* memory, struct OneSynthParams* params, bool loadArp);

    int bankBaseLength(const char* bankName);

    struct PFM3File *myFiles;
    struct PFM3File errorFile;
    char fullName[40];
    FileSystemUtils* fsu;
    bool isInitialized;

    virtual bool isReadOnly(struct PFM3File *file) {
    	return false;
    }
};

#endif /* PREENFMFILETYPE_H_ */
