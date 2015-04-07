package co.paralleluniverse.quasar.jvmti;

import java.util.concurrent.TimeUnit;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.infra.Blackhole;

/**
 *
 * @author circlespainter
 */
@State(Scope.Benchmark)
public class GetStackDepthBench {

    public static native int getStackDepth();
    
    @Setup
    public void init() {
        System.loadLibrary("getStackDepth");
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    @OutputTimeUnit(TimeUnit.NANOSECONDS)
    public void bench(final Blackhole bh) {
        bh.consume(getStackDepth());
    }
}
