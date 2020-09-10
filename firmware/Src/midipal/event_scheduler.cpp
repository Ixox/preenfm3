// Copyright 2011 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Event scheduler.

#include "event_scheduler.h"

void EventScheduler::Init() {
    //   memset(entries_, kFreeSlot, kEventSchedulerSize * sizeof(SchedulerEntry));
    for (int k = 0; k < kEventSchedulerSize; k++) {
        entries_[k].note = kFreeSlot;
        entries_[k].velocity = kFreeSlot;
        entries_[k].when = kFreeSlot;
        entries_[k].next = kFreeSlot;
        entries_[k].tag = kFreeSlot;
    }

    root_ptr_ = 0;
    size_ = 0;
}

void EventScheduler::Tick() {
    while (!entries_[root_ptr_].when && root_ptr_) {
        entries_[root_ptr_].note = kFreeSlot;
        root_ptr_ = entries_[root_ptr_].next;
        --size_;
    }

    uint8_t current = root_ptr_;
    while (current) {
        --entries_[current].when;
        current = entries_[current].next;
    }
}

uint8_t EventScheduler::Remove(uint8_t note, uint8_t velocity) {
    uint8_t current = root_ptr_;
    uint8_t found = 0;
    while (current) {
        if (entries_[current].note == note && entries_[current].velocity == velocity) {
            entries_[current].note = kZombieSlot;
            ++found;
        }
        current = entries_[current].next;
    }
    return found;
}

void EventScheduler::Schedule(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag) {
    // Locate a free entry in the list.
    uint8_t free_slot = 0;
    for (uint8_t i = 1; i < kEventSchedulerSize; ++i) {
        if (entries_[i].note == kFreeSlot) {
            free_slot = i;
            break;
        }
    }

    if (!free_slot) {
        return;  // Queue is full!
    }
    ++size_;
    entries_[free_slot].note = note;
    entries_[free_slot].velocity = velocity;
    entries_[free_slot].when = when;
    entries_[free_slot].tag = tag;

    if (!root_ptr_ || when < entries_[root_ptr_].when) {
        entries_[free_slot].next = root_ptr_;
        root_ptr_ = free_slot;
    } else {
        uint8_t insert_position = root_ptr_;
        while (entries_[insert_position].next && when >= entries_[entries_[insert_position].next].when) {
            insert_position = entries_[insert_position].next;
        }
        entries_[free_slot].next = entries_[insert_position].next;
        entries_[insert_position].next = free_slot;
    }
}

