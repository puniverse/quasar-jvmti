 /*
 * @test
 * @library /lib/testlibrary
 * @build jdk.testlibrary.OutputAnalyzer
 * @build OperandStack
 * @build ProgramTest
 * @run main/native ProgramTest
 */

import java.io.File;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

public class ProgramTest {
    public static void main(final String... args) throws Exception {
        final String jdk = System.getProperty("test.jdk");
        final String cp = System.getProperty("test.class.path");
        final String lib = System.getProperty("test.nativepath");
        final ProcessBuilder pb =
            new ProcessBuilder (
                jdk + File.separator + "bin" + File.separator + "java",
                "-agentpath:" + lib + File.separator + getLibName("libOperandStack"),
                "-cp", cp,
                "OperandStack"
            );
        final OutputAnalyzer output = ProcessTools.executeProcess(pb);
        output.shouldHaveExitValue(0);
    }

    private static String getLibName(final String base) {
        final String os = System.getProperty("os.name");
        final String lcos = os.toLowerCase();

        // TODO check
        if (lcos.contains("win")) {
            return base + ".dll";
        } else if (lcos.contains("mac")) {
            return base + ".dylib";
        } else if (lcos.contains("nix") || lcos.contains("nux") || lcos.contains("aix") || lcos.contains("sunos") || lcos.contains("bsd")) {
            return base + ".so";
        } else {
            throw new IllegalStateException("Unsupported platform: " + os);
        }
    }
}
