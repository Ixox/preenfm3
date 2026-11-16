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




class Timbre;

class Storage {
public:
    virtual ~Storage() {}
    void init(Timbre *timbre1, Timbre *timbre2, Timbre *timbre3,
        Timbre *timbre4, Timbre *timbre5, Timbre *timbre6);

#ifdef DEBUG
    void testMemoryPreset();
#endif

#ifndef BOOTLOADER
    MixerBank* getMixerBank() { return &mixerBank_; }
    ConfigurationFile* getConfigurationFile() { return &configurationFile_; }
    DX7SysexFile* getDX7SysexFile() { return &dx7SysexFile_; }
    PatchBank* getPatchBank() { return &patchBank_; }
    ScalaFile* getScalaFile() { return &scalaFile_; }
    UserWaveform* getUserWaveform() { return &userWaveForm_; };
    UserEnvCurve* getUserEnvCurve() { return &userEnvCurve_; };
#ifdef PPMIMAGE_ENABLE
    PPMImage* getPPMImage() { return &ppmImage_; };
#endif
    SequenceBank* getSequenceBank() { return &sequenceBank_; };
#else
#endif

private:
    FileSystemUtils fsu_;
#ifndef BOOTLOADER
    MixerBank mixerBank_;
    ConfigurationFile configurationFile_;
    DX7SysexFile dx7SysexFile_;
    PatchBank patchBank_;
    ScalaFile scalaFile_;
    UserWaveform userWaveForm_;
    UserEnvCurve userEnvCurve_;
#ifdef PPMIMAGE_ENABLE
    PPMImage ppmImage_;
#endif
    SequenceBank sequenceBank_;
#else
#endif
};

#endif /*__STORAGE_H__*/
