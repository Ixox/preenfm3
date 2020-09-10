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


#ifndef SYNTHMENULISTENER_H_
#define SYNTHMENULISTENER_H_

#include "Menu.h"


class SynthMenuListener {
public:
    virtual void newSynthMode(FullState* fullState) = 0;
    virtual void menuBack(const MenuItem* oldMenuItem, FullState* fullState) = 0;
    virtual void newMenuState(FullState* fullState) = 0;
    virtual void newMenuSelect(FullState* fullState) = 0;
    virtual void newPfm3Page(FullState* fullState) = 0;


    SynthMenuListener* nextListener;
};

#endif /* SYNTHMENULISTENER_H_ */
