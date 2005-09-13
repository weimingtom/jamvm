/*
 * Copyright (C) 2003, 2004, 2005 Robert Lougher <rob@lougher.demon.co.uk>.
 *
 * This file is part of JamVM.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef NO_JNI
#error to use classpath, Jam must be compiled with JNI!
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "jam.h"
#include "thread.h"
#include "lock.h"
#include "natives.h"

static int pd_offset;

void initialiseNatives() {
    FieldBlock *pd = findField(java_lang_Class, "pd", "Ljava/security/ProtectionDomain;");

    if(pd == NULL) {
        fprintf(stderr, "Error initialising VM (initialiseNatives)\n");
        exitVM(1);
    }
    pd_offset = pd->offset;
}

/* java.lang.VMObject */

uintptr_t *getClass(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *ob = (Object*)*ostack;
    *ostack++ = (uintptr_t)ob->class;
    return ostack;
}

uintptr_t *jamClone(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *ob = (Object*)*ostack;
    *ostack++ = (uintptr_t)cloneObject(ob);
    return ostack;
}

/* static method wait(Ljava/lang/Object;JI)V */
uintptr_t *jamWait(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *obj = (Object *)ostack[0];
    long long ms = *((long long *)&ostack[1]);
    int ns = ostack[3];

    objectWait(obj, ms, ns);
    return ostack;
}

/* static method notify(Ljava/lang/Object;)V */
uintptr_t *notify(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *obj = (Object *)*ostack;
    objectNotify(obj);
    return ostack;
}

/* static method notifyAll(Ljava/lang/Object;)V */
uintptr_t *notifyAll(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *obj = (Object *)*ostack;
    objectNotifyAll(obj);
    return ostack;
}

/* java.lang.VMSystem */

/* arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V */
uintptr_t *arraycopy(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *src = (Object *)ostack[0];
    int start1 = ostack[1];
    Object *dest = (Object *)ostack[2];
    int start2 = ostack[3];
    int length = ostack[4];

    if((src == NULL) || (dest == NULL))
        signalException("java/lang/NullPointerException", NULL);
    else {
        ClassBlock *scb = CLASS_CB(src->class);
        ClassBlock *dcb = CLASS_CB(dest->class);
        char *sdata = ARRAY_DATA(src);            
        char *ddata = ARRAY_DATA(dest);            

        if((scb->name[0] != '[') || (dcb->name[0] != '['))
            goto storeExcep; 

        if((start1 < 0) || (start2 < 0) || (length < 0)
                        || ((start1 + length) > ARRAY_LEN(src))
                        || ((start2 + length) > ARRAY_LEN(dest))) {
            signalException("java/lang/ArrayIndexOutOfBoundsException", NULL);
            return ostack;
        }

        if(isInstanceOf(dest->class, src->class)) {
            int size;

            switch(scb->name[1]) {
                case 'B':
                case 'Z':
                    size = 1;
                    break;
                case 'C':
                case 'S':
                    size = 2;
                    break;
                case 'I':
                case 'F':
                    size = 4;
                    break;
                case 'L':
                case '[':
                    size = sizeof(Object*);
                    break;
                case 'J':
                case 'D':
                    size = 8;
                    break;
            } 

            memmove(ddata + start2*size, sdata + start1*size, length*size);
        } else {
            Object **sob, **dob;
            int i;

            if(!(((scb->name[1] == 'L') || (scb->name[1] == '[')) &&
                          ((dcb->name[1] == 'L') || (dcb->name[1] == '['))))
                goto storeExcep; 

            /* Not compatible array types, but elements may be compatible...
               e.g. src = [Ljava/lang/Object, dest = [Ljava/lang/String, but
               all src = Strings - check one by one...
             */
            
            if(scb->dim > dcb->dim)
                goto storeExcep;

            sob = &((Object**)sdata)[start1];
            dob = &((Object**)ddata)[start2];

            for(i = 0; i < length; i++) {
                if((*sob != NULL) && !arrayStoreCheck(dest->class, (*sob)->class))
                    goto storeExcep;
                *dob++ = *sob++;
            }
        }
    }
    return ostack;

storeExcep:
    signalException("java/lang/ArrayStoreException", NULL);
    return ostack;
}

uintptr_t *identityHashCode(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    return ++ostack;
}

/* java.lang.VMRuntime */

uintptr_t *freeMemory(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *(u8*)ostack = freeHeapMem();
    return ostack + 2;
}

uintptr_t *totalMemory(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *(u8*)ostack = totalHeapMem();
    return ostack + 2;
}

uintptr_t *maxMemory(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *(u8*)ostack = maxHeapMem();
    return ostack + 2;
}

uintptr_t *gc(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    gc1();
    return ostack;
}

uintptr_t *runFinalization(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    runFinalizers();
    return ostack;
}

uintptr_t *exitInternal(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    int status = ostack[0];
    exit(status);
}

uintptr_t *nativeLoad(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    char *name = String2Cstr((Object*)ostack[0]);

    ostack[0] = resolveDll(name);
    free(name);

    return ostack+1;
}

uintptr_t *mapLibraryName(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    char *name = String2Cstr((Object*)ostack[0]);
    char *lib = getDllName(name);
    free(name);

    *ostack++ = (uintptr_t)Cstr2String(lib);
    free(lib);

    return ostack;
}

uintptr_t *propertiesPreInit(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *properties = (Object *)*ostack;
    addDefaultProperties(properties);
    return ostack;
}

uintptr_t *propertiesPostInit(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *properties = (Object *)*ostack;
    addCommandLineProperties(properties);
    return ostack;
}

/* java.lang.VMClass */

#define GET_CLASS(vmClass) (Class*)vmClass

uintptr_t *isInstance(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    Object *ob = (Object*)ostack[1];

    *ostack++ = ob == NULL ? FALSE : (uintptr_t)isInstanceOf(clazz, ob->class);
    return ostack;
}

uintptr_t *isAssignableFrom(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    Class *clazz2 = (Class*)ostack[1];

    if(clazz2 == NULL)
        signalException("java/lang/NullPointerException", NULL);
    else
        *ostack++ = (uintptr_t)isInstanceOf(clazz, clazz2);

    return ostack;
}

uintptr_t *isInterface(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_INTERFACE(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *isPrimitive(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_PRIMITIVE(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *isArray(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_ARRAY(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *isSynthetic(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_SYNTHETIC(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *isAnnotation(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_ANNOTATION(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *isEnum(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = IS_ENUM(cb) ? TRUE : FALSE;
    return ostack;
}

uintptr_t *getSuperclass(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    ClassBlock *cb = CLASS_CB(GET_CLASS(ostack[0]));
    *ostack++ = (uintptr_t) (IS_PRIMITIVE(cb) || IS_INTERFACE(cb) ? NULL : cb->super);
    return ostack;
}

uintptr_t *getComponentType(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    Class *class = GET_CLASS(ostack[0]);
    ClassBlock *cb = CLASS_CB(class);
    Class *componentType = NULL;

    if(IS_ARRAY(cb))
        switch(cb->name[1]) {
            case '[':
                componentType = findArrayClassFromClass(&cb->name[1], class);
                break;

            default:
                componentType = cb->element_class;
                break;
        }
 
    *ostack++ = (uintptr_t) componentType;
    return ostack;
}

uintptr_t *getName(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    char *dot_name = slash2dots(CLASS_CB((GET_CLASS(*ostack)))->name);
    Object *string = createString(dot_name);
    *ostack++ = (uintptr_t)string;
    free(dot_name);
    return ostack;
}

uintptr_t *getDeclaredClasses(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    int public = ostack[1];
    *ostack++ = (uintptr_t) getClassClasses(clazz, public);
    return ostack;
}

uintptr_t *getDeclaringClass0(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    *ostack++ = (uintptr_t) getDeclaringClass(clazz);
    return ostack;
}

uintptr_t *getDeclaredConstructors(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    int public = ostack[1];
    *ostack++ = (uintptr_t) getClassConstructors(clazz, public);
    return ostack;
}

uintptr_t *getDeclaredMethods(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    int public = ostack[1];
    *ostack++ = (uintptr_t) getClassMethods(clazz, public);
    return ostack;
}

uintptr_t *getDeclaredFields(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    int public = ostack[1];
    *ostack++ = (uintptr_t) getClassFields(clazz, public);
    return ostack;
}

uintptr_t *getInterfaces(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    *ostack++ = (uintptr_t) getClassInterfaces(clazz);
    return ostack;
}

uintptr_t *getClassLoader(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(*ostack);
    *ostack++ = (uintptr_t)CLASS_CB(clazz)->class_loader;
    return ostack;
}

uintptr_t *getClassModifiers(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = GET_CLASS(ostack[0]);
    int ignore_inner_attrs = ostack[1];
    ClassBlock *cb = CLASS_CB(clazz);

    if(!ignore_inner_attrs && cb->declaring_class)
        *ostack++ = (uintptr_t)cb->inner_access_flags;
    else
        *ostack++ = (uintptr_t)cb->access_flags;

    return ostack;
}

uintptr_t *forName0(uintptr_t *ostack, int resolve, Object *loader) {
    Object *string = (Object *)ostack[0];
    Class *class = NULL;
    int len, i = 0;
    char *cstr;
    
    if(string == NULL) {
        signalException("java/lang/NullPointerException", NULL);
        return ostack;
    }

    cstr = String2Cstr(string);
    len = strlen(cstr);

    if(cstr[0] == '[') {
        for(; cstr[i] == '['; i++);
        switch(cstr[i]) {
            case 'Z':
            case 'B':
            case 'C':
            case 'S':
            case 'I':
            case 'F':
            case 'J':
            case 'D':
                if(len-i != 1)
                    goto out;
                break;
            case 'L':
                if(cstr[len-1] != ';')
                    goto out;
                break;
            default:
                goto out;
                break;
        }
    }

    for(; i < len; i++)
        if(cstr[i]=='.') cstr[i]='/';

    class = findClassFromClassLoader(cstr, loader);

out:
    if(class == NULL) {
        Object *e = exceptionOccured();
        clearException();
        signalChainedException("java/lang/ClassNotFoundException", cstr, e);
    } else
        if(resolve)
            initClass(class);

    free(cstr);
    *ostack++ = (uintptr_t)class;
    return ostack;
}

uintptr_t *forName(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    int init = ostack[1];
    Object *loader = (Object*)ostack[2];
    return forName0(ostack, init, loader);
}

uintptr_t *throwException(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *excep = (Object *)ostack[0];
    setException(excep);
    return ostack;
}

uintptr_t *hasClassInitializer(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = (Class*)ostack[0];
    *ostack++ = findMethod(clazz, "<clinit>", "()V") == NULL ? FALSE : TRUE;
    return ostack;
}

/* java.lang.VMThrowable */

uintptr_t *fillInStackTrace(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *ostack++ = (uintptr_t) setStackTrace();
    return ostack;
}

uintptr_t *getStackTrace(Class *class, MethodBlock *m, uintptr_t *ostack) {
    Object *this = (Object *)*ostack;
    *ostack++ = (uintptr_t) convertStackTrace(this);
    return ostack;
}

/* gnu.classpath.VMStackWalker */

Frame *skipFrames(Frame *last) {

loop:
    /* Skip the frame with the found class, and
       check if the previous is a dummy frame */
    if((last = last->prev)->mb == NULL) {

        /* Skip the dummy frame, and check if
         * we're at the top of the stack */
        if((last = last->prev)->prev == NULL)
            return NULL;

        /* Check if we were invoked via reflection */
        if(last->mb->class == getReflectMethodClass()) {

            /* There will be two frames for invoke.  Skip
               the first, and jump back.  This also handles
               recursive invocation via reflection. */

            last = last->prev;
            goto loop;
        }
    }
    return last;
}

Class *getCallingClass0() {
    Frame *last = getExecEnv()->last_frame->prev;

    if((last->mb == NULL && (last = last->prev)->prev == NULL) ||
             (last = skipFrames(last)) == NULL)
        return NULL;

    return last->mb->class;
}

uintptr_t *getCallingClass(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *ostack++ = (uintptr_t) getCallingClass0();
    return ostack;
}

uintptr_t *getCallingClassLoader(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    Class *class = getCallingClass0();

    *ostack++ = (uintptr_t) (class ? CLASS_CB(class)->class_loader : NULL);
    return ostack;
}

uintptr_t *getClassContext(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *class_class = findArrayClass("[Ljava/lang/Class;");
    Object *array;
    Frame *last;

    if(class_class == NULL)
        return ostack;

    if((last = skipFrames(getExecEnv()->last_frame)) == NULL)
        array = allocArray(class_class, 0, sizeof(Class*)); 
    else {
        Frame *bottom = last;
        int depth = 0;

        do {
            for(; last->mb != NULL; last = last->prev, depth++);
        } while((last = last->prev)->prev != NULL);
    
        array = allocArray(class_class, depth, sizeof(Class*));

        if(array != NULL) {
            Class **data = ARRAY_DATA(array);

            depth = 0;
            do {
                for(; bottom->mb != NULL; bottom = bottom->prev)
                    data[depth++] = bottom->mb->class;
            } while((bottom = bottom->prev)->prev != NULL);
        }
    }

    *ostack++ = (uintptr_t)array;
    return ostack;
}

/* java.lang.VMClassLoader */

/* loadClass(Ljava/lang/String;I)Ljava/lang/Class; */
uintptr_t *loadClass(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    int resolve = ostack[1];
    return forName0(ostack, resolve, NULL);
}

/* getPrimitiveClass(C)Ljava/lang/Class; */
uintptr_t *getPrimitiveClass(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    char prim_type = *ostack;
    *ostack++ = (uintptr_t)findPrimitiveClass(prim_type);
    return ostack;
}

uintptr_t *defineClass0(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    Object *class_loader = (Object *)ostack[0];
    Object *string = (Object *)ostack[1];
    Object *array = (Object *)ostack[2];
    int offset = ostack[3];
    int data_len = ostack[4];
    int pd = ostack[5];
    Class *class = NULL;

    if(array == NULL)
        signalException("java/lang/NullPointerException", NULL);
    else
        if((offset < 0) || (data_len < 0) ||
                           ((offset + data_len) > ARRAY_LEN(array)))
            signalException("java/lang/ArrayIndexOutOfBoundsException", NULL);
        else {
            char *data = ARRAY_DATA(array);
            char *cstr = string ? String2Cstr(string) : NULL;
            int len = string ? strlen(cstr) : 0;
            int i;

            for(i = 0; i < len; i++)
                if(cstr[i]=='.') cstr[i]='/';

            if((class = defineClass(cstr, data, offset, data_len, class_loader)) != NULL) {
                INST_DATA(class)[pd_offset] = pd;
                linkClass(class);
            }

            free(cstr);
        }

    *ostack++ = (uintptr_t) class;
    return ostack;
}

uintptr_t *findLoadedClass(Class *clazz, MethodBlock *mb, uintptr_t *ostack) {
    Object *class_loader = (Object *)ostack[0];
    Object *string = (Object *)ostack[1];
    Class *class;
    char *cstr;
    int len, i;

    if(string == NULL) {
        signalException("java/lang/NullPointerException", NULL);
        return ostack;
    }

    cstr = String2Cstr(string);
    len = strlen(cstr);

    for(i = 0; i < len; i++)
        if(cstr[i]=='.') cstr[i]='/';

    class = findHashedClass(cstr, class_loader);

    free(cstr);
    *ostack++ = (uintptr_t) class;
    return ostack;
}

uintptr_t *resolveClass0(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *clazz = (Class *)*ostack;
    initClass(clazz);
    return ostack;
}

uintptr_t *getBootClassPathSize(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *ostack++ = bootClassPathSize();
    return ostack;
}

uintptr_t *getBootClassPathResource(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *string = (Object *) ostack[0];
    char *filename = String2Cstr(string);
    int index = ostack[1];

    *ostack++ = (uintptr_t) bootClassPathResource(filename, index);
    return ostack;
}

/* java.lang.reflect.Constructor */

uintptr_t *constructNative(Class *class, MethodBlock *mb2, uintptr_t *ostack) {
    Object *array = (Object*)ostack[1]; 
    Object *paramTypes = (Object*)ostack[3];
    MethodBlock *mb = (MethodBlock*)ostack[4]; 
    Object *ob = allocObject(mb->class);

    if(ob) invoke(ob, mb, array, paramTypes);

    *ostack++ = (uintptr_t) ob;
    return ostack;
}

uintptr_t *getMethodModifiers(Class *class, MethodBlock *mb2, uintptr_t *ostack) {
    MethodBlock *mb = (MethodBlock*)ostack[1]; 
    *ostack++ = (uintptr_t) mb->access_flags;
    return ostack;
}

uintptr_t *getFieldModifiers(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    FieldBlock *fb = (FieldBlock*)ostack[1]; 
    *ostack++ = (uintptr_t) fb->access_flags;
    return ostack;
}

Object *getAndCheckObject(uintptr_t *ostack, Class *type) {
    Object *ob = (Object*)ostack[1];

    if(ob == NULL) {
        signalException("java/lang/NullPointerException", NULL);
        return NULL;
    }
    if(!isInstanceOf(type, ob->class)) {
        signalException("java/lang/IllegalArgumentException",
                        "object is not an instance of declaring class");
        return NULL;
    }
    return ob;
}

uintptr_t *getPntr2Field(uintptr_t *ostack) {
    Class *decl_class = (Class *)ostack[2];
    FieldBlock *fb = (FieldBlock*)ostack[4]; 
    Object *ob;

    if(fb->access_flags & ACC_STATIC) {
        initClass(decl_class);
        return &fb->static_value;
    }

    if((ob = getAndCheckObject(ostack, decl_class)) == NULL)
        return NULL;

    return &(INST_DATA(ob)[fb->offset]);
}

uintptr_t *getField(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *field_type = (Class *)ostack[3];
    uintptr_t *field;

    /* If field is static, getPntr2Field also initialises the field's declaring class */
    if((field = getPntr2Field(ostack)) != NULL)
        *ostack++ = (uintptr_t) createWrapperObject(field_type, field);
    return ostack;
}

uintptr_t *getPrimitiveField(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *field_type = (Class *)ostack[3];
    int type_no = ostack[5]; 

    ClassBlock *type_cb = CLASS_CB(field_type);
    uintptr_t *field;

    /* If field is static, getPntr2Field also initialises the field's declaring class */
    if(((field = getPntr2Field(ostack)) != NULL) && (!(IS_PRIMITIVE(type_cb)) ||
                 ((ostack = widenPrimitiveValue(getPrimTypeIndex(type_cb), type_no, field, ostack)) == NULL)))
        signalException("java/lang/IllegalArgumentException", "field type mismatch");
    return ostack;
}

uintptr_t *setField(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *field_type = (Class *)ostack[3];
    Object *value = (Object*)ostack[5];
    uintptr_t *field;

    /* If field is static, getPntr2Field also initialises the field's declaring class */
    if(((field = getPntr2Field(ostack)) != NULL) &&
                     (unwrapAndWidenObject(field_type, value, field) == NULL))
        signalException("java/lang/IllegalArgumentException", "field type mismatch");

    return ostack;
}

uintptr_t *setPrimitiveField(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *field_type = (Class *)ostack[3];
    int type_no = ostack[5]; 

    ClassBlock *type_cb = CLASS_CB(field_type);
    uintptr_t *field;

    /* If field is static, getPntr2Field also initialises the field's declaring class */
    if(((field = getPntr2Field(ostack)) != NULL) && (!(IS_PRIMITIVE(type_cb)) ||
                 (widenPrimitiveValue(type_no, getPrimTypeIndex(type_cb), &ostack[6], field) == NULL)))
        signalException("java/lang/IllegalArgumentException", "field type mismatch");
    return ostack;
}

/* java.lang.reflect.Method */

uintptr_t *invokeNative(Class *class, MethodBlock *mb2, uintptr_t *ostack) {
    Object *array = (Object*)ostack[2]; 
    Object *paramTypes = (Object*)ostack[4];
    Class *retType = (Class*)ostack[5];
    MethodBlock *mb = (MethodBlock*)ostack[6]; 
    Object *ob = NULL;
    uintptr_t *ret;

    if(mb->access_flags & ACC_STATIC)
        initClass(mb->class);
    else
        if(((ob = getAndCheckObject(ostack, mb->class)) == NULL) ||
                                     ((mb = lookupVirtualMethod(ob, mb)) == NULL))
            return ostack;
 
    if((ret = (uintptr_t*) invoke(ob, mb, array, paramTypes)) != NULL)
        *ostack++ = (uintptr_t) createWrapperObject(retType, ret);

    return ostack;
}

/* java.lang.VMString */

/* static method - intern(Ljava/lang/String;)Ljava/lang/String; */
uintptr_t *intern(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *string = (Object*)ostack[0];
    ostack[0] = (uintptr_t)findInternedString(string);
    return ostack+1;
}

/* java.lang.VMThread */

/* static method currentThread()Ljava/lang/Thread; */
uintptr_t *currentThread(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    *ostack++ = (uintptr_t)getExecEnv()->thread;
    return ostack;
}

/* static method create(Ljava/lang/Thread;J)V */
uintptr_t *create(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *this = (Object *)ostack[0];
    long long stack_size = *((long long*)&ostack[1]);
    createJavaThread(this, stack_size);
    return ostack;
}

/* static method sleep(JI)V */
uintptr_t *jamSleep(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    long long ms = *((long long *)&ostack[0]);
    int ns = ostack[2];
    Thread *thread = threadSelf();

    threadSleep(thread, ms, ns);
    return ostack;
}

/* instance method interrupt()V */
uintptr_t *interrupt(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *this = (Object *)*ostack;
    Thread *thread = threadSelf0(this);
    if(thread)
        threadInterrupt(thread);
    return ostack;
}

/* instance method isAlive()Z */
uintptr_t *isAlive(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *this = (Object *)*ostack;
    Thread *thread = threadSelf0(this);
    *ostack++ = thread ? threadIsAlive(thread) : FALSE;
    return ostack;
}

/* static method yield()V */
uintptr_t *yield(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Thread *thread = threadSelf();
    threadYield(thread);
    return ostack;
}

/* instance method isInterrupted()Z */
uintptr_t *isInterrupted(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *this = (Object *)*ostack;
    Thread *thread = threadSelf0(this);
    *ostack++ = thread ? threadIsInterrupted(thread) : FALSE;
    return ostack;
}

/* static method interrupted()Z */
uintptr_t *interrupted(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Thread *thread = threadSelf();
    *ostack++ = threadInterrupted(thread);
    return ostack;
}

/* instance method nativeSetPriority(I)V */
uintptr_t *nativeSetPriority(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    return ostack+1;
}

/* instance method holdsLock(Ljava/lang/Object;)Z */
uintptr_t *holdsLock(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Object *ob = (Object *)ostack[0];
    if(ob == NULL)
        signalException("java/lang/NullPointerException", NULL);
    else
        *ostack++ = objectLockedByCurrent(ob);
    return ostack;
}

/* java.security.VMAccessController */

/* instance method getStack()[[Ljava/lang/Object; */
uintptr_t *getStack(Class *class, MethodBlock *mb, uintptr_t *ostack) {
    Class *object_class = findArrayClass("[[Ljava/lang/Object;");
    Class *class_class = findArrayClass("[Ljava/lang/Class;");
    Class *string_class = findArrayClass("[Ljava/lang/String;");
    Object *stack, *names, *classes;
    Frame *frame;
    int depth = 0;

    if(object_class == NULL || class_class == NULL || string_class == NULL)
      return ostack;

    frame = getExecEnv()->last_frame;

    do {
        for(; frame->mb != NULL; frame = frame->prev, depth++);
    } while((frame = frame->prev)->prev != NULL);

    stack = allocArray(object_class, 2, 4);
    classes = allocArray(class_class, depth, 4);
    names = allocArray(string_class, depth, 4);

    if(stack != NULL && names != NULL && classes != NULL) {
        uintptr_t *dcl = INST_DATA(classes);
        uintptr_t *dnm = INST_DATA(names);

        frame = getExecEnv()->last_frame;
        depth = 1;

        do {
            for(; frame->mb != NULL; frame = frame->prev, depth++) {
                dcl[depth] = (uintptr_t) frame->mb->class;
                dnm[depth] = (uintptr_t) createString(frame->mb->name);
            }
        } while((frame = frame->prev)->prev != NULL);

        INST_DATA(stack)[1] = (uintptr_t) classes;
        INST_DATA(stack)[2] = (uintptr_t) names;
    }

    *ostack++ = (uintptr_t) stack;
    return ostack;
}

VMMethod vm_object[] = {
    {"getClass",                    getClass},
    {"clone",                       jamClone},
    {"wait",                        jamWait},
    {"notify",                      notify},
    {"notifyAll",                   notifyAll},
    {NULL,                          NULL}
};

VMMethod vm_system[] = {
    {"arraycopy",                   arraycopy},
    {"identityHashCode",            identityHashCode},
    {NULL,                          NULL}
};

VMMethod vm_runtime[] = {
    {"freeMemory",                  freeMemory},
    {"totalMemory",                 totalMemory},
    {"maxMemory",                   maxMemory},
    {"gc",                          gc},
    {"runFinalization",             runFinalization},
    {"exit",                        exitInternal},
    {"nativeLoad",                  nativeLoad},
    {"mapLibraryName",              mapLibraryName},
    {NULL,                          NULL}
};

VMMethod vm_class[] = {
    {"isInstance",                  isInstance},
    {"isAssignableFrom",            isAssignableFrom},
    {"isInterface",                 isInterface},
    {"isPrimitive",                 isPrimitive},
    {"isArray",                     isArray},
    {"isSynthetic",                 isSynthetic},
    {"isAnnotation",                isAnnotation},
    {"isEnum",                      isEnum},
    {"getSuperclass",               getSuperclass},
    {"getComponentType",            getComponentType},
    {"getName",                     getName},
    {"getDeclaredClasses",          getDeclaredClasses},
    {"getDeclaringClass",           getDeclaringClass0},
    {"getDeclaredConstructors",     getDeclaredConstructors},
    {"getDeclaredMethods",          getDeclaredMethods},
    {"getDeclaredFields",           getDeclaredFields},
    {"getInterfaces",               getInterfaces},
    {"getClassLoader",              getClassLoader},
    {"getModifiers",                getClassModifiers},
    {"forName",                     forName},
    {"throwException",              throwException},
    {"hasClassInitializer",         hasClassInitializer},
    {NULL,                          NULL}
};

VMMethod vm_string[] = {
    {"intern",                      intern},
    {NULL,                          NULL}
};

VMMethod vm_thread[] = {
    {"currentThread",               currentThread},
    {"create",                      create},
    {"sleep",                       jamSleep},
    {"interrupt",                   interrupt},
    {"isAlive",                     isAlive},
    {"yield",                       yield},
    {"isInterrupted",               isInterrupted},
    {"interrupted",                 interrupted},
    {"nativeSetPriority",           nativeSetPriority},
    {"holdsLock",                   holdsLock},
    {NULL,                          NULL}
};

VMMethod vm_throwable[] = {
    {"fillInStackTrace",            fillInStackTrace},
    {"getStackTrace",               getStackTrace},
    {NULL,                          NULL}
};

VMMethod vm_classloader[] = {
    {"loadClass",                   loadClass},
    {"getPrimitiveClass",           getPrimitiveClass},
    {"defineClass",                 defineClass0},
    {"findLoadedClass",             findLoadedClass},
    {"resolveClass",                resolveClass0},
    {"getBootClassPathSize",        getBootClassPathSize},
    {"getBootClassPathResource",    getBootClassPathResource},
    {NULL,                          NULL}
};

VMMethod vm_reflect_constructor[] = {
    {"constructNative",             constructNative},
    {"getConstructorModifiers",     getMethodModifiers},
    {NULL,                          NULL}
};

VMMethod vm_reflect_method[] = {
    {"invokeNative",                invokeNative},
    {"getMethodModifiers",          getMethodModifiers},
    {NULL,                          NULL}
};

VMMethod vm_reflect_field[] = {
    {"getFieldModifiers",           getFieldModifiers},
    {"getField",                    getField},
    {"setField",                    setField},
    {"setZField",                   setPrimitiveField},
    {"setBField",                   setPrimitiveField},
    {"setCField",                   setPrimitiveField},
    {"setSField",                   setPrimitiveField},
    {"setIField",                   setPrimitiveField},
    {"setFField",                   setPrimitiveField},
    {"setJField",                   setPrimitiveField},
    {"setDField",                   setPrimitiveField},
    {"getZField",                   getPrimitiveField},
    {"getBField",                   getPrimitiveField},
    {"getCField",                   getPrimitiveField},
    {"getSField",                   getPrimitiveField},
    {"getIField",                   getPrimitiveField},
    {"getFField",                   getPrimitiveField},
    {"getJField",                   getPrimitiveField},
    {"getDField",                   getPrimitiveField},
    {NULL,                          NULL}
};

VMMethod vm_system_properties[] = {
    {"preInit",                     propertiesPreInit},
    {"postInit",                    propertiesPostInit},
    {NULL,                          NULL}
};

VMMethod vm_stack_walker[] = {
    {"getClassContext",             getClassContext},
    {"getCallingClass",             getCallingClass},
    {"getCallingClassLoader",       getCallingClassLoader},
    {NULL,                          NULL}
};

VMMethod vm_access_controller[] = {
    {"getStack",                    getStack},
    {NULL,                          NULL}
};

VMClass native_methods[] = {
    {"java/lang/VMClass",                vm_class},
    {"java/lang/VMObject",               vm_object},
    {"java/lang/VMThread",               vm_thread},
    {"java/lang/VMSystem",               vm_system},
    {"java/lang/VMString",               vm_string},
    {"java/lang/VMRuntime",              vm_runtime},
    {"java/lang/VMThrowable",            vm_throwable},
    {"java/lang/VMClassLoader",          vm_classloader},
    {"java/lang/reflect/Field",          vm_reflect_field},
    {"java/lang/reflect/Method",         vm_reflect_method},
    {"java/lang/reflect/Constructor",    vm_reflect_constructor},
    {"java/security/VMAccessController", vm_access_controller},
    {"gnu/classpath/VMSystemProperties", vm_system_properties},
    {"gnu/classpath/VMStackWalker",      vm_stack_walker},
    {NULL,                               NULL}
};