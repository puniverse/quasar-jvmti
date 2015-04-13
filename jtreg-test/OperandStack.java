 /*
 * @test
 * @run main/native OperandStack
 */

public class OperandStack {
    public static class E extends Exception {}

    public static E e = new E();
    
    public static int foo(int i, String s, long l, float f, double d, int i2) {
        return i + i2;
    }

    public static int bar(int i, String s, long l, float f, double d) throws Exception {
        throw e;
    }

    public static void main(String[] args) {
        int i1 = 1;
        int i2 = 1;
        String s1 = "Hi";
        String s2 = "Hi";
        long l1 = 1l;
        long l2 = 1l;
        float f1 = 1.0f;
        float f2 = 1.0f;
        double d1 = 1.0d;
        double d2 = 1.0d;
        try {
            foo(i1, s1, l1, f1, d1, bar(i2, s2, l2, f2, d2));
        } catch (Exception ex) {
        }
    }
}
