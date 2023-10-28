/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

package org.direct_bt;

import org.jau.io.PrintUtil;
import org.jau.util.BasicTypes;

/**
 * Class maps a GATT command and optionally its asynchronous response
 * to a synchronous atomic operation.
 *
 * The GATT command is issued by writing the associated GATT characteristic value
 * via BTGattChar.writeValueNoResp() or BTGattChar.writeValue().
 *
 * Its optional asynchronous characteristic value notification or indication response
 * is awaited and collected after command issuance.
 *
 * If a response jau.uuid_t is given, notification or indication will be enabled at first send() command
 * and disabled at close() or destruction.
 *
 * @see BTGattChar.writeValueNoResp()
 * @see BTGattChar.writeValue()
 * @since 2.4.0
 */
public class BTGattCmd implements AutoCloseable
{
    public static interface DataCallback {
        public void run(final BTGattChar charDecl, byte[] char_value, long timestampMS);
    }

    private static final boolean DEBUG = BTFactory.DEBUG;

    /** Name, representing the command */
    private final String name;
    /** Command's BTGattService jau.uuid_t, may be null. */
    private final String service_uuid;
    /** Command's BTGattChar value jau.uuid_t to write command, never null. */
    private final String cmd_uuid;
    /** Command's optional BTGattChar value jau.uuid_t for the notification or indication response, may be null. */
    private final String rsp_uuid;

    private final Object mtxRspReceived = new Object();
    private final BTDevice dev;
    private volatile byte[] rsp_data;
    private BTGattChar cmdCharRef;
    private BTGattChar rspCharRef;
    private int rspMinSize;
    private DataCallback dataCallback;
    private boolean setup_done;

    private static class ResponseCharListener extends BTGattCharListener {
        private final BTGattCmd source;

        public ResponseCharListener(final BTGattCmd source_) {
            super();
            source = source_;
        }

        private void store(final byte[] value) {
            if( null == source.rsp_data ) {
                source.rsp_data = value;
            } else {
                final int new_size = source.rsp_data.length + value.length;
                final byte[] new_buf = new byte[new_size];
                System.arraycopy(source.rsp_data, 0, new_buf, 0,                      source.rsp_data.length);
                System.arraycopy(value,           0, new_buf, source.rsp_data.length, value.length);
                source.rsp_data = new_buf;
            }
        }

        @Override
        public void notificationReceived(final BTGattChar charDecl,
                                         final byte[] value, final long timestamp) {
            synchronized( source.mtxRspReceived ) {
                if( DEBUG ) {
                    PrintUtil.fprintf_td(System.err, "BTGattCmd.notificationReceived: Resp %s, value[%s]\n",
                            charDecl.toString(), BasicTypes.bytesHexString(value, 0, value.length, true /* lsbFirst */));
                }
                store(value);
                if( null != source.dataCallback ) {
                    source.dataCallback.run(charDecl, value, timestamp);
                }
                source.mtxRspReceived.notifyAll();
            }
        }

        @Override
        public void indicationReceived(final BTGattChar charDecl,
                                       final byte[] value, final long timestamp,
                                       final boolean confirmationSent) {
            synchronized( source.mtxRspReceived ) {
                if( DEBUG ) {
                    PrintUtil.fprintf_td(System.err, "BTGattCmd.indicationReceived: Resp %s, value[%s]\n",
                            charDecl.toString(), BasicTypes.bytesHexString(value, 0, value.length, true /* lsbFirst */));
                }
                store(value);
                if( null != source.dataCallback ) {
                    source.dataCallback.run(charDecl, value, timestamp);
                }
                source.mtxRspReceived.notifyAll();
            }
        }
    }
    private final ResponseCharListener rspCharListener;
    private boolean verbose;

    private boolean isConnected() { return dev.getConnected(); }

    private boolean isResolvedEq() {
        return null != cmdCharRef;
    }

    private String rspCharStr() { return null != rspCharRef ? rspCharRef.toString() : "n/a"; }

    private HCIStatusCode setup() {
        if( setup_done ) {
            return isResolvedEq() ? HCIStatusCode.SUCCESS : HCIStatusCode.NOT_SUPPORTED;
        }
        setup_done = true;
        cmdCharRef = null != service_uuid ? dev.findGattChar(service_uuid, cmd_uuid)
                                          : dev.findGattChar(cmd_uuid);
        if( null == cmdCharRef ) {
            if( verbose ) {
                PrintUtil.fprintf_td(System.err, "Command not found: service %s, char %s\n", service_uuid, cmd_uuid);
            }
            return HCIStatusCode.NOT_SUPPORTED;
        }

        if( !cmdCharRef.getProperties().isSet(GattCharPropertySet.Type.WriteNoAck) &&
            !cmdCharRef.getProperties().isSet(GattCharPropertySet.Type.WriteWithAck) ) {
            if( verbose ) {
                PrintUtil.fprintf_td(System.err, "Command has no write property: %s\n", cmdCharRef.toString());
            }
            cmdCharRef = null;
            return HCIStatusCode.NOT_SUPPORTED;
        }

        if( null != rsp_uuid ) {
            rspCharRef = null != service_uuid ? dev.findGattChar(service_uuid, rsp_uuid)
                                              : dev.findGattChar(rsp_uuid);
            if( null == rspCharRef ) {
                if( verbose ) {
                    PrintUtil.fprintf_td(System.err, "Response not found: service %s, char %s\n", service_uuid, rsp_uuid);
                }
                cmdCharRef = null;
                return HCIStatusCode.NOT_SUPPORTED;
            }
            try {
                final boolean cccdEnableResult[] = { false, false };
                if( rspCharRef.addCharListener( rspCharListener, cccdEnableResult ) ) {
                    return HCIStatusCode.SUCCESS;
                } else {
                    if( verbose ) {
                        PrintUtil.fprintf_td(System.err, "CCCD Notify/Indicate not supported on response %s\n", rspCharRef.toString());
                    }
                    cmdCharRef = null;
                    rspCharRef = null;
                    return HCIStatusCode.NOT_SUPPORTED;
                }
            } catch ( final Exception e ) {
                PrintUtil.fprintf_td(System.err, "Exception caught for %s: %s\n", e.toString(), toString());
                cmdCharRef = null;
                rspCharRef = null;
                return HCIStatusCode.TIMEOUT;
            }
        } else {
            return HCIStatusCode.SUCCESS;
        }
    }

    /**
     * Close this command instance, usually called at destruction.
     * <p>
     * If a response jau.uuid_t has been given, notification or indication will be disabled.
     * </p>
     * {@inheritDoc}
     */
    @Override
    public void close() {
        close0();
    }

    /**
     * Close this command instance, usually called at destruction.
     *
     * If a response jau.uuid_t has been given, notification or indication will be disabled.
     */
    public synchronized HCIStatusCode close0() {
        final boolean wasResolved = isResolvedEq();
        final BTGattChar rspCharRefCopy = rspCharRef;
        cmdCharRef = null;
        rspCharRef = null;
        if( !setup_done ) {
            return HCIStatusCode.SUCCESS;
        }
        setup_done = false;
        if( !wasResolved ) {
            return HCIStatusCode.SUCCESS;
        }
        if( !isConnected() ) {
            return HCIStatusCode.DISCONNECTED;
        }
        if( null != rspCharRefCopy) {
            try {
                final boolean res1 = rspCharRefCopy.removeCharListener(rspCharListener);
                final boolean res2 = rspCharRefCopy.disableIndicationNotification();
                if( res1 && res2 ) {
                    return HCIStatusCode.SUCCESS;
                } else {
                    return HCIStatusCode.FAILED;
                }
            } catch (final Exception e ) {
                PrintUtil.fprintf_td(System.err, "Exception caught for %s: %s\n", e.toString(), toString());
                return HCIStatusCode.TIMEOUT;
            }
        } else {
            return HCIStatusCode.SUCCESS;
        }
    }

    /**
     * Constructor for commands with notification or indication response.
     *
     * @param dev_ the remote BTDevice
     * @param name_ user given name, representing the command
     * @param service_uuid_ command's BTGattService jau.uuid_t, may be null for using a less efficient BTGattChar lookup
     * @param cmd_uuid_ command's BTGattChar value jau.uuid_t to write the command
     * @param rsp_uuid_ command's BTGattChar value jau.uuid_t for the notification or indication response.
     */
    public BTGattCmd(final BTDevice dev_, final String name_,
                     final String service_uuid_, final String cmd_uuid_, final String rsp_uuid_ )
    {
        name = name_;
        service_uuid = service_uuid_;
        cmd_uuid = cmd_uuid_;
        rsp_uuid = rsp_uuid_;
        dev = dev_;
        rsp_data = null;
        cmdCharRef = null;
        rspCharRef = null;
        rspMinSize = 0;
        dataCallback = null;
        setup_done = false;
        rspCharListener = new ResponseCharListener(this);
        verbose = DEBUG;
    }

    /**
     * Constructor for commands without response.
     *
     * @param dev_ the remote BTDevice
     * @param name_ user given name, representing the command
     * @param service_uuid_ command's BTGattService jau.uuid_t, may be null for using a less efficient BTGattChar lookup
     * @param cmd_uuid_ command's BTGattChar value jau.uuid_t to write the command
     */
    public BTGattCmd(final BTDevice dev_, final String name_, final String service_uuid_, final String cmd_uuid_)
    {
        name = name_;
        service_uuid = service_uuid_;
        cmd_uuid = cmd_uuid_;
        rsp_uuid = null;
        dev = dev_;
        rsp_data = null;
        cmdCharRef = null;
        rspCharRef = null;
        rspMinSize = 0;
        dataCallback = null;
        setup_done = false;
        rspCharListener = null;
        verbose = DEBUG;
    }

    @Override
    public void finalize() { close(); }

    public void setResponseMinSize(final int v) { rspMinSize = v; }
    public void setDataCallback(final DataCallback dcb) { dataCallback = dcb; }

    /** Return name, representing the command */
    public String getName() { return name; }

    /** Return command's BTGattService jau::uuid_t, may be null. */
    public String getServiceUUID() { return service_uuid; }

    /** Return command's BTGattChar value jau::uuid_t to write command, never null. */
    public String getCommandUUID() { return cmd_uuid; }

    /** Return true if a notification or indication response has been set via constructor, otherwise false. */
    public boolean hasResponseSet() { return null != rsp_uuid; }

    /** Return command's optional BTGattChar value jau::uuid_t for the notification or indication response, may be null. */
    public String getResponseUUID() { return rsp_uuid; }

    /** Set verbosity for UUID resolution. */
    public void setVerbose(final boolean v) { verbose = v; }

    /**
     * Returns the read-only response data object
     * for configured commands with response notification or indication.
     *
     * jau.TROOctets.size() matches the size of last received command response or zero.
     * @see #send(boolean, byte[], int)
     */
    public byte[] getResponse() { return rsp_data; }

    private String rspDataToString() {
        return null == rsp_data ? "null" : BasicTypes.bytesHexString(rsp_data, 0, rsp_data.length, true /* lsbFirst */);
    }

    /**
     * Query whether all UUIDs of this commands have been resolved.
     *
     * In case no command has been issued via send() yet,
     * the UUIDs will be resolved with this call.
     *
     * @return true if all UUIDs have been resolved, otherwise false
     */
    public synchronized boolean isResolved() {
        if( !setup_done ) {
            return HCIStatusCode.SUCCESS == setup();
        } else {
            return isResolvedEq();
        }
    }

    /**
     * Send the command to the remote BTDevice.
     *
     * If a notification or indication result jau.uuid_t has been set via constructor,
     * it will be awaited and can be retrieved via {@link #getResponse()} after command returns.
     *
     * @param prefNoAck pass true to prefer command write without acknowledge, otherwise use with-ack if available
     * @param cmd_data raw command octets
     * @param timeoutMS timeout in milliseconds. Defaults to 10 seconds limited blocking for the response to become available, if any.
     * @return
     * @see #getResponse()
     */
    public synchronized HCIStatusCode send(final boolean prefNoAck, final byte[] cmd_data, final int timeoutMS) {
        return sendImpl(prefNoAck, cmd_data, timeoutMS, true);
    }

    /**
     * Send the command to the remote BTDevice, only.
     *
     * Regardless whether a notification or indication result jau::uuid_t has been set via constructor,
     * this command will not wait for the response.
     *
     * @param prefNoAck pass true to prefer command write without acknowledge, otherwise use with-ack if available
     * @param cmd_data raw command octets
     * @return
     * @see #getResponse()
     */
    public synchronized HCIStatusCode sendOnly(final boolean prefNoAck, final byte[] cmd_data) {
        return sendImpl(prefNoAck, cmd_data, 0, false);
    }

    private synchronized HCIStatusCode sendImpl(final boolean prefNoAck, final byte[] cmd_data, final int timeoutMS, final boolean allowResponse) {
        HCIStatusCode res = HCIStatusCode.SUCCESS;

        if( !isConnected() ) {
            return HCIStatusCode.DISCONNECTED;
        }
        synchronized( mtxRspReceived ) {
            res = setup();
            if( HCIStatusCode.SUCCESS != res ) {
                return res;
            }
            rsp_data = null;

            if( DEBUG ) {
                PrintUtil.fprintf_td(System.err, "BTGattCmd.sendBlocking: Start: Cmd %s, args[%s], Resp %s, result[%s]",
                        cmdCharRef.toString(), cmd_data.toString(),
                        rspCharStr(), rspDataToString());
            }

            final boolean hasWriteNoAck = cmdCharRef.getProperties().isSet(GattCharPropertySet.Type.WriteNoAck);
            final boolean hasWriteWithAck = cmdCharRef.getProperties().isSet(GattCharPropertySet.Type.WriteWithAck);
            // Prefer WriteNoAck, if hasWriteNoAck and ( prefNoAck -or- !hasWriteWithAck )
            final boolean prefWriteNoAck = hasWriteNoAck && ( prefNoAck || !hasWriteWithAck );

            if( prefWriteNoAck ) {
                try {
                    if( !cmdCharRef.writeValue(cmd_data, false /* withResponse */) ) {
                        PrintUtil.fprintf_td(System.err, "Write (noAck) to command failed: Cmd %s, args[%s]\n",
                                cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */));
                        res = HCIStatusCode.FAILED;
                    }
                } catch ( final Throwable t ) {
                    PrintUtil.fprintf_td(System.err, "Exception caught @ Write (noAck) to command failed: Cmd %s, args[%s]: %s\n",
                            cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */), t.toString());
                    res = HCIStatusCode.TIMEOUT;
                }
            } else if( hasWriteWithAck ) {
                try {
                    if( !cmdCharRef.writeValue(cmd_data, true /* withResponse */) ) {
                        PrintUtil.fprintf_td(System.err, "Write (withAck) to command failed: Cmd %s, args[%s]\n",
                                cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */));
                        res = HCIStatusCode.TIMEOUT;
                    }
                } catch ( final Throwable t ) {
                    PrintUtil.fprintf_td(System.err, "Exception caught @ Write (withAck) to command failed: Cmd %s, args[%s]: %s\n",
                            cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */), t.toString());
                    res = HCIStatusCode.TIMEOUT;
                }
            } else {
                PrintUtil.fprintf_td(System.err, "Command has no write property: %s\n", cmdCharRef.toString());
                res = HCIStatusCode.FAILED;
            }

            if( null != rspCharRef && allowResponse ) {
                while( HCIStatusCode.SUCCESS == res &&
                       ( null == rsp_data || rspMinSize > rsp_data.length || ( 0 == rspMinSize && 0 == rsp_data.length ) )
                     )
                {
                    if( 0 == timeoutMS ) {
                        try {
                            mtxRspReceived.wait();
                        } catch (final InterruptedException e) { }
                    } else {
                        try {
                            mtxRspReceived.wait(timeoutMS);
                        } catch (final Throwable t) {}
                        if( null == rsp_data ) {
                            PrintUtil.fprintf_td(System.err, "BTGattCmd.sendBlocking: Timeout: Cmd %s, args[%s]\n",
                                    cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */));
                            res = HCIStatusCode.TIMEOUT;
                        }
                    }
                }
            }
        } // mtxRspReceived
        if( DEBUG && HCIStatusCode.SUCCESS == res ) {
            PrintUtil.fprintf_td(System.err, "BTGattCmd.sendBlocking: OK: Cmd %s, args[%s], Resp %s, result[%s]\n",
                    cmdCharRef.toString(), BasicTypes.bytesHexString(cmd_data, 0, cmd_data.length, true /* lsbFirst */),
                    rspCharStr(), rspDataToString());
        }
        return res;
    }

    @Override
    public String toString() {
        return "BTGattCmd["+dev.getName()+":"+name+", service "+service_uuid+
               ", char[cmd "+cmd_uuid+", rsp "+rsp_uuid+
               ", set["+setup_done+", resolved "+isResolvedEq()+"]]]";
    }
}
