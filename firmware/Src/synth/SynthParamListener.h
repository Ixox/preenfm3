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


#ifndef SYNTHPARAMLISTENER_H_
#define SYNTHPARAMLISTENER_H_

#include "stdint.h"
#include "FMDisplay.h"

class SynthParamListener {

public:
    virtual void newParamValue(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue, float newValue) = 0;
    virtual void newParamValueFromExternal(int timbre, int currentrow, int encoder, ParameterDisplay* param, float oldValue, float newValue) = 0;
    virtual void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue);
    virtual void newTimbre(int timbre) = 0;
    virtual void newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) = 0;
    virtual void newMixerEdit(int oldButton, int newButton) = 0;

    virtual void beforeNewParamsLoad(int timbre) = 0;
    virtual void afterNewParamsLoad(int timbre) = 0;
    virtual void afterNewMixerLoad() = 0;

    virtual void playNote(int timbre, char note, char velocity) = 0;
    virtual void stopNote(int timbre, char note) = 0;

    virtual void newPresetName(int timbre) {};

    SynthParamListener* nextListener;
};

#endif /* SYNTHPARAMLISTENER_H_ */
