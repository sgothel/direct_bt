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

import org.direct_bt.SMPKeyMask.KeyType;

/**
 * Storage for SMP keys including the required connection parameter.
 *
 * Storage for a device's {@link BDAddressAndType}, its security connection setup {@link BTSecurityLevel} + {@link SMPIOCapability}
 * and optionally the initiator and responder {@link SMPLongTermKeyInfo LTK} and {@link SMPSignatureResolvingKeyInfo CSRK} within one file.
 * <p>
 * Since the {@link SMPLongTermKeyInfo LTK} and {@link SMPSignatureResolvingKeyInfo CSRK}
 * can be optionally set due to their availability per initiator and responder,
 * implementation supports mixed mode for certain devices.
 * E.g. LTK responder key only etc.
 * </p>
 */
public class SMPKeyBin {
        public static final short VERSION = (short)0b0101010101010101 + (short)1; // bitpattern + version

        private short version;                          //  2
        private short  size;                            //  2
        private final BDAddressAndType addrAndType;     //  7
        private BTSecurityLevel sec_level;;             //  1
        private SMPIOCapability io_cap;                 //  1

        private final SMPKeyMask keys_init;             //  1
        private final SMPKeyMask keys_resp;             //  1

        private SMPLongTermKeyInfo ltk_init;            // 28 (optional)
        private SMPSignatureResolvingKeyInfo csrk_init; // 17 (optional)

        private SMPLongTermKeyInfo ltk_resp;            // 28 (optional)
        private SMPSignatureResolvingKeyInfo csrk_resp; // 17 (optional)

        private static final int byte_size_max = 105;
        private static final int byte_size_min =  15;
        // Min-Max: 15 - 105 bytes

        boolean verbose;

        final private short calcSize() {
            short s = 0;
            s += 2; // sizeof(version);
            s += 2; // sizeof(size);
            s += 6; // sizeof(addrAndType.address);
            s += 1; // sizeof(addrAndType.type);
            s += 1; // sizeof(sec_level);
            s += 1; // sizeof(io_cap);

            s += 1; // sizeof(keys_init);
            s += 1; // sizeof(keys_resp);

            if( hasLTKInit() ) {
                s += 28; // sizeof(ltk_init);
            }
            if( hasCSRKInit() ) {
                s += 17; // sizeof(csrk_init);
            }

            if( hasLTKResp() ) {
                s += 28; // sizeof(ltk_resp);
            }
            if( hasCSRKResp() ) {
                s += 17; // sizeof(csrk_resp);
            }
            return s;
        }

        public SMPKeyBin(final BDAddressAndType addrAndType_,
                         final BTSecurityLevel sec_level_, final SMPIOCapability io_cap_)
        {
            version = VERSION;
            this.size = 0;
            this.addrAndType = addrAndType_;
            this.sec_level = sec_level_;
            this.io_cap = io_cap_;

            this.keys_init = new SMPKeyMask();
            this.keys_resp = new SMPKeyMask();

            this.ltk_init = new SMPLongTermKeyInfo();
            this.csrk_init = new SMPSignatureResolvingKeyInfo();

            this.ltk_resp = new SMPLongTermKeyInfo();
            this.csrk_resp = new SMPSignatureResolvingKeyInfo();

            this.size = calcSize();
        }

        public SMPKeyBin() {
            version = VERSION;
            size = 0;
            addrAndType = new BDAddressAndType();
            sec_level = BTSecurityLevel.UNSET;
            io_cap = SMPIOCapability.UNSET;

            keys_init = new SMPKeyMask();
            keys_resp = new SMPKeyMask();

            ltk_init = new SMPLongTermKeyInfo();
            csrk_init = new SMPSignatureResolvingKeyInfo();

            ltk_resp = new SMPLongTermKeyInfo();
            csrk_resp = new SMPSignatureResolvingKeyInfo();

            size = calcSize();
        }

        final public boolean isVersionValid() { return VERSION==version; }
        final public short getVersion() { return version;}

        final public boolean isSizeValid() { return calcSize() == size;}
        final public short getSize() { return size;}

        final public BDAddressAndType getAddrAndType() { return addrAndType; }
        final public BTSecurityLevel getSecLevel() { return sec_level; }
        final public SMPIOCapability getIOCap() { return io_cap; }

        final public boolean hasLTKInit() { return keys_init.isSet(KeyType.ENC_KEY); }
        final public boolean hasCSRKInit() { return keys_init.isSet(KeyType.SIGN_KEY); }
        final public SMPLongTermKeyInfo getLTKInit() { return ltk_init; }
        final public SMPSignatureResolvingKeyInfo getCSRKInit() { return csrk_init; }
        final public void setLTKInit(final SMPLongTermKeyInfo v) {
            ltk_init = v;
            keys_init.set(KeyType.ENC_KEY);
            size = calcSize();
        }
        final public void setCSRKInit(final SMPSignatureResolvingKeyInfo v) {
            csrk_init = v;
            keys_init.set(KeyType.SIGN_KEY);
            size = calcSize();
        }

        final public boolean hasLTKResp() { return keys_resp.isSet(KeyType.ENC_KEY); }
        final public boolean hasCSRKResp() { return keys_resp.isSet(KeyType.SIGN_KEY); }
        final public SMPLongTermKeyInfo getLTKResp() { return ltk_resp; }
        final public SMPSignatureResolvingKeyInfo getCSRKResp() { return csrk_resp; }
        final public void setLTKResp(final SMPLongTermKeyInfo v) {
            ltk_resp = v;
            keys_resp.set(KeyType.ENC_KEY);
            size = calcSize();
        }
        final public void setCSRKResp(final SMPSignatureResolvingKeyInfo v) {
            csrk_resp = v;
            keys_resp.set(KeyType.SIGN_KEY);
            size = calcSize();
        }

        final public void setVerbose(final boolean v) { verbose = v; }

        final public boolean isValid() {
            return isVersionValid() && isSizeValid() &&
                   BTSecurityLevel.UNSET != sec_level &&
                   SMPIOCapability.UNSET != io_cap &&
                   ( !hasLTKInit() || ltk_init.isValid() ) &&
                   ( !hasLTKResp() || ltk_resp.isValid() );
        }

        final public String getFileBasename() {
            return "bd_"+addrAndType.address.toString()+":"+addrAndType.type.value+".smpkey.bin";
        }
        final public static String getFileBasename(final BDAddressAndType addrAndType_) {
            return "bd_"+addrAndType_.address.toString()+":"+addrAndType_.type.value+".smpkey.bin";
        }

        @Override
        final public String toString() {
            final StringBuilder res = new StringBuilder();
            res.append("SMPKeyBin[").append(addrAndType.toString()).append(", sec ").append(sec_level).append(", io ").append(io_cap).append(", ");
            if( isVersionValid() ) {
                res.append("Init[");
                if( hasLTKInit() && null != ltk_init ) {
                    res.append(ltk_init.toString());
                }
                if( hasCSRKInit() && null != csrk_init ) {
                    if( hasLTKInit() ) {
                        res.append(", ");
                    }
                    res.append(csrk_init.toString());
                }
                res.append("], Resp[");
                if( hasLTKResp() && null != ltk_resp ) {
                    res.append(ltk_resp.toString());
                }
                if( hasCSRKResp() && null != csrk_resp ) {
                    if( hasLTKResp() ) {
                        res.append(", ");
                    }
                    res.append(csrk_resp.toString());
                }
                res.append("], ");
            }
            res.append("ver[0x").append(Integer.toHexString(version)).append(", ok ").append(isVersionValid()).append("], size[").append(size);
            if( verbose ) {
                res.append(", calc ").append(calcSize());
            }
            res.append(", valid ").append(isSizeValid()).append("], valid ").append(isValid()).append("]");

            return res.toString();
        }

        final public boolean write(final String path, final String basename) {
            if( !isValid() ) {
                System.err.println("****** WRITE SMPKeyBin: Invalid (skipped) "+toString());
                return false;
            }
            final String fname = path+"/"+basename;
            final File file = new File(fname);
            OutputStream out = null;
            try {
                file.delete(); // alternative to truncate, if existing
                out = new FileOutputStream(file);
                out.write( (byte)  version );
                out.write( (byte)( version >> 8 ) );
                out.write( (byte)  size );
                out.write( (byte)( size >> 8 ) );
                out.write(addrAndType.address.b);
                out.write(addrAndType.type.value);
                out.write(sec_level.value);
                out.write(io_cap.value);

                out.write(keys_init.mask);
                out.write(keys_resp.mask);

                if( hasLTKInit() ) {
                    final byte[] ltk_init_b = new byte[SMPLongTermKeyInfo.byte_size];
                    ltk_init.getStream(ltk_init_b, 0);
                    out.write(ltk_init_b);
                }
                if( hasCSRKInit() ) {
                    final byte[] csrk_init_b = new byte[SMPSignatureResolvingKeyInfo.byte_size];
                    csrk_init.getStream(csrk_init_b, 0);
                    out.write(csrk_init_b);
                }

                if( hasLTKResp() ) {
                    final byte[] ltk_resp_b = new byte[SMPLongTermKeyInfo.byte_size];
                    ltk_resp.getStream(ltk_resp_b, 0);
                    out.write(ltk_resp_b);
                }
                if( hasCSRKResp() ) {
                    final byte[] csrk_resp_b = new byte[SMPSignatureResolvingKeyInfo.byte_size];
                    csrk_resp.getStream(csrk_resp_b, 0);
                    out.write(csrk_resp_b);
                }
                if( verbose ) {
                    System.err.println("****** WRITE SMPKeyBin: "+fname+": "+toString());
                }
                return true;
            } catch (final Exception ex) {
                System.err.println("****** WRITE SMPKeyBin: Failed "+fname+": "+toString()+": "+ex.getMessage());
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
        final public boolean write(final String path) {
            return write( path, getFileBasename() );
        }

        final public boolean read(final String path, final String basename) {
            final String fname = path+"/"+basename;
            final File file = new File(fname);
            InputStream in = null;
            try {
                if( !file.canRead() ) {
                    if( verbose ) {
                        System.err.println("****** READ SMPKeyBin: Failed "+fname+": Not existing or readable: "+toString());
                    }
                    return false;
                }
                final byte[] buffer = new byte[byte_size_max];
                in = new FileInputStream(file);
                read(in, buffer, byte_size_min, fname);

                int i=0;
                version = (short) ( buffer[i++] | ( buffer[i++] << 8 ) );
                size = (short) ( buffer[i++] | ( buffer[i++] << 8 ) );
                addrAndType.address.putStream(buffer, i); i+=6;
                addrAndType.type = BDAddressType.get(buffer[i++]);
                sec_level = BTSecurityLevel.get(buffer[i++]);
                io_cap = SMPIOCapability.get(buffer[i++]);

                keys_init.mask = buffer[i++];
                keys_resp.mask = buffer[i++];

                int remaining = size - i; // i == byte_size_min == 15
                boolean err = false;

                if( !err && hasLTKInit() ) {
                    if( SMPLongTermKeyInfo.byte_size <= remaining ) {
                        read(in, buffer, SMPLongTermKeyInfo.byte_size, fname);
                        ltk_init.putStream(buffer, 0);
                        remaining -= SMPLongTermKeyInfo.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasCSRKInit() ) {
                    if( SMPSignatureResolvingKeyInfo.byte_size <= remaining ) {
                        read(in, buffer, SMPSignatureResolvingKeyInfo.byte_size, fname);
                        csrk_init.putStream(buffer, 0);
                        remaining -= SMPSignatureResolvingKeyInfo.byte_size;
                    } else {
                        err = true;
                    }
                }

                if( !err && hasLTKResp() ) {
                    if( SMPLongTermKeyInfo.byte_size <= remaining ) {
                        read(in, buffer, SMPLongTermKeyInfo.byte_size, fname);
                        ltk_resp.putStream(buffer, 0);
                        remaining -= SMPLongTermKeyInfo.byte_size;
                    } else {
                        err = true;
                    }
                }
                if( !err && hasCSRKResp() ) {
                    if( SMPSignatureResolvingKeyInfo.byte_size <= remaining ) {
                        read(in, buffer, SMPSignatureResolvingKeyInfo.byte_size, fname);
                        csrk_resp.putStream(buffer, 0);
                        remaining -= SMPSignatureResolvingKeyInfo.byte_size;
                    } else {
                        err = true;
                    }
                }

                if( verbose ) {
                    System.err.println("****** READ SMPKeyBin: "+fname+": "+toString()+", remaining "+remaining);
                }
                return isValid();
            } catch (final Exception ex) {
                System.err.println("****** READ SMPKeyBin: Failed "+fname+": "+toString()+": "+ex.getMessage());
                ex.printStackTrace();
            } finally {
                try {
                    if( null != in ) {
                        in.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }
        final public boolean read(final String path, final BDAddressAndType addrAndType_) {
            return read(path, getFileBasename(addrAndType_));
        }

        final static private int read(final InputStream in, final byte[] buffer, final int rsize, final String fname) throws IOException {
            final int read_count = in.read(buffer, 0, rsize);
            if( read_count != rsize ) {
                throw new IOException("Couldn't read "+rsize+" bytes, only "+read_count+" from "+fname);
            }
            return read_count;
        }

        /**
         * If this instance isValid() and initiator or responder LTK available, i.e. hasLTKInit() or hasLTKResp(),
         * the following procedure will be applied to the given BTDevice:
         *
         * - Setting security to BTSecurityLevel::ENC_ONLY and SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
         * - Setting initiator LTK from getLTKInit() via BTDevice::setLongTermKeyInfo(), if available
         * - Setting responder LTK from getLTKResp() via BTDevice::setLongTermKeyInfo(), if available
         *
         * If all three operations succeed, HCIStatusCode::SUCCESS will be returned,
         * otherwise the appropriate status code below.
         *
         * BTSecurityLevel::ENC_ONLY is set to avoid a new SMP PairingMode negotiation,
         * which is undesired as this instances' stored LTK shall be used for PairingMode::PRE_PAIRED.
         *
         * Method may fail for any of the following reasons:
         *
         *  Reason                                                   | HCIStatusCode                             |
         *  :------------------------------------------------------  | :---------------------------------------- |
         *  ! isValid()                                              | HCIStatusCode::INVALID_PARAMS             |
         *  ! hasLTKInit() && ! hasLTKResp()                         | HCIStatusCode::INVALID_PARAMS             |
         *  BTDevice::isValid() == false                             | HCIStatusCode::INVALID_PARAMS             |
         *  BTDevice has already being connected                     | HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  BTDevice::connectLE() or BTDevice::connectBREDR() called | HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  BTDevice::setLongTermKeyInfo() failed                    | HCIStatusCode from BT adapter             |
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
         * @see BTSecurityLevel
         * @see SMPIOCapability
         * @see BTDevice::isValid()
         * @see BTDevice::setConnSecurity()
         * @see BTDevice::setLongTermKeyInfo()
         */
        final public HCIStatusCode apply(final BTDevice device) {
            HCIStatusCode res = HCIStatusCode.SUCCESS;

            if( !isValid() || ( !hasLTKInit() && !hasLTKResp() ) ) {
                res = HCIStatusCode.INVALID_PARAMS;
                if( verbose ) {
                    System.err.println("****** APPLY SMPKeyBin failed: SMPKeyBin Status: "+res+", "+toString());
                }
                return res;
            }
            if( !device.isValid() ) {
                res = HCIStatusCode.INVALID_PARAMS;
                if( verbose ) {
                    System.err.println("****** APPLY SMPKeyBin failed: Device Invalid: "+res+", "+toString()+", "+device);
                }
                return res;
            }

            if( !device.setConnSecurity(BTSecurityLevel.ENC_ONLY, SMPIOCapability.NO_INPUT_NO_OUTPUT) ) {
                res = HCIStatusCode.CONNECTION_ALREADY_EXISTS;
                if( verbose ) {
                    System.err.println("****** APPLY SMPKeyBin failed: Device Connected/ing: "+res+", "+toString()+", "+device);
                }
                return res;
            }

            if( hasLTKInit() ) {
                res = device.setLongTermKeyInfo( getLTKInit() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    System.err.println("****** APPLY SMPKeyBin failed: Init-LTK Upload: "+res+", "+toString()+", "+device);
                }
            }

            if( HCIStatusCode.SUCCESS == res && hasLTKResp() ) {
                res = device.setLongTermKeyInfo( getLTKResp() );
                if( HCIStatusCode.SUCCESS != res && verbose ) {
                    System.err.println("****** APPLY SMPKeyBin failed: Resp-LTK Upload: "+res+", "+toString()+", "+device);
                }
            }

            return res;
        }
};
