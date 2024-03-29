/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

package trial.org.direct_bt;

import java.io.File;
import java.nio.file.Files;

import org.direct_bt.BTFactory;
import org.jau.io.PrintUtil;

public class DBTUtils {
    public static final boolean mkdirKeyFolder() {
        BTFactory.initDirectBTLibrary();
        boolean res = true;
        {
            final File file = new File(DBTConstants.CLIENT_KEY_PATH);
            try {
                if( !file.isDirectory() ) {
                    final boolean res2 = file.mkdirs();
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': mkdir: "+res2);
                    res = res && res2;
                } else {
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': already exists");
                }
            } catch(final Throwable t) {
                PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': Caught "+t.getMessage());
                res = false;
            }
        }
        if( res ) {
            final File file = new File(DBTConstants.SERVER_KEY_PATH);
            try {
                if( !file.isDirectory() ) {
                    final boolean res2 = file.mkdirs();
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': mkdir: "+res2);
                    res = res && res2;
                } else {
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': already exists");
                }
            } catch(final Throwable t) {
                PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': Caught "+t.getMessage());
                res = false;
            }
        }
        return res;
    }

    /**
     *
     * @param file
     * @param recursive
     * @return true only if the file or the directory with content has been deleted, otherwise false
     */
    private static boolean delete(final File file, final boolean recursive) {
        boolean rm_parent = true;
        final File[] contents = file.listFiles();
        if (contents != null) {
            for (final File f : contents) {
                if( f.isDirectory() && !Files.isSymbolicLink( f.toPath() ) ) {
                    if( recursive ) {
                        rm_parent = delete(f, true) && rm_parent;
                    } else {
                        // can't empty contents -> can't rm 'file'
                        rm_parent = false;
                    }
                } else {
                    try {
                        rm_parent = f.delete() && rm_parent;
                    } catch( final Exception e ) {
                        e.printStackTrace();
                        rm_parent = false;
                    }
                }
            }
        }
        if( rm_parent ) {
            try {
                return file.delete();
            } catch( final Exception e ) { e.printStackTrace(); }
        }
        return false;
    }

    public static final boolean rmKeyFolder() {
        BTFactory.initDirectBTLibrary();
        boolean res = true;
        {
            final File file = new File(DBTConstants.CLIENT_KEY_PATH);
            try {
                if( file.isDirectory() ) {
                    final boolean res2 = delete(file, false /* recursive */);
                    res = res2 && res;
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': delete: "+res2);
                }
            } catch(final Throwable t) {
                PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': Caught "+t.getMessage());
                res = false;
            }
        }
        if( res ) {
            final File file = new File(DBTConstants.SERVER_KEY_PATH);
            try {
                if( file.isDirectory() ) {
                    final boolean res2 = delete(file, false /* recursive */);
                    res = res2 && res;
                    PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': delete: "+res2);
                }
            } catch(final Throwable t) {
                PrintUtil.println(System.err, "****** KEY_PATH '"+file.toString()+"': Caught "+t.getMessage());
                res = false;
            }
        }
        return res;
    }
}
