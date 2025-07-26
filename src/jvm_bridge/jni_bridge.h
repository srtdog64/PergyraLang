#ifndef PERGYRA_JNI_BRIDGE_H
#define PERGYRA_JNI_BRIDGE_H

#include <jni.h>
#include "../runtime/slot_manager.h"

// JNI 클래스 및 메서드 ID 캐시
typedef struct {
    jclass slot_handle_class;
    jmethodID slot_handle_constructor;
    jfieldID slot_id_field;
    jfieldID type_tag_field;
    jfieldID generation_field;
    
    jclass slot_manager_class;
    jmethodID error_callback;
    
    jclass string_class;
    jclass integer_class;
    jclass long_class;
    jclass float_class;
    jclass double_class;
    jclass boolean_class;
} JNICache;

// JVM 인터페이스 구조체
typedef struct {
    JavaVM* jvm;
    JNIEnv* env;
    JNICache cache;
    SlotManager* slot_manager;
    bool initialized;
} PergyraJVMBridge;

// 초기화 및 해제
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// JNI 브릿지 함수들
JNIEXPORT jlong JNICALL Java_com_pergyra_SlotManager_createManager(
    JNIEnv* env, jclass clazz, jint max_slots, jlong memory_pool_size);

JNIEXPORT void JNICALL Java_com_pergyra_SlotManager_destroyManager(
    JNIEnv* env, jclass clazz, jlong manager_ptr);

JNIEXPORT jobject JNICALL Java_com_pergyra_SlotManager_claimSlot(
    JNIEnv* env, jclass clazz, jlong manager_ptr, jint type_tag);

JNIEXPORT jint JNICALL Java_com_pergyra_SlotManager_writeSlot(
    JNIEnv* env, jclass clazz, jlong manager_ptr, jobject handle, jobject data);

JNIEXPORT jobject JNICALL Java_com_pergyra_SlotManager_readSlot(
    JNIEnv* env, jclass clazz, jlong manager_ptr, jobject handle, jint type_tag);

JNIEXPORT jint JNICALL Java_com_pergyra_SlotManager_releaseSlot(
    JNIEnv* env, jclass clazz, jlong manager_ptr, jobject handle);

JNIEXPORT jobject JNICALL Java_com_pergyra_SlotManager_getStats(
    JNIEnv* env, jclass clazz, jlong manager_ptr);

// 유틸리티 함수들
SlotHandle jni_to_slot_handle(JNIEnv* env, jobject java_handle, const JNICache* cache);
jobject slot_handle_to_jni(JNIEnv* env, const SlotHandle* handle, const JNICache* cache);
jobject create_java_value(JNIEnv* env, const void* data, TypeTag type, const JNICache* cache);
bool extract_java_value(JNIEnv* env, jobject java_obj, void* buffer, size_t buffer_size, TypeTag type);

// 에러 처리
void throw_pergyra_exception(JNIEnv* env, SlotError error, const char* message);
jint slot_error_to_jni(SlotError error);

// 초기화 함수
bool init_jni_cache(JNIEnv* env, JNICache* cache);
void cleanup_jni_cache(JNIEnv* env, JNICache* cache);

#endif // PERGYRA_JNI_BRIDGE_H