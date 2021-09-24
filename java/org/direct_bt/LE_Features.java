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

/**
 * LE Link Layer Feature Set (bitmask)
 * <pre>
 * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
 *
 * BT Core Spec v5.2: Vol 4, Part E, 7.8.3 LE Read Local Supported Features command
 *
 * BT Core Spec v5.2: Vol 4, Part E, 7.8.21 LE Read Remote Features command
 * BT Core Spec v5.2: Vol 4, Part E, 7.7.65.4 LE Read Remote Features Complete event
 *
 * BT Core Spec v5.2: Vol 6, Part B, 7.8.115 LE Set Host Feature Command
 * </pre>
 *
 * @since 2.4.0
 */
public class LE_Features {

    /**
     * Each enum represents a 'LE Link Layer Feature' bit value.
     *
     * @since 2.4.0
     */
    public enum Feature {
        LE_Encryption(0),
        Conn_Param_Req_Proc(1),
        Ext_Rej_Ind(2),
        SlaveInit_Feat_Exchg(3),
        LE_Ping(4),
        LE_Data_Pkt_Len_Ext(5),
        LL_Privacy(6),
        Ext_Scan_Filter_Pol(7),
        LE_2M_PHY(8),
        Stable_Mod_Idx_Tx(9),
        Stable_Mod_Idx_Rx(10),
        LE_Coded_PHY(11),
        LE_Ext_Adv(12),
        LE_Per_Adv(13),
        Chan_Sel_Algo_2(14),
        LE_Pwr_Cls_1(15),
        Min_Num_Used_Chan_Proc(16),
        Conn_CTE_Req(17),
        Conn_CTE_Res(18),
        ConnLess_CTE_Tx(19),
        ConnLess_CTE_Rx(20),
        AoD(21),
        AoA(22),
        Rx_Const_Tone_Ext(23),
        Per_Adv_Sync_Tx_Sender(24),
        Per_Adv_Sync_Tx_Rec(25),
        Zzz_Clk_Acc_Upd(26),
        Rem_Pub_Key_Val(27),
        Conn_Iso_Stream_Master(28),
        Conn_Iso_Stream_Slave(29),
        Iso_Brdcst(30),
        Sync_Rx(31),
        Iso_Chan(32),
        LE_Pwr_Ctrl_Req(33),
        LE_Pwr_Chg_Ind(34),
        LE_Path_Loss_Mon(35);

        Feature(final int v) {
            value = 1L << v;
        }
        public final long value;
    }

    public long mask;


    public LE_Features(final long v) {
        mask = v;
    }

    public boolean isSet(final Feature bit) { return 0 != ( mask & bit.value ); }
    public void set(final Feature bit) { mask = mask | bit.value; }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        for (final Feature f : Feature.values()) {
            if( isSet(f) ) {
                if( 0 < count ) { out.append(", "); }
                out.append(f.name()); count++;
            }
        }
        if( 1 < count ) {
            out.insert(0, "[");
            out.append("]");
        }
        return out.toString();
    }
}
