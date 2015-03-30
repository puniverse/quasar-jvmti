#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <jvmti.h>

static void checkJvmtiError(jvmtiEnv *jvmti, jvmtiError err, char *file, int line);
static char* getErrorName(jvmtiEnv *jvmti, jvmtiError errnum);
static void deallocate(jvmtiEnv *jvmti, void *p, char *file, int line);
static void checkForNull(void *ptr, char *file, int line);

static void enterCriticalSection(jvmtiEnv *jvmti);
static void exitCriticalSection(jvmtiEnv *jvmti);

static void getClassName(JNIEnv* jni, jobject obj, jstring* str, char** name);

static void JNICALL VMInit(jvmtiEnv *jvmti, JNIEnv* _jni, jthread _thread);
static void JNICALL Exception(jvmtiEnv *jvmti, JNIEnv* _jni, jthread thread, jmethodID _m1, jlocation _l1, jobject _o, jmethodID _m2, jlocation _l2);

/* Global agent data structure */
typedef struct {
  /* JVMTI Environment */
  jvmtiEnv *jvmti;
  /* Data access Lock */
  jrawMonitorID lock;
} GlobalAgentData;
static GlobalAgentData *gdata;

/* Check for NULL pointer error */
#define CHECK_FOR_NULL(ptr) checkForNull(ptr, __FILE__, __LINE__)
/* Check for JVMTI error */
#define CHECK_JVMTI_ERROR(jvmti, err) checkJvmtiError(jvmti, err, __FILE__, __LINE__)

#define DEALLOC(jvmti, ptr) deallocate(jvmti, ptr, __FILE__, __LINE__)

/* Agent_OnLoad() is called first, we prepare for a VM_INIT event here. */
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
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

    static GlobalAgentData data;
    (void)memset((void*)&data, 0, sizeof(data));
    gdata = &data;
    /* Here we save the jvmtiEnv* for Agent_OnUnload(). */
    gdata->jvmti = jvmti;
    err = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));

    // Set the "capabilities"
    (void) memset(&capabilities, 0, sizeof (jvmtiCapabilities));
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_access_local_variables = 1;
    capabilities.can_generate_exception_events = 1;
    err = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    CHECK_JVMTI_ERROR(jvmti, err);


    // Set callbacks and enable event notifications
    memset(&callbacks, 0, sizeof (callbacks));
    callbacks.VMInit = VMInit;
    callbacks.Exception = Exception;
    err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof (callbacks));
    CHECK_JVMTI_ERROR(jvmti, err);

    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    CHECK_JVMTI_ERROR(jvmti, err);

    return 0;
}

static void JNICALL VMInit(jvmtiEnv *jvmti, JNIEnv* _jni, jthread _thread) {
    enterCriticalSection(jvmti); {
        jvmtiError err;
        err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, (jthread)NULL);
    } exitCriticalSection(jvmti);
}

static void JNICALL Exception(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID _m1, jlocation _l1, jobject exceptionObj, jmethodID _m2, jlocation _l2) {
    enterCriticalSection(jvmti); {
        jvmtiError err;
        jvmtiThreadInfo threadInfo;
        jvmtiFrameInfo frames[10]; // Allocate space for 10 stack frames
        jint recordCount = 0;      // Actual stack records
        int k = 0;

        err = (*jvmti)->GetStackTrace(jvmti, thread, 0, 10, frames, &recordCount);
        CHECK_JVMTI_ERROR(jvmti, err);

        err = (*jvmti)->GetThreadInfo(jvmti, thread, &threadInfo);
        CHECK_JVMTI_ERROR(jvmti, err);

        for (k = 0; k < recordCount; k++) {
            jclass classPtr;
            jstring exceptionClassStringObj;
            char* sourceFileName = "";
            char* className = "";
            char* methodName = "";
            char* methodSig = "";
            char* exceptionClassName = "";

            err = (*jvmti)->GetMethodName(jvmti, frames[k].method, &methodName, &methodSig, NULL);
            CHECK_JVMTI_ERROR(jvmti, err);
            err = (*jvmti)->GetMethodDeclaringClass(jvmti, frames[k].method, &classPtr);
            CHECK_JVMTI_ERROR(jvmti, err);
            err = (*jvmti)->GetClassSignature(jvmti, classPtr, &className, NULL);
            CHECK_JVMTI_ERROR(jvmti, err);
            err = (*jvmti)->GetSourceFileName(jvmti, classPtr, &sourceFileName);
            CHECK_JVMTI_ERROR(jvmti, err);

            getClassName(jni, exceptionObj, &exceptionClassStringObj, &exceptionClassName);
            
            // printf("THREAD: '%s', SOURCE: '%s', CLASS: '%s', METHOD: '%s%s', EXCEPTION CLASS:'%s'\n", threadInfo.name, sourceFileName, className, methodName, methodSig, exceptionClassName);

            if (!strcmp("main", threadInfo.name) && !strcmp("main", methodName)) {
                jint i;
                err = (*jvmti)->GetOperandInt(jvmti, thread, k, 0, &i);
                CHECK_JVMTI_ERROR(jvmti, err);
                if (i != 1) {
                    printf("ERROR: int @ OpStack[0] of '%s' is '%d' != 1\n", methodName, i);
                    abort();
                }

                jobject o;
                err = (*jvmti)->GetOperandObject(jvmti, thread, k, 1, &o);
                CHECK_JVMTI_ERROR(jvmti, err);

                jlong l;
                err = (*jvmti)->GetOperandLong(jvmti, thread, k, 2, &l);
                CHECK_JVMTI_ERROR(jvmti, err);
                if (l != 1l) {
                    printf("ERROR: long @ OpStack[2] of '%s' is '%ld' != 1l\n", methodName, l);
                    abort();
                }

                jfloat f;
                err = (*jvmti)->GetOperandFloat(jvmti, thread, k, 4, &f);
                CHECK_JVMTI_ERROR(jvmti, err);
                if (f != 1.0f) {
                    printf("ERROR: float @ OpStack[4] of '%s' is '%f' != 1.0f\n", methodName, f);
                    abort();
                }

                jdouble d;
                err = (*jvmti)->GetOperandDouble(jvmti, thread, k, 5, &d);
                CHECK_JVMTI_ERROR(jvmti, err);
                if (d != 1.0) {
                    printf("ERROR: float @ OpStack[5] of '%s' is '%lf' != 1.0d\n", methodName, d);
                    abort();
                }
            }

            // Release memory
            DEALLOC(jvmti, methodName);
            DEALLOC(jvmti, methodSig);
            DEALLOC(jvmti, className);
            DEALLOC(jvmti, sourceFileName);
            (*jni)->ReleaseStringUTFChars(jni, exceptionClassStringObj, exceptionClassName);
        }

        DEALLOC(jvmti, threadInfo.name);
    } exitCriticalSection(jvmti);
}

/* Agent_OnUnload() is called last */
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
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

/* Enter a critical section by doing a JVMTI Raw Monitor Enter */
static void enterCriticalSection(jvmtiEnv *jvmti) {
    jvmtiError err;
    err = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock);
    CHECK_JVMTI_ERROR(jvmti, err);
}

/* Exit a critical section by doing a JVMTI Raw Monitor Exit */
static void exitCriticalSection(jvmtiEnv *jvmti) {
    jvmtiError err;
    err = (*jvmti)->RawMonitorExit(jvmti, gdata->lock);
    CHECK_JVMTI_ERROR(jvmti, err);
}

static void getClassName(JNIEnv* jni, jobject obj, jstring* str, char** name) {
    jclass cls = (*jni)->GetObjectClass(jni, obj);

    // First get the class object
    jmethodID mid = (*jni)->GetMethodID(jni, cls, "getClass", "()Ljava/lang/Class;");
    jobject clsObj = (*jni)->CallObjectMethod(jni, obj, mid);

    // Now get the class object's class descriptor
    cls = (*jni)->GetObjectClass(jni, clsObj);

    // Find the getName() method on the class object
    mid = (*jni)->GetMethodID(jni, cls, "getName", "()Ljava/lang/String;");

    // Call the getName() to get a jstring object back
    *str = (jstring)(*jni)->CallObjectMethod(jni, clsObj, mid);

    // Now get the c string from the java jstring object
    *name = (*jni)->GetStringUTFChars(jni, *str, NULL);
}