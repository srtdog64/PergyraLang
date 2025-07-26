/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PERGYRA_JNI_BRIDGE_H
#define PERGYRA_JNI_BRIDGE_H

#include <jni.h>
#include "../runtime/slot_manager.h"

/*
 * JNI class and method ID cache structure
 */
typedef struct
{
    jclass    slot_handle_class;
    jmethodID slot_handle_constructor;
    jfieldID  slot_id_field;
    jfieldID  type_tag_field;
    jfieldID  generation_field;
    
    jclass    slot_manager_class;
    jmethodID error_callback;
    
    jclass    string_class;
    jclass    integer_class;
    jclass    long_class;
    jclass    float_class;
    jclass    double_class;
    jclass    boolean_class;
} JNICache;

/*
 * JVM interface structure
 */
typedef struct
{
    JavaVM       *jvm;
    JNIEnv       *env;
    JNICache      cache;
    SlotManager  *slot_manager;
    bool          initialized;
} PergyraJVMBridge;

/*
 * JNI lifecycle functions
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved);

/*
 * JNI bridge functions for slot manager operations
 */
JNIEXPORT jlong JNICALL 
Java_com_pergyra_SlotManager_createManager(JNIEnv *env, jclass clazz, 
                                           jint max_slots, jlong memory_pool_size);

JNIEXPORT void JNICALL 
Java_com_pergyra_SlotManager_destroyManager(JNIEnv *env, jclass clazz, 
                                            jlong manager_ptr);

JNIEXPORT jobject JNICALL 
Java_com_pergyra_SlotManager_claimSlot(JNIEnv *env, jclass clazz, 
                                       jlong manager_ptr, jint type_tag);

JNIEXPORT jint JNICALL 
Java_com_pergyra_SlotManager_writeSlot(JNIEnv *env, jclass clazz, 
                                       jlong manager_ptr, jobject handle, 
                                       jobject data);

JNIEXPORT jobject JNICALL 
Java_com_pergyra_SlotManager_readSlot(JNIEnv *env, jclass clazz, 
                                      jlong manager_ptr, jobject handle, 
                                      jint type_tag);

JNIEXPORT jint JNICALL 
Java_com_pergyra_SlotManager_releaseSlot(JNIEnv *env, jclass clazz, 
                                         jlong manager_ptr, jobject handle);

JNIEXPORT jobject JNICALL 
Java_com_pergyra_SlotManager_getStats(JNIEnv *env, jclass clazz, 
                                      jlong manager_ptr);

/*
 * Utility functions for Java-C data conversion
 */
SlotHandle jni_to_slot_handle(JNIEnv *env, jobject java_handle, 
                             const JNICache *cache);
jobject    slot_handle_to_jni(JNIEnv *env, const SlotHandle *handle, 
                             const JNICache *cache);
jobject    create_java_value(JNIEnv *env, const void *data, TypeTag type, 
                            const JNICache *cache);
bool       extract_java_value(JNIEnv *env, jobject java_obj, void *buffer, 
                             size_t buffer_size, TypeTag type);

/*
 * Error handling functions
 */
void throw_pergyra_exception(JNIEnv *env, SlotError error, const char *message);
jint slot_error_to_jni(SlotError error);

/*
 * Cache management functions
 */
bool init_jni_cache(JNIEnv *env, JNICache *cache);
void cleanup_jni_cache(JNIEnv *env, JNICache *cache);

#endif /* PERGYRA_JNI_BRIDGE_H */