// Copyright 2011 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// Event list. I apologize for not using a smarter data structure - the
// insertion of an item in the queue is O(n) while it should be O(log n).

#ifndef MIDIPAL_EVENT_SCHEDULER_H_
#define MIDIPAL_EVENT_SCHEDULER_H_

#include <stdint.h>

static const uint8_t kEventSchedulerSize = 90;

static const uint8_t kFreeSlot = 0xff;
static const uint8_t kZombieSlot = 0xfe;

struct SchedulerEntry {
    uint8_t note;  // 0xff for free slot
    uint8_t velocity;  // 0 for note off
    uint8_t when;
    uint8_t next;
    uint8_t tag;
};

class EventScheduler {
public:
    EventScheduler() {
    }

    void Init();
    void Tick();
    void Schedule(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag);
    void Schedule(uint8_t note, uint8_t velocity, uint8_t when) {
        Schedule(note, velocity, when, 0);
    }
    uint8_t Remove(uint8_t note, uint8_t velocity);

    inline const SchedulerEntry& entry(uint8_t address) {
        return entries_[address];
    }
    inline const uint8_t root() {
        return root_ptr_;
    }
    inline const uint8_t size() {
        return size_;
    }
    inline const uint8_t overflow() {
        return size() >= kEventSchedulerSize - 8;
    }

private:
    SchedulerEntry entries_[kEventSchedulerSize];
    uint8_t root_ptr_;
    uint8_t size_;

};

#endif // MIDIPAL_EVENT_SCHEDULER_H_
