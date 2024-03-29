/*************************************************************************
 *  Compilation:  javac HelloWorld.java
 *  Execution:    java HelloWorld
 *
 *  Prints "Hello, World". By tradition, this is everyone's first program.
 *
 *  % java HelloWorld
 *  Hello, World
 *
 *  These 17 lines of text are comments. They are not part of the program;
 *  they serve to remind us about its properties. The first two lines tell
 *  us what to type to compile and test the program. The next line describes
 *  the purpose of the program. The next few lines give a sample execution
 *  of the program and the resulting output. We will always include such 
 *  lines in our programs and encourage you to do the same.
 *
 *************************************************************************/

public class HelloWorld {

    public static int foo(int i1, int i2) throws Exception {
        return i1 + i2;
    }

    public static int bar(int i3) throws Exception {
        Thread.sleep(10000);
        return i3;
    }

    public static void main(String[] args) throws Exception {
/*
        System.out.println("Hello, World! Sleeping...");
        Thread.sleep(10000);
        System.out.println("...Slept! Hello, World!");
*/
      int i1 = 3;
      int i2 = 4;
      foo(i1, bar(i2));
    }
}

