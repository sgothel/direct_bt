/*
 * Author: Petre Eftime <petre.p.eftime@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "orgbluez-dbus.h"
#include "BluetoothEvent.hpp"
#include "BluetoothManager.hpp"

void BluetoothEvent::generic_callback(BluetoothObject &object, void *data)
{

   if (data == nullptr)
        return;

   BluetoothConditionVariable *generic_data = static_cast<BluetoothConditionVariable *>(data);

   generic_data->result = object.clone();
   generic_data->notify();
}

BluetoothEvent::BluetoothEvent(BluetoothType type_, std::string *name_,
    std::string *identifier_, BluetoothObject *parent_, bool execute_once_,
    BluetoothCallback cb_, void *data_)
{
    canceled = false;
    this->type = type_;
    if (name_ != nullptr)
    	this->name = new std::string(*name_);
    else
        this->name = nullptr;

    if (identifier_ != nullptr)
        this->identifier = new std::string(*identifier_);
    else
        this->identifier = nullptr;

    if (parent_ != nullptr)
        this->parent = parent_->clone();
    else
        this->parent = nullptr;

    this->execute_once = execute_once_;

    if (cb_ == nullptr) {
        this->data = static_cast<void *>(&cv);
        this->cb = generic_callback;
    }
    else {
        this->cb = cb_;
        this->data = data_;
    }
}

bool BluetoothEvent::execute_callback(BluetoothObject &object)
{
    if (has_callback()) {
        cb(object, data);
        cv.notify();
        return execute_once;
    }

    return true;
}

void BluetoothEvent::wait(std::chrono::milliseconds timeout)
{
    if (!canceled && execute_once == true) {
        if (timeout == std::chrono::milliseconds::zero())
            cv.wait();
        else
            cv.wait_for(timeout);
    }
}

void BluetoothEvent::cancel()
{
    BluetoothManager *manager = BluetoothManager::get_bluetooth_manager();
    manager->remove_event(*this);

    cv.notify();
}

BluetoothEvent::~BluetoothEvent()
{
    if (name != nullptr)
        delete name;
    if (identifier != nullptr)
        delete identifier;
    if (parent != nullptr)
        delete parent;
}
