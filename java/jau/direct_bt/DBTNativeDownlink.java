/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
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

package jau.direct_bt;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Java counterpart to C++ jau::JavaUplink
 *
 * Store the native `std::shared_ptr<T> *` long representation
 * and handles the lifecycles of
 * - Java object reference in the native object
 *   - `jau::JavaUplink`'s `std::shared_ptr<JavaAnon>` via `JavaUplink::setJavaObject()`
 * - Native object reference in the Java object
 *   - `std::shared_ptr<T> *` long representation in `long nativeInstance`
 *
 * Using a `shared_ptr<T>` ensures surviving of the native object when temporary copied
 * within JNI methods w/o locking.
 */
public abstract class DBTNativeDownlink
{
    /** native `std::shared_ptr<T> *` long representation. */
    private long nativeInstance;
    private final AtomicBoolean isNativeValid = new AtomicBoolean(false);
    private final Object nativeLock = new Object();

    /**
     *
     * @param nativeInstance
     */
    protected DBTNativeDownlink(final long nativeInstance)
    {
        this.nativeInstance = nativeInstance;
        isNativeValid.set(true);
        initNativeJavaObject(nativeInstance);
    }
    protected DBTNativeDownlink()
    {
        this.nativeInstance = 0;
        isNativeValid.set(false);
    }
    protected final void initDownlink(final long nativeInstance) {
        synchronized (nativeLock) {
            if( isNativeValid.compareAndSet(false, true) ) {
                this.nativeInstance = nativeInstance;
                initNativeJavaObject(nativeInstance);
            } else {
                System.err.println("DBTNativeDownlink.init2: Already valid, dropping nativeInstance 0x"+Long.toHexString(nativeInstance));
            }
        }
    }

    /**
     * Returns true if native instance is valid, otherwise false.
     */
    protected final boolean isNativeValid() { return isNativeValid.get(); }

    @Override
    protected void finalize()
    {
        delete();
    }

    /**
     * Deletes the {@code nativeInstance} in the following order
     * <ol>
     *   <li>Removes this java reference from the {@code nativeInstance}, i.e. `std::shared_ptr<JavaAnon>` via `JavaUplink::setJavaObject()`</li>
     *   <li>Deletes the {@code nativeInstance} via {@link #deleteImpl(long)}</li>
     *   <li>Zeros the {@code nativeInstance} reference</li>
     * </ol>
     */
    public final void delete() {
        synchronized (nativeLock) {
            if( !isNativeValid.compareAndSet(true, false) ) {
                if( DBTManager.DEBUG ) {
                    System.err.println("JAVA: delete: !valid -> bail: "+getClass().getSimpleName());
                }
                return;
            }
            if( DBTManager.DEBUG ) {
                System.err.println("JAVA: delete.0: "+getClass().getSimpleName()+": valid, handle 0x"+Long.toHexString(nativeInstance));
            }
            final long _nativeInstance = nativeInstance;
            nativeInstance = 0;
            deleteNativeJavaObject(_nativeInstance); // will issue notifyDeleted() itself!
            deleteImpl(_nativeInstance);
            if( DBTManager.DEBUG ) {
                System.err.println("JAVA: delete.X: "+getClass().getSimpleName()+": handle 0x"+Long.toHexString(nativeInstance));
            }
        }
    }

    /**
     * Called from native JavaUplink dtor -> JavaGlobalObj dtor,
     * i.e. native instance destructed in native land.
     */
    private final void notifyDeleted() {
        synchronized (nativeLock) {
            final boolean _isValid = isNativeValid.get();
            final long _nativeInstance = nativeInstance;
            isNativeValid.set(false);
            nativeInstance = 0;
            if( DBTManager.DEBUG ) {
                System.err.println("JAVA: delete.notifyDeleted: "+getClass().getSimpleName()+", was: valid "+_isValid+", handle 0x"+Long.toHexString(_nativeInstance)+": "+toString());
            }
        }
    }

    /**
     * Deletes the native instance.
     * <p>
     * Called via {@link #delete()} and at this point
     * <ul>
     *  <li>this java reference has been removed from the native instance, i.e. {@code JavaUplink}'s {@code javaObjectRef = nullptr}</li>
     *  <li>the {@link #nativeInstance} reference has been zeroed, but passed as argument for this final native deletion task.</li>
     * </ul>
     * </p>
     * @param nativeInstance copy of {@link #nativeInstance} reference, which has been already zeroed.
     */
    protected abstract void deleteImpl(long nativeInstance);

    private native void initNativeJavaObject(final long nativeInstance);
    private native void deleteNativeJavaObject(final long nativeInstance);
}
