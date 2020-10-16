#pragma once
#include <stdio.h>
struct serializerEntryPair;
struct serializer ;
void **deserializeFile(const mapDeserializerFunc structures, FILE *file,
                       long *count);
struct serializer *serializerCreate();
void serializerFieldStr(struct serializer *ser, const char *name,
                        const char *data, long len);
void serializerFieldPtr(struct serializer *ser, const char *name,
                        const void *ptr);
void serializerFieldObject(struct serializer *ser, const char *name,
                           const char *typeName, const void *data,
                           void (*dumper)(const void *data,
                                          struct serializer *serializer));
