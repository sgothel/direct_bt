package org.direct_bt;

/**
 * TransportType determines type of bluetooth scan.
 */
public enum TransportType {
    /**
     * interleaved scan
     */
    AUTO,
    /**
     * BR/EDR inquiry
     */
    BREDR,
    /**
     * LE scan only
     */
    LE
}
