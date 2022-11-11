/*
 * Copyright 2013 Xavier Hosxe
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



#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <MixerBank.h>
#include "Common.h"
#include "FileSystemUtils.h"
#include "ConfigurationFile.h"
#include "DX7SysexFile.h"
#include "PatchBank.h"
#include "ScalaFile.h"
#include "UserWaveform.h"
#include "UserEnvCurve.h"
#include "PPMImage.h"
#include "SequenceBank.h"




class Storage {
public:
    virtual ~Storage() {}
    void init(struct OneSynthParams*timbre1, struct OneSynthParams*timbre2, struct OneSynthParams*timbre3,
        struct OneSynthParams*timbre4, struct OneSynthParams*timbre5, struct OneSynthParams*timbre6);

#ifdef DEBUG
    void testMemoryPreset();
#endif

#ifndef BOOTLOADER
    MixerBank* getMixerBank() { return &mixerBank; }
    ConfigurationFile* getConfigurationFile() { return &configurationFile; }
    DX7SysexFile* getDX7SysexFile() { return &dx7SysexFile; }
    PatchBank* getPatchBank() { return &patchBank; }
    ScalaFile* getScalaFile() { return &scalaFile; }
    UserWaveform* getUserWaveform() { return &userWaveForm; };
    UserEnvCurve* getUserEnvCurve() { return &userEnvCurve; };
#ifdef PPMIMAGE_ENABLE
    PPMImage* getPPMImage() { return &ppmImage; };
#endif
    SequenceBank* getSequenceBank() { return &sequenceBank; };
#else
#endif

private:
    FileSystemUtils fsu;
#ifndef BOOTLOADER
    MixerBank mixerBank;
    ConfigurationFile configurationFile;
    DX7SysexFile dx7SysexFile;
    PatchBank patchBank;
    ScalaFile scalaFile;
    UserWaveform userWaveForm;
    UserEnvCurve userEnvCurve;
#ifdef PPMIMAGE_ENABLE
    PPMImage ppmImage;
#endif
    SequenceBank sequenceBank;
#else
#endif
};

#endif /*__STORAGE_H__*/
