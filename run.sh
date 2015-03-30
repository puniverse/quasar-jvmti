#!/bin/bash
PATCHED_JDK9_HOME=/Users/fabio/Src/openjdk9/build/macosx-x86_64-normal-server-release/images/jdk

cd agent
make
cd ../java
rm -rf build
mkdir -p build/classes
javac -d build/classes src/OperandStackApp.java
cd ..
$PATCHED_JDK9_HOME/bin/java -agentpath:agent/dist/Debug/CLang-MacOSX/libagent.dylib -cp java/build/classes OperandStackApp
