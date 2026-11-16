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
#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

template <typename T, int size> class RingBuffer {
public:
    RingBuffer() {
        clear();
    }

    ~RingBuffer() {};

    void clear() {
        this->head_ = 0;
        this->tail_ = 0;
    }

    void insert(T element) {
        this->buf_[this->tail_] = element;
        this->tail_ = (this->tail_ == size - 1) ? 0 : this->tail_ + 1;
    }

    T remove() {
        T element = this->buf_[this->head_];
        this->head_ = (this->head_ == size - 1) ? 0 : this->head_ + 1;
        return element;
    }

    bool isFull() {
        return (this->tail_ + 1 == this->head_) || (this->tail_ == (size - 1) && this->head_ == 0);
    }

    int getCount() {
        int count = this->tail_ - this->head_;
        if (this->tail_ < this->head_) {
            count += size;
        }
        return count;
    }

    void appendBlock(T* block, int number) {
        for (int k = 0; k < number; k++) {
            insert(block[k]);
        }
    }

private:
    volatile int head_;
    volatile int tail_;
    T buf_[size + 1];
};

#endif /* RINGBUFFER_H_ */
