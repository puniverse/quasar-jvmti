

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <jvmti.h>

static void checkJvmtiError(jvmtiEnv *jvmti, jvmtiError err, char *file, int line);
static char* getErrorName(jvmtiEnv *jvmti, jvmtiError errnum);
static void deallocate(jvmtiEnv *jvmti, void *p, char *file, int line);
static void checkForNull(void *ptr, char *file, int line);

static void JNICALL VMStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env);
static void JNICALL VMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);
static void JNICALL VMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env);
static void JNICALL dumpThreadInfo(jvmtiEnv* jvmti);

/* Check for NULL pointer error */
#define CHECK_FOR_NULL(ptr) checkForNull(ptr, __FILE__, __LINE__)
/* Check for JVMTI error */
#define CHECK_JVMTI_ERROR(jvmti, err) checkJvmtiError(jvmti, err, __FILE__, __LINE__)

#define DEALLOC(jvmti, ptr) deallocate(jvmti, ptr, __FILE__, __LINE__)

/* Agent_OnLoad() is called first, we prepare for a VM_INIT event here. */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    jint rc;
    jvmtiError err;
    jvmtiCapabilities capabilities;
    jvmtiEventCallbacks callbacks;
    jvmtiEnv *jvmti;

    /* Get JVMTI environment */
    jvmti = NULL;
    rc = (*vm)->GetEnv(vm, (void **) &jvmti, JVMTI_VERSION);
    if (rc != JNI_OK) {
        printf("ERROR: Unable to create jvmtiEnv, GetEnv failed, error=%d\n", rc);
        return -1;
    }

    //
    // Set the "capabilities"
    //
    (void) memset(&capabilities, 0, sizeof (jvmtiCapabilities));
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_access_local_variables = 1;
    err = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    CHECK_JVMTI_ERROR(jvmti, err);


    // Set callbacks and enable event notifications
    memset(&callbacks, 0, sizeof (callbacks));
    callbacks.VMInit = VMInit;
    callbacks.VMStart = VMStart;
    callbacks.VMDeath = VMDeath;
    callbacks.DataDumpRequest = &dumpThreadInfo;
    err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof (callbacks));
    CHECK_JVMTI_ERROR(jvmti, err);

    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_START, NULL);
    CHECK_JVMTI_ERROR(jvmti, err);
    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    CHECK_JVMTI_ERROR(jvmti, err);
    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    CHECK_JVMTI_ERROR(jvmti, err);
    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, NULL);
    CHECK_JVMTI_ERROR(jvmti, err);

    printf("\n\nDemo JVMTI agent loaded and initialized\n\n");

    return 0;
}

static void JNICALL VMStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env) {
    printf("VMStart\n");

}

static void JNICALL VMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread) {
    printf("VMInit\n");
}

static void JNICALL VMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env) {
    printf("VMDeath\n");
}

/* Agent_OnUnload() is called last */
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {
}

/*
Check the state of the Java Thread
 */
static unsigned char* JNICALL getThreadState(jvmtiEnv* jvmti, jthread thread, jint state) {
    jvmtiError err;

    err = (*jvmti)->GetThreadState(jvmti, thread, &state);
    CHECK_JVMTI_ERROR(jvmti, err);

    //
    // Return state of thread
    //
    switch (state & JVMTI_JAVA_LANG_THREAD_STATE_MASK) {
        case JVMTI_JAVA_LANG_THREAD_STATE_NEW: return "New";
        case JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED: return "Terminated";
        case JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE: return "Runnable";
        case JVMTI_JAVA_LANG_THREAD_STATE_BLOCKED: return "Blocked";
        case JVMTI_JAVA_LANG_THREAD_STATE_WAITING: return "Waiting";
        case JVMTI_JAVA_LANG_THREAD_STATE_TIMED_WAITING: return "Timed Waiting";
    }
    return "UNKNOWN";
}

static void JNICALL dumpThreadInfo(jvmtiEnv* jvmti) {
    jvmtiError err;
    jint totalThreadCount = 0;
    jint threadState = 0;
    int i = 0, j = 0;
    jthread* threadPtr;
    jvmtiThreadInfo threadInfo;

    err = (*jvmti)->GetAllThreads(jvmti, &totalThreadCount, &threadPtr);
    CHECK_JVMTI_ERROR(jvmti, err);

    //
    // Examine the "live" threads
    //

    for (i = 0; i < totalThreadCount; i++) {
        jvmtiFrameInfo frames[10]; // Allocate space for 10 stack frames 
        jint recordCount = 0; // Actual stack records

        /* make sure the stack variables are garbage free */
        (void) memset(&threadInfo, 0, sizeof (threadInfo));

        //
        // Get / Display the Thread's "state" information
        //
        err = (*jvmti)->GetThreadInfo(jvmti, threadPtr[i], &threadInfo);
        printf("\n\n\"%s\", prio=%d, daemon=%s, state=%s\n", \
  threadInfo.name, threadInfo.priority, (threadInfo.is_daemon == 1 ? "yes" : "no"), getThreadState(jvmti, threadPtr[i], threadState));

        /*
        jobject o;
        err = (*jvmti)->GetOperandObject(jvmti, *threadPtr, 1, 1, &o);
        CHECK_JVMTI_ERROR(jvmti, err);
        printf(" o: %p\n", o);
        jdouble d;
        err = (*jvmti)->GetOperandDouble(jvmti, *threadPtr, 1, 1, &d);
        CHECK_JVMTI_ERROR(jvmti, err);
        printf(" d: %f\n", d);
        jfloat f;
        err = (*jvmti)->GetOperandFloat(jvmti, *threadPtr, 1, 1, &f);
        CHECK_JVMTI_ERROR(jvmti, err);
        printf(" f: %f\n", f);
        jlong l;
        err = (*jvmti)->GetOperandLong(jvmti, *threadPtr, 1, 1, &l);
        CHECK_JVMTI_ERROR(jvmti, err);
        printf(" l: %d\n", l);
        */
        //
        // Get / Display the Thread's "stack trace" information
        //
        err = (*jvmti)->GetStackTrace(jvmti, threadPtr[i], 0, 10, frames, &recordCount);
        CHECK_JVMTI_ERROR(jvmti, err);

        printf(" Stack Trace Depth: %d\n", recordCount);
        printf(" ---------------------\n");
        for (j = 0; j < recordCount; j++) {
            char* className = "";
            char* methodName = "";
            char* methodSig = "";
            char* sourceFileName = "";
            int k = 0;
            jlocation methodStartAddr;
            jlocation methodEndAddr;
            jclass classPtr;
            jboolean isNative;
            jvmtiLineNumberEntry* lineNumTbl;
            jint lineNumTblSize;
            
            err = (*jvmti)->GetMethodName(jvmti, frames[j].method, &methodName, &methodSig, NULL);
            CHECK_JVMTI_ERROR(jvmti, err);

            err = (*jvmti)->GetMethodDeclaringClass(jvmti, frames[j].method, &classPtr);
            CHECK_JVMTI_ERROR(jvmti, err);
            err = (*jvmti)->GetClassSignature(jvmti, classPtr, &className, NULL);
            CHECK_JVMTI_ERROR(jvmti, err);
            err = (*jvmti)->GetSourceFileName(jvmti, classPtr, &sourceFileName);
            CHECK_JVMTI_ERROR(jvmti, err);

            //
            // Check for the "native"ness of the method while iterating through the stack
            //
            err = (*jvmti)->IsMethodNative(jvmti, frames[j].method, &isNative);
            CHECK_JVMTI_ERROR(jvmti, err);

            if (!isNative) {
                err = (*jvmti)->GetMethodLocation(jvmti, frames[j].method, &methodStartAddr, &methodEndAddr); // if method is in native-mode, memory access error
                CHECK_JVMTI_ERROR(jvmti, err);
                err = (*jvmti)->GetLineNumberTable(jvmti, frames[j].method, &lineNumTblSize, &lineNumTbl);
                CHECK_JVMTI_ERROR(jvmti, err);
                printf("\n  at %s in class %s (%s:%d-%d)", methodName, className, sourceFileName, lineNumTbl[0].line_number, lineNumTbl[lineNumTblSize - 1].line_number);
            }

            if (isNative) {
                printf("\n  at %s in class %s (%s:Native)", methodName, className, sourceFileName);
            }

            if (!strcmp("main", threadInfo.name) && !strcmp("bar", methodName)) {
                jint i;
                err = (*jvmti)->GetOperandInt(jvmti, threadPtr[i], j, 0, &i);
                CHECK_JVMTI_ERROR(jvmti, err);
                printf(" i: %d\n", i);
            }

            // Release memory
            DEALLOC(jvmti, methodName);
            DEALLOC(jvmti, methodSig);
            DEALLOC(jvmti, className);
            DEALLOC(jvmti, sourceFileName);
        }
        // Release memory
        DEALLOC(jvmti, threadInfo.name);
    }

    printf("\n\n ------------------------ THREAD DUMP COMPLETE -------------------------------------- \n\n");
}

////////////////////

static void checkJvmtiError(jvmtiEnv *jvmti, jvmtiError err, char *file, int line) {
    if (err != JVMTI_ERROR_NONE) {
        char *name;

        name = getErrorName(jvmti, err);
        printf("ERROR: JVMTI error err=%d(%s) in %s:%d\n", err, name, file, line);
        (*jvmti)->Deallocate(jvmti, (unsigned char *) name);
        abort();
    }
}

/* Get name for JVMTI error code */
static char* getErrorName(jvmtiEnv *jvmti, jvmtiError errnum) {
    char *name;
    jvmtiError err = (*jvmti)->GetErrorName(jvmti, errnum, &name);
    if (err != JVMTI_ERROR_NONE) {
        printf("ERROR: JVMTI GetErrorName error err=%d\n", err);
        abort();
    }
    return name;
}

static void checkForNull(void *ptr, char *file, int line) {
    if (ptr == NULL) {
        printf("ERROR: NULL pointer error in %s:%d\n", file, line);
        abort();
    }
}

/* Deallocate JVMTI memory */
static void deallocate(jvmtiEnv *jvmti, void *p, char *file, int line) {
    jvmtiError err = (*jvmti)->Deallocate(jvmti, (unsigned char *) p);
    if (err != JVMTI_ERROR_NONE) {
        printf("ERROR: JVMTI Deallocate error err=%d in %s:%d\n", err, file, line);
        abort();
    }
}
