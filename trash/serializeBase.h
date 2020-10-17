#pragma once
#include <stdio.h>
#include <hashTable.h>
struct serializerEntryPair;
struct serializer ;

typedef void (*serializerFunc)(const void *data, struct serializer *serializer);
MAP_TYPE_DEF(serializerFunc,SerializerFunc);
MAP_TYPE_FUNCS(serializerFunc,SerializerFunc);
struct serializer *serializerCreate(const mapSerializerFunc *funcs);
void serializerFieldStr(struct serializer *ser, const char *name,
                        const char *data, long len);
void serializerFieldPtr(struct serializer *ser, const char *name,
                        const void *ptr);
void serializerFieldObject(struct serializer *ser, const char *name,
                           const char *typeName, const void *data);
void serializerFieldInt64s(struct serializer *ser, const char *name,
                           const uint64_t *ints, long len);
void serializerFieldInt64(struct serializer *ser, const char *name,int64_t value);
void serializerPushObjects(struct serializer *ser, const char *name,
                           const char *typeName, const void *data,
                           long itemSize, long count);
void serializerDestroy(struct serializer **ser) ;

struct serializerEntryPair ;
MAP_TYPE_DEF(struct serializerEntryPair, SerializerKeys);

typedef void *(*deserializeFunc)(mapSerializerKeys keys);
MAP_TYPE_DEF(deserializeFunc, DeserializerFunc);
MAP_TYPE_FUNCS(deserializeFunc, DeserializerFunc);
void **deserializeFile(const mapDeserializerFunc structures, FILE *file,
                       long *count);
