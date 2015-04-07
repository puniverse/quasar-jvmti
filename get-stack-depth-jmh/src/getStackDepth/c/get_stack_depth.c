#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>
#include <jvmti.h>

//////////////////// JVMTI AGENT

static void checkJvmtiError(jvmtiEnv *jvmti, jvmtiError err, char *file, int line);
static char* getErrorName(jvmtiEnv *jvmti, jvmtiError errnum);
static void deallocate(jvmtiEnv *jvmti, void *p, char *file, int line);
static void checkForNull(void *ptr, char *file, int line);

static void enterCriticalSection(jvmtiEnv *jvmti);
static void exitCriticalSection(jvmtiEnv *jvmti);

static void JNICALL VMInit(jvmtiEnv *jvmti, JNIEnv* _jni, jthread _thread);

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
    capabilities.can_generate_exception_events = 1;
    err = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    CHECK_JVMTI_ERROR(jvmti, err);

    return 0;
}

/* Agent_OnUnload() is called last */
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
}

//////////////////// JVMTI AGENT UTILS

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

//////////////////// JNI

static jclass thread_cls = NULL;
static jmethodID thread_currentThread_mid = NULL;
static jthread t = NULL;
static jvmtiEnv *jvmti = NULL;

jint getStackDepth (JNIEnv *jni, jclass _clazz) {
    // 1) Get current thread (and cache it)
    if (thread_cls == NULL)
        thread_cls = (*jni)->FindClass(jni, "java/lang/Thread");
    if (thread_currentThread_mid == NULL)
        thread_currentThread_mid = (*jni)->GetStaticMethodID(jni, thread_cls, "currentThread", "()Ljava/lang/Thread;");
    if (t == NULL)
        t = (*jni)->CallStaticObjectMethod(jni, thread_cls, thread_currentThread_mid);
    if (jvmti == NULL)
        jvmti = gdata->jvmti;

    // 2) JVMTI GetFrameCount
    jint res = 0;
    (*jvmti)->GetFrameCount(jvmti, t, &res);
    return res;
}

static JNINativeMethod method_table[] = {
  { "getStackDepth", "()I", (void *) getStackDepth }
};

static int method_table_size = sizeof(method_table) / sizeof(method_table[0]);

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv* env;
    (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6);
    jclass clazz = (*env)->FindClass(env, "co/paralleluniverse/quasar/jvmti/GetStackDepthBench");
    if (clazz) {
      jint ret = (*env)->RegisterNatives(env, clazz, method_table, method_table_size);
      (*env)->DeleteLocalRef(env, clazz);
      return ret == 0 ? JNI_VERSION_1_6 : JNI_ERR;
    } else {
      return JNI_ERR;
    }
}
