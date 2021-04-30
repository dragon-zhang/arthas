#include <iostream>
#include <jni.h>
#include <jni_md.h>
#include <jvmti.h>
#include "arthas_VmTool.h" // under target/native/javah/

//缓存
static jclass cachedClass = NULL;

extern "C"
JNIEXPORT jclass JNICALL getClass(JNIEnv *env) {
    if (cachedClass == NULL) {
        //通过其签名找到Class的Class
        jclass theClass = env->FindClass("java/lang/Class");
        //放入缓存
        cachedClass = static_cast<jclass>(env->NewGlobalRef(theClass));
        env->DeleteLocalRef(theClass);
    }
    return cachedClass;
}

class UserData {
public:
    jlong *getTag() {
        return tag;
    }

    void setTag(jlong *tag) {
        UserData::tag = tag;
    }

    jint *getLimit() {
        return limit;
    }

    void setLimit(jint *limit) {
        UserData::limit = limit;
    }

    jint *getCount() {
        return count;
    }

    void setCount(jint *count) {
        UserData::count = count;
    }

    UserData(jlong *tag, jint *limit, jint *count) : tag(tag), limit(limit), count(count) {
    }

private:
    jlong *tag;
    jint *limit;
    jint *count;
};

extern "C"
jvmtiEnv *getJvmtiEnv(JNIEnv *env) {

    JavaVM *vm;
    env->GetJavaVM(&vm);

    jvmtiEnv *jvmti;
    vm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_2);
    return jvmti;
}

extern "C"
JNIEXPORT void JNICALL
Java_arthas_VmTool_forceGc0(JNIEnv *env, jclass thisClass) {
    jvmtiEnv *jvmti = getJvmtiEnv(env);
    jvmti->ForceGarbageCollection();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_arthas_VmTool_check0(JNIEnv *env, jclass thisClass) {
    return env->NewStringUTF("OK");
}

extern "C"
jobject createJavaInstance(JNIEnv *env, jclass javaClass) {
    //找到java类的构造方法
    jmethodID construct = env->GetMethodID(javaClass, "<init>", "()V");
    //生成java类实例
    return env->NewObject(javaClass, construct, "");
}

extern "C"
jlong getClassHashCode(JNIEnv *env, jclass javaClass) {
    //找到java类的hashCode方法
    jmethodID hashCodeMethod = env->GetMethodID(javaClass, "hashCode", "()I");
    //生成java类实例
    return env->CallLongMethod(javaClass, hashCodeMethod);
}

extern "C"
jvmtiIterationControl JNICALL
HeapObjectCallback(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data) {
    UserData *data = static_cast<UserData *>(user_data);
    jint *limit = data->getLimit();
    jint *count = data->getCount();
    if (-1 == *limit || *count < *limit) {
        *tag_ptr = *data->getTag();
    }
    *count = *count + 1;
    return JVMTI_ITERATION_CONTINUE;
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_arthas_VmTool_getInstances0(JNIEnv *env, jclass thisClass, jclass klass, jint limit) {

    //参数校验，校验失败抛出IllegalArgumentException
    if (limit < -1) {
        jclass exceptionClass = env->FindClass("java/lang/IllegalArgumentException");
        env->ThrowNew(exceptionClass, "limit cannot be less than -1");
    }

    jvmtiEnv *jvmti = getJvmtiEnv(env);

    jvmtiCapabilities capabilities = {0};
    capabilities.can_tag_objects = 1;
    jvmtiError error = jvmti->AddCapabilities(&capabilities);
    if (error) {
        printf("ERROR: JVMTI AddCapabilities failed!%u\n", error);
        return JNI_FALSE;
    }
    //这里用hashCode作为标记
    jlong tag = getClassHashCode(env, klass);
    jint count = 0;
    UserData userData(&tag, &limit, &count);
    error = jvmti->IterateOverInstancesOfClass(klass, JVMTI_HEAP_OBJECT_EITHER,
                                               HeapObjectCallback, &userData);
    if (error) {
        printf("ERROR: JVMTI IterateOverInstancesOfClass failed!%u\n", error);
        return JNI_FALSE;
    }

    count = 0;
    jobject *instances;
    error = jvmti->GetObjectsWithTags(1, &tag, &count, &instances, NULL);
    if (error) {
        printf("ERROR: JVMTI GetObjectsWithTags failed!%u\n", error);
        return JNI_FALSE;
    }

    jobjectArray array = env->NewObjectArray(count, klass, NULL);
    //添加元素到数组
    for (int i = 0; i < count; i++) {
        env->SetObjectArrayElement(array, i, instances[i]);
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(instances));
    return array;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_arthas_VmTool_sumInstanceSize0(JNIEnv *env, jclass thisClass, jclass klass) {

    jvmtiEnv *jvmti = getJvmtiEnv(env);

    jvmtiCapabilities capabilities = {0};
    capabilities.can_tag_objects = 1;
    jvmtiError error = jvmti->AddCapabilities(&capabilities);
    if (error) {
        printf("ERROR: JVMTI AddCapabilities failed!%u\n", error);
        return JNI_FALSE;
    }
    //这里用hashCode作为标记
    jlong tag = getClassHashCode(env, klass);
    jint count = 0;
    jint limit = -1;
    UserData userData(&tag, &limit, &count);
    error = jvmti->IterateOverInstancesOfClass(klass, JVMTI_HEAP_OBJECT_EITHER,
                                               HeapObjectCallback, &userData);
    if (error) {
        printf("ERROR: JVMTI IterateOverInstancesOfClass failed!%u\n", error);
        return JNI_FALSE;
    }

    count = 0;
    jobject *instances;
    error = jvmti->GetObjectsWithTags(1, &tag, &count, &instances, NULL);
    if (error) {
        printf("ERROR: JVMTI GetObjectsWithTags failed!%u\n", error);
        return JNI_FALSE;
    }

    jlong sum = 0;
    for (int i = 0; i < count; i++) {
        jlong size = 0;
        jvmti->GetObjectSize(instances[i], &size);
        sum = sum + size;
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(instances));
    return sum;
}

extern "C"
JNIEXPORT jlong JNICALL Java_arthas_VmTool_getInstanceSize0
        (JNIEnv *env, jclass thisClass, jobject instance) {

    jvmtiEnv *jvmti = getJvmtiEnv(env);

    jlong size = -1;
    jvmtiError error = jvmti->GetObjectSize(instance, &size);
    if (error) {
        printf("ERROR: JVMTI GetObjectSize failed!%u\n", error);
        return JNI_FALSE;
    }
    return size;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_arthas_VmTool_countInstances0(JNIEnv *env, jclass thisClass, jclass klass) {

    jvmtiEnv *jvmti = getJvmtiEnv(env);

    jvmtiCapabilities capabilities = {0};
    capabilities.can_tag_objects = 1;
    jvmtiError error = jvmti->AddCapabilities(&capabilities);
    if (error) {
        printf("ERROR: JVMTI AddCapabilities failed!%u\n", error);
        return JNI_FALSE;
    }
    //这里用hashCode作为标记
    jlong tag = getClassHashCode(env, klass);
    jint count = 0;
    jint limit = -1;
    UserData userData(&tag, &limit, &count);
    error = jvmti->IterateOverInstancesOfClass(klass, JVMTI_HEAP_OBJECT_EITHER,
                                               HeapObjectCallback, &userData);
    if (error) {
        printf("ERROR: JVMTI IterateOverInstancesOfClass failed!%u\n", error);
        return JNI_FALSE;
    }

    count = 0;
    error = jvmti->GetObjectsWithTags(1, &tag, &count, NULL, NULL);
    if (error) {
        printf("ERROR: JVMTI GetObjectsWithTags failed!%u\n", error);
        return JNI_FALSE;
    }
    return count;
}

extern "C"
JNIEXPORT jobjectArray JNICALL Java_arthas_VmTool_getAllLoadedClasses0
        (JNIEnv *env, jclass thisClass) {

    jvmtiEnv *jvmti = getJvmtiEnv(env);

    jclass *classes;
    jint count = 0;

    jvmtiError error = jvmti->GetLoadedClasses(&count, &classes);
    if (error) {
        printf("ERROR: JVMTI GetLoadedClasses failed!\n");
        return JNI_FALSE;
    }

    jobjectArray array = env->NewObjectArray(count, getClass(env), NULL);
    //添加元素到数组
    for (int i = 0; i < count; i++) {
        env->SetObjectArrayElement(array, i, classes[i]);
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(classes));
    return array;
}