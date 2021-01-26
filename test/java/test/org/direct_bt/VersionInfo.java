package test.org.direct_bt;

import java.io.IOException;

import org.direct_bt.DirectBTVersion;
import org.jau.util.VersionUtil;

public class VersionInfo {
    public static void main(final String args[]) throws IOException {
        System.err.println(VersionUtil.getPlatformInfo());
        System.err.println("Version Info:");
        System.err.println(DirectBTVersion.getInstance());
        System.err.println("");
        System.err.println("Full Manifest:");
        System.err.println(DirectBTVersion.getInstance().getFullManifestInfo(null));
    }

}
