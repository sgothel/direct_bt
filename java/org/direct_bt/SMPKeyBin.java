/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.Date;

import org.direct_bt.SMPKeyMask.KeyType;
import org.jau.net.EUI48;

/**
 * Storage for SMP keys including required connection parameter per local adapter and remote device.
 *
 * File format version 5.
 *
 * Storage for a device's {@link BDAddressAndType}, its security connection setup {@link BTSecurityLevel} + {@link SMPIOCapability}
 * and optionally the initiator and responder {@link SMPLongTermKey LTK}, {@link SMPSignatureResolvingKey CSRK}
 * and {@link SMPLinkKey LK} within one file.
 * <p>
 * Since the {@link SMPLongTermKey LTK}, {@link SMPSignatureResolvingKey CSRK}
 * and {@link SMPLinkKey LK}
 * are optionally set depending on their availability per initiator and responder,
 * implementation supports mixed mode for certain devices.
 * E.g. LTK responder key only etc.
 * </p>
 * <p>
 * Data is stored in {@link ByteOrder#LITTLE_ENDIAN} format, native to Bluetooth.
 * </p>
 * <p>
 * Filename as retrieved by {@link #getFileBasename(BDAddressAndType)} and {@link #getFileBasename()}
 * has the following form {@code bd_010203040506_C026DA01DAB11.key}:
 * <ul>
 * <li>{@code 'bd_'} prefix</li>
 * <li>{@code '010203040506'} local {@link EUI48} local adapter address</li>
 * <li>{@code '_'} separator</li>
 * <li>{@code 'C026DA01DAB1'} remote {@link EUI48} remote device address</li>
 * <li>{@code '1'} {@link BDAddressType}</li>
 * <li>{@code '.key'} suffix</li>
 * </li>
 * </p>
 */
public class SMPKeyBin {
        public static final short VERSION = (short)0b0101010101010101 + (short)5; // bitpattern + version

        private short version;                          //  2
        private short size;                             //  2
        private long ts_creation_sec;                   //  8
        private final BDAddressAndType localAddress;    //  7
        private final BDAddressAndType remoteAddress;   //  7
        private BTSecurityLevel sec_level;;             //  1
        private SMPIOCapability io_cap;                 //  1

        private final SMPKeyMask keys_init;             //  1
        private final SMPKeyMask keys_resp;             //  1

        private SMPLongTermKey ltk_init;                // 28 (optional)
        private SMPIdentityResolvingKey  irk_init;      // 17 (optional)
        private SMPSignatureResolvingKey csrk_init;     // 17 (optional)
        private SMPLinkKey               lk_init;       // 18 (optional)

        private SMPLongTermKey ltk_resp;                // 28 (optional)
        private SMPIdentityResolvingKey  irk_resp;      // 17 (optional)
        private SMPSignatureResolvingKey csrk_resp;     // 17 (optional)
        private SMPLinkKey               lk_resp;       // 18 (optional)

        private static final int byte_size_max = 190;
        private static final int byte_size_min =  30;

        // Min-Max: 30 - 190 bytes

        boolean verbose;

        final private short calcSize() {
            short s = 0;
            s += 2; // sizeof(version);
            s += 2; // sizeof(size);
            s += 8; // sizeof(ts_creation_sec);
            s += 6; // sizeof(localAddress.address);
            s += 1; // sizeof(localAddress.type);
            s += 6; // sizeof(remoteAddress.address);
            s += 1; // sizeof(remoteAddress.type);
            s += 1; // sizeof(sec_level);
            s += 1; // sizeof(io_cap);

            s += 1; // sizeof(keys_init);
            s += 1; // sizeof(keys_resp);

            if( hasLTKInit() ) {
                s += SMPLongTermKey.byte_size; // sizeof(ltk_init);
            }
            if( hasIRKInit() ) {
                s += SMPIdentityResolvingKey.byte_size; // sizeof(irk_init);
            }
            if( hasCSRKInit() ) {
                s += SMPSignatureResolvingKey.byte_size; // sizeof(csrk_init);
            }
            if( hasLKInit() ) {
                s += SMPLinkKey.byte_size; // sizeof(lk_init);
            }

            if( hasLTKResp() ) {
                s += SMPLongTermKey.byte_size; // sizeof(ltk_resp);
            }
            if( hasIRKResp() ) {
                s += SMPIdentityResolvingKey.byte_size; // sizeof(irk_resp);
            }
            if( hasCSRKResp() ) {
                s += SMPSignatureResolvingKey.byte_size; // sizeof(csrk_resp);
            }
            if( hasLKResp() ) {
                s += SMPLinkKey.byte_size; // sizeof(lk_resp);
            }
            return s;
        }

        /**
         * Create a new SMPKeyBin instance based upon given BTDevice's
         * BTSecurityLevel, SMPPairingState, PairingMode and LTK keys.
         *
         * Returned SMPKeyBin shall be tested if valid via {@link SMPKeyBin#isValid()},
         * whether the retrieved data from BTDevice is consistent and hence
         * having BTDevice is a well connected state.
         *
         * @param device the BTDevice from which all required data is derived
         * @return a valid SMPKeyBin instance if properly connected, otherwise an invalid instance.
         * @see BTDevice
         * @see #isValid()
         */
        static public SMPKeyBin create(final BTDevice device) {
            final BTSecurityLevel sec_lvl = device.getConnSecurityLevel();
            final SMPPairingState pstate = device.getPairingState();
            final PairingMode pmode = device.getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)

            final SMPKeyBin smpKeyBin = new SMPKeyBin(device.getAdapter().getAddressAndType(), device.getAddressAndType(),
                                                      device.getConnSecurityLevel(), device.getConnIOCapability());

            if( ( BTSecurityLevel.NONE.value  < sec_lvl.value && SMPPairingState.COMPLETED == pstate && PairingMode.NEGOTIATING.value < pmode.value ) ||
                ( BTSecurityLevel.NONE.value == sec_lvl.value && SMPPairingState.NONE == pstate && PairingMode.NONE == pmode  ) )
            {
                final SMPKeyMask keys_resp = device.getAvailableSMPKeys(true /* responder */);
                final SMPKeyMask keys_init = device.getAvailableSMPKeys(false /* responder */);

                if( keys_init.isSet(SMPKeyMask.KeyType.ENC_KEY) ) {
                    smpKeyBin.setLTKInit( device.getLongTermKey(false /* responder */) );
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.ENC_KEY) ) {
                    smpKeyBin.setLTKResp( device.getLongTermKey(true  /* responder */) );
                }

                if( keys_init.isSet(SMPKeyMask.KeyType.ID_KEY) ) {
                    smpKeyBin.setIRKInit( device.getIdentityResolvingKey(false /* responder */) );
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.ID_KEY) ) {
                    smpKeyBin.setIRKResp( device.getIdentityResolvingKey(true  /* responder */) );
                }

                if( keys_init.isSet(SMPKeyMask.KeyType.SIGN_KEY) ) {
                    smpKeyBin.setCSRKInit( device.getSignatureResolvingKey(false /* responder */) );
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.SIGN_KEY) ) {
                    smpKeyBin.setCSRKResp( device.getSignatureResolvingKey(true  /* responder */) );
                }

                if( keys_init.isSet(SMPKeyMask.KeyType.LINK_KEY) ) {
                    smpKeyBin.setLKInit( device.getLinkKey(false /* responder */) );
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.LINK_KEY) ) {
                    smpKeyBin.setLKResp( device.getLinkKey(true  /* responder */) );
                }
            } else {
                smpKeyBin.size = 0; // explicitly mark invalid
            }
            return smpKeyBin;
        }

        /**
         * Create a new SMPKeyBin instance on the fly based upon given BTDevice's
         * BTSecurityLevel, SMPPairingState, PairingMode and LTK keys.
         * If valid, instance is stored to a file denoted by `path` and {@link BTDevice#getAddressAndType()}.
         *
         * Method returns `false` if resulting SMPKeyBin is not {@link SMPKeyBin#isValid()}.
         *
         * Otherwise, method returns the {@link SMPKeyBin#write(String)} result.
         *
         * @param device the BTDevice from which all required data is derived
         * @param path the path for the stored SMPKeyBin file.
         * @param overwrite if `true` and file already exists, delete file first. If `false` and file exists, return `false` w/o writing.
         * @param verbose_ set to true to have detailed write processing logged to stderr, otherwise false
         * @return `true` if file has been successfully written, otherwise `false`.
         * @see BTDevice
         * @see #create(BTDevice)
         * @see #write(String)
         * @see #isValid()
         */
        static public boolean createAndWrite(final BTDevice device, final String path, final boolean overwrite, final boolean verbose_) {
            final SMPKeyBin smpKeyBin = SMPKeyBin.create(device);
            if( smpKeyBin.isValid() ) {
                smpKeyBin.setVerbose( verbose_ );
                return smpKeyBin.write( getFilename(path, device), overwrite );
            } else {
                if( verbose_ ) {
                    BTUtils.println(System.err, "Create SMPKeyBin: Invalid "+smpKeyBin+", "+device);
                }
                return false;
            }
        }

        /**
         * Create a new SMPKeyBin instance based upon stored file denoted by `fname`.
         *
         * Returned SMPKeyBin shall be tested if valid via {@link SMPKeyBin#isValid()},
         * whether the read() operation was successful and data is consistent.
         *
         * If file is invalid, it is removed.
         *
         * @param fname full path of the stored SMPKeyBin file.
         * @param removeInvalidFile if `true` and file is invalid, remove it. Otherwise keep it alive.
         * @param verbose_ set to true to have detailed read processing logged to stderr, otherwise false
         * @return valid SMPKeyBin instance if file exist and read successfully, otherwise invalid SMPKeyBin instance.
         * @see #isValid()
         * @see #read(String, String)
         */
        static public SMPKeyBin read(final String fname, final boolean verbose_) {
            final SMPKeyBin smpKeyBin = new SMPKeyBin();
            smpKeyBin.setVerbose( verbose_ );
            smpKeyBin.read( fname ); // read failure -> !isValid()
            return smpKeyBin;
        }

        /**
         * Create a new SMPKeyBin instance on the fly based upon stored file denoted by `path` and {@link BTDevice#getAddressAndType()},
         * i.e. `path/` + {@link #getFileBasename(BDAddressAndType)}.
         *
         * Method returns HCIStatusCode#INVALID_PARAMS if resulting SMPKeyBin is not {@link SMPKeyBin#isValid()}.
         *
         * Otherwise, method returns the HCIStatusCode of {@link SMPKeyBin#apply(BTDevice)}.
         *
         * If key file is invalid or key could not be applied, i.e. not returning {@link HCIStatusCode#SUCCESS}, it is removed.
         *
         * @param path the path of the stored SMPKeyBin file.
         * @param device the BTDevice for which address the stored SMPKeyBin file will be read and applied to
         * @param minSecLevel minimum BTSecurityLevel the read SMPKeyBin::sec_level must be compliant to.
         *        If SMPKeyBin::sec_level < minSecLevel method removes the key file and returns {@link HCIStatusCode#ENCRYPTION_MODE_NOT_ACCEPTED}.
         * @param verbose_ set to true to have detailed read processing logged to stderr, otherwise false
         * @return {@link HCIStatusCode#SUCCESS} or error code for failure
         * @see #read(String, BDAddressAndType, boolean)
         * @see #isValid()
         * @see #read(String, String)
         * @see #apply(BTDevice)
         */
        static public HCIStatusCode readAndApply(final String path, final BTDevice device, final BTSecurityLevel minSecLevel, final boolean verbose_) {
            final String fname = getFilename(path, device);
            final SMPKeyBin smpKeyBin = read(fname, verbose_);
            if( smpKeyBin.isValid() ) {
                if( !smpKeyBin.getLocalAddrAndType().equals( device.getAdapter().getAddressAndType() ) ) {
                    if( smpKeyBin.verbose ) {
                        BTUtils.println(System.err, "SMPKeyBin::readAndApply: Local address mismatch: Has "+device.getAdapter().getAddressAndType().toString()+
                                        ", read "+smpKeyBin.getLocalAddrAndType().toString()+", removing file "+fname+": "+smpKeyBin.toString());
                    }
                    remove_impl(fname);
                    return HCIStatusCode.INVALID_PARAMS;
                }
                if( !smpKeyBin.getRemoteAddrAndType().equals( device.getAddressAndType() ) ) {
                    if( smpKeyBin.verbose ) {
                        BTUtils.println(System.err, "SMPKeyBin::readAndApply: Remote address mismatch: Has "+device.getAddressAndType().toString()+
                                        ", read "+smpKeyBin.getRemoteAddrAndType().toString()+", removing file "+fname+": "+smpKeyBin.toString());
                    }
                    remove_impl(fname);
                    return HCIStatusCode.INVALID_PARAMS;
                }
                if( smpKeyBin.sec_level.value < minSecLevel.value ) {
                    if( smpKeyBin.verbose ) {
                        BTUtils.fprintf_td(System.err, "SMPKeyBin::readAndApply: sec_level %s < minimum %s: Key ignored %s, removing file %s\n",
                                smpKeyBin.sec_level.toString(),
                                minSecLevel.toString(),
                                smpKeyBin.toString(), fname);
                    }
                    remove_impl(fname);
                    return HCIStatusCode.ENCRYPTION_MODE_NOT_ACCEPTED;
                }
                final HCIStatusCode res = smpKeyBin.apply(device);
                if( HCIStatusCode.SUCCESS != res ) {
                    if( smpKeyBin.verbose ) {
                        BTUtils.println(System.err, "SMPKeyBin::readAndApply: Apply failed "+res+", "+device+", removing file "+fname);
                    }
                    remove_impl(fname);
                }
                return res;
            } else {
                return HCIStatusCode.INVALID_PARAMS;
            }
        }

        public SMPKeyBin(final BDAddressAndType localAddress_,
                         final BDAddressAndType remoteAddress_,
                         final BTSecurityLevel sec_level_, final SMPIOCapability io_cap_)
        {
            version = VERSION;
            this.size = 0;
            this.ts_creation_sec = BTUtils.wallClockSeconds();
            this.localAddress = localAddress_;
            this.remoteAddress = remoteAddress_;
            this.sec_level = sec_level_;
            this.io_cap = io_cap_;

            this.keys_init = new SMPKeyMask();
            this.keys_resp = new SMPKeyMask();

            this.ltk_init = new SMPLongTermKey();
            this.irk_init = new SMPIdentityResolvingKey();
            this.csrk_init = new SMPSignatureResolvingKey();
            this.lk_init = new SMPLinkKey();

            this.ltk_resp = new SMPLongTermKey();
            this.irk_resp = new SMPIdentityResolvingKey();
            this.csrk_resp = new SMPSignatureResolvingKey();
            this.lk_resp = new SMPLinkKey();

            this.size = calcSize();
        }

        public SMPKeyBin() {
            version = VERSION;
            size = 0;
            ts_creation_sec = 0;
            localAddress = new BDAddressAndType();
            remoteAddress = new BDAddressAndType();
            sec_level = BTSecurityLevel.UNSET;
            io_cap = SMPIOCapability.UNSET;

            keys_init = new SMPKeyMask();
            keys_resp = new SMPKeyMask();

            ltk_init = new SMPLongTermKey();
            irk_init = new SMPIdentityResolvingKey();
            csrk_init = new SMPSignatureResolvingKey();
            lk_init = new SMPLinkKey();

            ltk_resp = new SMPLongTermKey();
            irk_resp = new SMPIdentityResolvingKey();
            csrk_resp = new SMPSignatureResolvingKey();
            lk_resp = new SMPLinkKey();

            size = calcSize();
        }

        final public boolean isVersionValid() { return VERSION==version; }
        final public short getVersion() { return version;}

        final public boolean isSizeValid() { return calcSize() == size;}
        final public short getSize() { return size;}

        /** Returns the creation timestamp in seconds since Unix epoch */
        final public long getCreationTime() { return ts_creation_sec; }

        /** Return the local adapter address. */
        final public BDAddressAndType getLocalAddrAndType() { return localAddress; }

        /** Return the remote device address. */
        final public BDAddressAndType getRemoteAddrAndType() { return remoteAddress; }

        final public BTSecurityLevel getSecLevel() { return sec_level; }
        final public SMPIOCapability getIOCap() { return io_cap; }

        final public boolean hasLTKInit() { return keys_init.isSet(KeyType.ENC_KEY); }
        final public boolean hasIRKInit() { return keys_init.isSet(KeyType.ID_KEY); }
        final public boolean hasCSRKInit() { return keys_init.isSet(KeyType.SIGN_KEY); }
        final public boolean hasLKInit() { return keys_init.isSet(KeyType.LINK_KEY); }
        final public SMPLongTermKey getLTKInit() { return ltk_init; }
        final public SMPIdentityResolvingKey getIRKInit() { return irk_init; }
        final public SMPSignatureResolvingKey getCSRKInit() { return csrk_init; }
        final public SMPLinkKey getLKInit() { return lk_init; }
        final public void setLTKInit(final SMPLongTermKey v) {
            ltk_init = v;
            keys_init.set(KeyType.ENC_KEY);
            size = calcSize();
        }
        final public void setIRKInit(final SMPIdentityResolvingKey v) {
            irk_init = v;
            keys_init.set(KeyType.ID_KEY);
            size = calcSize();
        }
        final public void setCSRKInit(final SMPSignatureResolvingKey v) {
            csrk_init = v;
            keys_init.set(KeyType.SIGN_KEY);
            size = calcSize();
        }
        final public void setLKInit(final SMPLinkKey v) {
            lk_init = v;
            keys_init.set(KeyType.LINK_KEY);
            size = calcSize();
        }

        final public boolean hasLTKResp() { return keys_resp.isSet(KeyType.ENC_KEY); }
        final public boolean hasIRKResp() { return keys_resp.isSet(KeyType.ID_KEY); }
        final public boolean hasCSRKResp() { return keys_resp.isSet(KeyType.SIGN_KEY); }
        final public boolean hasLKResp() { return keys_resp.isSet(KeyType.LINK_KEY); }
        final public SMPLongTermKey getLTKResp() { return ltk_resp; }
        final public SMPIdentityResolvingKey getIRKResp() { return irk_resp; }
        final public SMPSignatureResolvingKey getCSRKResp() { return csrk_resp; }
        final public SMPLinkKey getLKResp() { return lk_resp; }
        final public void setLTKResp(final SMPLongTermKey v) {
            ltk_resp = v;
            keys_resp.set(KeyType.ENC_KEY);
            size = calcSize();
        }
        final public void setIRKResp(final SMPIdentityResolvingKey v) {
            irk_resp = v;
            keys_resp.set(KeyType.ID_KEY);
            size = calcSize();
        }
        final public void setCSRKResp(final SMPSignatureResolvingKey v) {
            csrk_resp = v;
            keys_resp.set(KeyType.SIGN_KEY);
            size = calcSize();
        }
        final public void setLKResp(final SMPLinkKey v) {
            lk_resp = v;
            keys_resp.set(KeyType.LINK_KEY);
            size = calcSize();
        }

        final public void setVerbose(final boolean v) { verbose = v; }

        final public boolean isValid() {
            return isVersionValid() && isSizeValid() &&
                   BTSecurityLevel.UNSET != sec_level &&
                   SMPIOCapability.UNSET != io_cap &&
                   ( !hasLTKInit() || ltk_init.isValid() ) &&
                   ( !hasLTKResp() || ltk_resp.isValid() ) &&
                   ( !hasLKInit()  || lk_init.isValid() )  &&
                   ( !hasLKResp()  || lk_resp.isValid() );
        }

        /**
         * Returns the base filename, see {@link SMPKeyBin} API doc for naming scheme.
         */
        final public String getFileBasename() {
            final String r = "bd_"+localAddress.address.toString()+"_"+remoteAddress.address.toString()+remoteAddress.type.value+".key";
            return r.replace(":", "");
        }
        /**
         * Returns the base filename, see {@link SMPKeyBin} API doc for naming scheme.
         */
        final public static String getFileBasename(final BDAddressAndType localAddress_, final BDAddressAndType remoteAddress_) {
            final String r = "bd_"+localAddress_.address.toString()+"_"+remoteAddress_.address.toString()+remoteAddress_.type.value+".key";
            return r.replace(":", "");
        }
        final public static String getFilename(final String path, final BDAddressAndType localAddress_, final BDAddressAndType remoteAddress_) {
            return path + "/" + getFileBasename(localAddress_, remoteAddress_);
        }
        final public static String getFilename(final String path, final BTDevice remoteDevice) {
            return getFilename(path, remoteDevice.getAdapter().getAddressAndType(), remoteDevice.getAddressAndType());
        }

        @Override
        final public String toString() {
            final StringBuilder res = new StringBuilder();
            res.append("SMPKeyBin[local").append(localAddress.toString()).append(", remote ").append(remoteAddress.toString()).append(", sec ").append(sec_level).append(", io ").append(io_cap).append(", ");
            if( isVersionValid() ) {
                boolean comma = false;
                res.append("Init[");
                if( hasLTKInit() && null != ltk_init ) {
                    res.append(ltk_init.toString());
                    comma = true;
                }
                if( hasIRKInit() && null != irk_init ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(irk_init.toString());
                    comma = true;
                }
                if( hasCSRKInit() && null != csrk_init ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(csrk_init.toString());
                    comma = true;
                }
                if( hasLKInit() && null != lk_init ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(lk_init.toString());
                    comma = true;
                }
                comma = false;
                res.append("], Resp[");
                if( hasLTKResp() && null != ltk_resp ) {
                    res.append(ltk_resp.toString());
                    comma = true;
                }
                if( hasIRKResp() && null != irk_resp ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(irk_resp.toString());
                    comma = true;
                }
                if( hasCSRKResp() && null != csrk_resp ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(csrk_resp.toString());
                    comma = true;
                }
                if( hasLKResp() && null != lk_resp ) {
                    if( comma ) {
                        res.append(", ");
                    }
                    res.append(lk_resp.toString());
                    comma = true;
                }
                res.append("], ");
            }
            res.append("ver[0x").append(Integer.toHexString(version)).append(", ok ").append(isVersionValid()).append("], size[").append(size);
            if( verbose ) {
                res.append(", calc ").append(calcSize());
            }
            res.append(", valid ").append(isSizeValid()).append("], ");
            {
                final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
                final Date d = new Date(ts_creation_sec*1000); // s -> ms
                res.append(sdf.format(d));
            }
            res.append(", valid ").append(isValid()).append("]");

            return res.toString();
        }

        final public static boolean remove(final String path, final BTDevice remoteDevice_) {
            return remove(path, remoteDevice_.getAdapter().getAddressAndType(), remoteDevice_.getAddressAndType());
        }
        final public static boolean remove(final String path, final BDAddressAndType localAddress_, final BDAddressAndType remoteAddress_) {
            final String fname = getFilename(path, localAddress_, remoteAddress_);
            return remove_impl(fname);
        }
        final private static boolean remove_impl(final String fname) {
            final File file = new File( fname );
            try {
                return file.delete(); // alternative to truncate, if existing
            } catch (final Exception ex) {
                BTUtils.println(System.err, "Remove SMPKeyBin: Failed "+fname+": "+ex.getMessage());
                ex.printStackTrace();
                return false;
            }
        }

        final public boolean write(final String fname, final boolean overwrite) {
            if( !isValid() ) {
                BTUtils.println(System.err, "Write SMPKeyBin: Invalid (skipped) "+toString());
                return false;
            }
            final File file = new File( fname );
            OutputStream out = null;
            try {
                if( file.exists() ) {
                    if( overwrite ) {
                        if( !file.delete() ) {
                            BTUtils.println(System.err, "Write SMPKeyBin: Failed deletion of existing file "+fname+": "+toString());
                            return false;
                        }
                    } else {
                        BTUtils.println(System.err, "Write SMPKeyBin: Not overwriting existing file "+fname+": "+toString());
                        return false;
                    }
                }
                final byte[] buffer = new byte[byte_size_max];

                out = new FileOutputStream(file);
                out.write( (byte)  version );
                out.write( (byte)( version >> 8 ) );
                out.write( (byte)  size );
                out.write( (byte)( size >> 8 ) );

                writeLong(ts_creation_sec, out, buffer);
                {
                    localAddress.address.put(buffer, 0, ByteOrder.LITTLE_ENDIAN);
                    out.write(buffer, 0, localAddress.address.b.length);
                }
                out.write(localAddress.type.value);
                {
                    remoteAddress.address.put(buffer, 0, ByteOrder.LITTLE_ENDIAN);
                    out.write(buffer, 0, remoteAddress.address.b.length);
                }
                out.write(remoteAddress.type.value);
                out.write(sec_level.value);
                out.write(io_cap.value);

                out.write(keys_init.mask);
                out.write(keys_resp.mask);

                if( hasLTKInit() ) {
                    ltk_init.put(buffer, 0);
                    out.write(buffer, 0, SMPLongTermKey.byte_size);
                }
                if( hasIRKInit() ) {
                    irk_init.put(buffer, 0);
                    out.write(buffer, 0, SMPIdentityResolvingKey.byte_size);
                }
                if( hasCSRKInit() ) {
                    csrk_init.put(buffer, 0);
                    out.write(buffer, 0, SMPSignatureResolvingKey.byte_size);
                }
                if( hasLKInit() ) {
                    lk_init.put(buffer, 0);
                    out.write(buffer, 0, SMPLinkKey.byte_size);
                }

                if( hasLTKResp() ) {
                    ltk_resp.put(buffer, 0);
                    out.write(buffer, 0, SMPLongTermKey.byte_size);
                }
                if( hasIRKResp() ) {
                    irk_resp.put(buffer, 0);
                    out.write(buffer, 0, SMPIdentityResolvingKey.byte_size);
                }
                if( hasCSRKResp() ) {
                    csrk_resp.put(buffer, 0);
                    out.write(buffer, 0, SMPSignatureResolvingKey.byte_size);
                }
                if( hasLKResp() ) {
                    lk_resp.put(buffer, 0);
                    out.write(buffer, 0, SMPLinkKey.byte_size);
                }

                if( verbose ) {
                    BTUtils.println(System.err, "Write SMPKeyBin: "+fname+": "+toString());
                }
                return true;
            } catch (final Exception ex) {
                BTUtils.println(System.err, "Write SMPKeyBin: Failed "+fname+": "+toString()+": "+ex.getMessage());
                ex.printStackTrace();
            } finally {
                try {
                    if( null != out ) {
                        out.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }

        final public boolean read(final String fname) {
            final File file = new File(fname);
            InputStream in = null;
            int remaining = 0;
            boolean err = false;
            try {
                if( !file.canRead() ) {
                    if( verbose ) {
                        BTUtils.println(System.err, "Read SMPKeyBin: Failed "+fname+": Not existing or readable: "+toString());
                    }
                    size = 0; // explicitly mark invalid
                    return false;
                }
                final byte[] buffer = new byte[byte_size_max];
                in = new FileInputStream(file);
                read(in, buffer, byte_size_min, fname);

                int i=0;
                version = (short) ( buffer[i++] | ( buffer[i++] << 8 ) );
                size = (short) ( buffer[i++] | ( buffer[i++] << 8 ) );

                remaining = size - 2 /* sizeof(version) */ - 2 /* sizeof(size) */;

                if( !err && 8 <= remaining ) {
                    ts_creation_sec = getLong(buffer, i); i+=8;
                    remaining -= 8;
                } else {
                    err = true;
                }
                if( !err && 7+7+4 <= remaining ) {
                    localAddress.address = new EUI48(buffer, i, ByteOrder.LITTLE_ENDIAN); i+=6;
                    localAddress.type = BDAddressType.get(buffer[i++]);
                    remoteAddress.address = new EUI48(buffer, i, ByteOrder.LITTLE_ENDIAN); i+=6;
                    remoteAddress.type = BDAddressType.get(buffer[i++]);
                    sec_level = BTSecurityLevel.get(buffer[i++]);
                    io_cap = SMPIOCapability.get(buffer[i++]);

                    keys_init.mask = buffer[i++];
                    keys_resp.mask = buffer[i++];

                    remaining -= 7+7+4;
                } else {
                    err = true;
                }

                if( !err && hasLTKInit() ) {
                    if( SMPLongTermKey.byte_size <= remaining ) {
                        read(in, buffer, SMPLongTermKey.byte_size, fname);
                        ltk_init.get(buffer, 0);
                        remaining -= SMPLongTermKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasIRKInit() ) {
                    if( SMPIdentityResolvingKey.byte_size <= remaining ) {
                        read(in, buffer, SMPIdentityResolvingKey.byte_size, fname);
                        irk_init.get(buffer, 0);
                        remaining -= SMPIdentityResolvingKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasCSRKInit() ) {
                    if( SMPSignatureResolvingKey.byte_size <= remaining ) {
                        read(in, buffer, SMPSignatureResolvingKey.byte_size, fname);
                        csrk_init.get(buffer, 0);
                        remaining -= SMPSignatureResolvingKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasLKInit() ) {
                    if( SMPLinkKey.byte_size <= remaining ) {
                        read(in, buffer, SMPLinkKey.byte_size, fname);
                        lk_init.get(buffer, 0);
                        remaining -= SMPLinkKey.byte_size;
                    } else {
                        err = true;
                    }
                }

                if( !err && hasLTKResp() ) {
                    if( SMPLongTermKey.byte_size <= remaining ) {
                        read(in, buffer, SMPLongTermKey.byte_size, fname);
                        ltk_resp.get(buffer, 0);
                        remaining -= SMPLongTermKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasIRKResp() ) {
                    if( SMPIdentityResolvingKey.byte_size <= remaining ) {
                        read(in, buffer, SMPIdentityResolvingKey.byte_size, fname);
                        irk_resp.get(buffer, 0);
                        remaining -= SMPIdentityResolvingKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasCSRKResp() ) {
                    if( SMPSignatureResolvingKey.byte_size <= remaining ) {
                        read(in, buffer, SMPSignatureResolvingKey.byte_size, fname);
                        csrk_resp.get(buffer, 0);
                        remaining -= SMPSignatureResolvingKey.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasLKResp() ) {
                    if( SMPLinkKey.byte_size <= remaining ) {
                        read(in, buffer, SMPLinkKey.byte_size, fname);
                        lk_resp.get(buffer, 0);
                        remaining -= SMPLinkKey.byte_size;
                    } else {
                        err = true;
                    }
                }

                if( !err ) {
                    err = !isValid();
                }
            } catch (final Exception ex) {
                BTUtils.println(System.err, "Read SMPKeyBin: Failed "+fname+": "+toString()+": "+ex.getMessage());
                ex.printStackTrace();
                err = true;
            } finally {
                try {
                    if( null != in ) {
                        in.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            if( err ) {
                file.delete();
                if( verbose ) {
                    BTUtils.println(System.err, "Read SMPKeyBin: Failed "+fname+" (removed): "+toString()+", remaining "+remaining);
                }
                size = 0; // explicitly mark invalid
            } else {
                if( verbose ) {
                    BTUtils.println(System.err, "Read SMPKeyBin: OK "+fname+": "+toString()+", remaining "+remaining);
                }
            }
            return err;
        }
        final static private int read(final InputStream in, final byte[] buffer, final int rsize, final String fname) throws IOException {
            final int read_count = in.read(buffer, 0, rsize);
            if( read_count != rsize ) {
                throw new IOException("Couldn't read "+rsize+" bytes, only "+read_count+" from "+fname);
            }
            return read_count;
        }
        final static private long getLong(final byte[] buffer, final int offset) throws IOException {
            long res = 0;
            for (int j = 0; j < 8; ++j) {
                res |= ((long) buffer[offset+j] & 0xff) << j*8;
            }
            return res;
        }
        final static private void writeLong(final long value, final OutputStream out, final byte[] buffer) throws IOException {
            for (int i = 0; i < 8; ++i) {
                buffer[i] = (byte) (value >> i*8);
            }
            out.write(buffer, 0, 8);
        }

        /**
         * If this instance isValid() and initiator or responder LTK available, i.e. hasLTKInit() or hasLTKResp(),
         * the following procedure will be applied to the given BTDevice:
         *
         * - If BTSecurityLevel _is_ BTSecurityLevel::NONE
         *   + Setting security to ::BTSecurityLevel::NONE and SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
         * - else if BTSecurityLevel > BTSecurityLevel::NONE
         *   + Setting security to ::BTSecurityLevel::ENC_ONLY and SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
         *   + Setting initiator LTK from getLTKInit() via BTDevice::setLongTermKeyInfo(), if available
         *   + Setting responder LTK from getLTKResp() via BTDevice::setLongTermKeyInfo(), if available
         *
         * If all three operations succeed, HCIStatusCode::SUCCESS will be returned,
         * otherwise the appropriate status code below.
         *
         * BTSecurityLevel::ENC_ONLY is set to avoid a new SMP PairingMode negotiation,
         * which is undesired as this instances' stored LTK shall be used for PairingMode::PRE_PAIRED.
         *
         * Method may fail for any of the following reasons:
         *
         *  Reason                                                     | HCIStatusCode                             |
         *  :--------------------------------------------------------  | :---------------------------------------- |
         *  ! isValid()                                                | HCIStatusCode::INVALID_PARAMS             |
         *  ! hasLTKInit() && ! hasLTKResp()                           | HCIStatusCode::INVALID_PARAMS             |
         *  } BTDevice::isValid() == false                             | HCIStatusCode::INVALID_PARAMS             |
         *  | BTDevice has already being connected                     | HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  | BTDevice::connectLE() or BTDevice::connectBREDR() called | HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  | BTDevice::setLongTermKeyInfo() failed                    | HCIStatusCode from BT adapter             |
         *  | BTDevice::setLinkKeyInfo() failed                        | ::HCIStatusCode from BT adapter           |
         *
         * On failure and after BTDevice::setConnSecurity() has been performed, the ::BTSecurityLevel
         * and ::SMPIOCapability pre-connect values have been written and must be set by the caller again.
         *
         * @param device the BTDevice for which this instances' LTK shall be applied
         *
         * @see isValid()
         * @see hasLTKInit()
         * @see hasLTKResp()
         * @see getLTKInit()
         * @see getLTKResp()
         * @see hasLKInit()
         * @see hasLKResp()
         * @see getLKInit()
         * @see getLKResp()
         * @see BTSecurityLevel
         * @see SMPIOCapability
         * @see BTDevice::isValid()
         * @see BTDevice::setConnSecurity()
         * @see BTDevice::setLongTermKeyInfo()
         * @see BTDevice::setLinkKeyInfo()
         */
        final public HCIStatusCode apply(final BTDevice device) {
            HCIStatusCode res = HCIStatusCode.SUCCESS;

            // Must be a valid SMPKeyBin instance and at least one LTK key if using encryption.
            if( !isValid() || ( BTSecurityLevel.NONE != sec_level && !hasLTKInit() && !hasLTKResp() ) ) {
                res = HCIStatusCode.INVALID_PARAMS;
                if( verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: SMPKeyBin Status: "+res+", "+toString());
                }
                return res;
            }
            if( !device.isValid() ) {
                res = HCIStatusCode.INVALID_PARAMS;
                if( verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Device Invalid: "+res+", "+toString()+", "+device);
                }
                return res;
            }

            // Allow no encryption at all, i.e. BTSecurityLevel::NONE
            final BTSecurityLevel applySecLevel = BTSecurityLevel.NONE == sec_level ?
                                                  BTSecurityLevel.NONE : BTSecurityLevel.ENC_ONLY;

            if( !device.setConnSecurity(applySecLevel, SMPIOCapability.NO_INPUT_NO_OUTPUT) ) {
                res = HCIStatusCode.CONNECTION_ALREADY_EXISTS;
                if( verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Device Connected/ing: "+res+", "+toString()+", "+device);
                }
                return res;
            }

            if( hasLTKInit() ) {
                res = device.setLongTermKey( getLTKInit() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Init-LTK Upload: "+res+", "+toString()+", "+device);
                }
            }
            if( HCIStatusCode.SUCCESS == res && hasLTKResp() ) {
                res = device.setLongTermKey( getLTKResp() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Resp-LTK Upload: "+res+", "+toString()+", "+device);
                }
            }

            if( HCIStatusCode.SUCCESS == res && hasIRKInit() ) {
                res = device.setIdentityResolvingKey( getIRKInit() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Init-IRK Upload: "+res+", "+toString()+", "+device);
                }
            }
            if( HCIStatusCode.SUCCESS == res && hasIRKResp() ) {
                res = device.setIdentityResolvingKey( getIRKResp() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Resp-IRK Upload: "+res+", "+toString()+", "+device);
                }
            }

            if( HCIStatusCode.SUCCESS == res && hasCSRKInit() ) {
                res = device.setSignatureResolvingKey( getCSRKInit() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Init-CSRK Upload: "+res+", "+toString()+", "+device);
                }
            }
            if( HCIStatusCode.SUCCESS == res && hasCSRKResp() ) {
                res = device.setSignatureResolvingKey( getCSRKResp() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Resp-CSRK Upload: "+res+", "+toString()+", "+device);
                }
            }

            if( HCIStatusCode.SUCCESS == res && hasLKInit() ) {
                res = device.setLinkKey( getLKInit() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Init-LK Upload: "+res+", "+toString()+", "+device);
                }
            }
            if( HCIStatusCode.SUCCESS == res && hasLKResp() ) {
                res = device.setLinkKey( getLKResp() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    BTUtils.println(System.err, "Apply SMPKeyBin failed: Resp-LK Upload: "+res+", "+toString()+", "+device);
                }
            }

            return res;
        }
};
