/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Author: Andrei Vasiliu <andrei.vasiliu@intel.com>
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

package tinyb.dbus;

import org.direct_bt.BTFactory;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;

public class DBusObject implements BTObject
{
    protected long nativeInstance;
    private boolean isValid;

    static {
        BTFactory.checkInitialized();
    }

    static BTType class_type() { return BTType.NONE; }

    @Override
    public native BTType getBluetoothType();

    @Override
    public native BTObject clone();

    private native void delete();
    private native boolean operatorEqual(DBusObject obj);

    protected DBusObject(final long instance)
    {
        nativeInstance = instance;
        isValid = true;
    }

    @Override
    protected void finalize()
    {
        close();
    }

    @Override
    public boolean equals(final Object obj)
    {
        if (obj == null || !(obj instanceof DBusObject))
            return false;
        return operatorEqual((DBusObject)obj);
    }

    protected native String getObjectPath();

    @Override
    public int hashCode() {
        final String objectPath = getObjectPath();
        return objectPath.hashCode();
    }

    protected synchronized boolean isValid() { return isValid; }

    @Override
    public synchronized void close() {
        if (!isValid)
            return;
        isValid = false;
        delete();
    }
}
