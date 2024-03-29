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


#ifdef __cplusplus
extern "C" {
#endif


void preenfm3Loop();
void preenfm3Init();
void preenfm3MidiControllerInit();
void preenfm3MidiControllerLoop();
bool isButton1Pressed();

void preenfm3TftTic();
void preenfm3_USART();
void preenfm3_usbDataReceive(uint8_t *buffer);
void preenfm3StartSai();

float getCompInstrumentVolume(int t);
float getCompInstrumentGainReduction(int t);
void preenfm3SwitchToMidiController();
void preenfm3ExitMidiController();


#ifdef __cplusplus
}
#endif
