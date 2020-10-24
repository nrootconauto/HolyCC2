#include <assert.h>
#include <hashTable.h>
#include <linkedList.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <str.h>
#include <unistd.h>
#include <serializeBase.h>
enum fieldType {
	FIELD_ARRAY = 1,
	FIELD_INT = 2,
	FIELD_FLOAT = 4,
	FIELD_CHAR = 8,
	FIELD_OBJECT = 16,
	FIELD_POINTER = 32,
};
struct serializer;
struct serialEntry {
	uint64_t dataStartOffset;
	uint64_t fieldsCount;
	uint64_t typeNameIndex;
	uint64_t entryID;
};
struct serialField {
	uint64_t size;
	uint64_t nameIndex;
	uint64_t type; // enum fieldType
};
struct serializer;
struct toSerialize {
	const void *item;
};
LL_TYPE_DEF(struct serialEntry, SerialEntry);
LL_TYPE_FUNCS(struct serialEntry, SerialEntry);

STR_TYPE_DEF(char *, Name);
STR_TYPE_FUNCS(char *, Name);

struct __serialField {
	long offset, size, entryDepth;
	const char *name;
};
STR_TYPE_DEF(struct __serialField, Field);
STR_TYPE_FUNCS(struct __serialField, Field);
struct __serialEntry {
	long offset;
	long entryId;
	const char *name;
	const char *typeName;
	const void *ptr;
	strField fields;
};
LL_TYPE_DEF(struct __serialEntry, Entry);
LL_TYPE_FUNCS(struct __serialEntry, Entry);
STR_TYPE_DEF(llEntry, LLEntry);
STR_TYPE_FUNCS(llEntry, LLEntry);

MAP_TYPE_DEF(long, PtrIndex);
MAP_TYPE_FUNCS(long, PtrIndex);

STR_TYPE_DEF(void *, Ptr);
STR_TYPE_FUNCS(void *, Ptr);
struct serializer {
	long  dataFileStart;
	FILE *dataTmp;
	long currentNameTablePos;
	strName names;
	llEntry entryOffsets;
	llEntry currentEntryNode;
	long pointerCount;
	long entryCount;
	mapPtrIndex pointers;
	strPtr registeredItems;
	const mapSerializerFunc *funcs;
};
void serializerSerialize(struct serializer *ser, const char *fieldName,
                         const char *typeName, const void *data,
                         void (*dumper)(const void *data,
                                        struct serializer *serializer));
void serializerStartField(struct serializer *serializer, const char *name);
static void serializerEndField(struct serializer *serializer,
                               enum fieldType type);
#if __ORDER_LITTLE_ENDIAN__
#define write64(value, file)                                                   \
	({                                                                           \
		int64_t value64 = value;                                                   \
		fwrite(&value64, sizeof(value64), 1, file);                                \
	})
#define read64(file)                                                           \
	({                                                                           \
		int64_t value64;                                                           \
		fread(&value64, sizeof(value64), 1, file);                                 \
		value64;                                                                   \
	})
#define value64(value)                                                         \
	({                                                                           \
		int64_t value64 = value;                                                   \
		value64;                                                                   \
	})
void serializerPush64(struct serializer *ser, const char *name, int64_t value) {
	serializerStartField(ser, name);

	__auto_type fields = &llEntryValuePtr(ser->currentEntryNode)->fields;
	assert(*fields != NULL);

	fwrite(&value, 8, 1, ser->dataTmp);
	serializerEndField(ser, FIELD_INT);
}
// TODO implement me
void serializerPushFloat(struct serializer *ser, const char *name,
                         double value) {
	serializerStartField(ser, name);

	__auto_type fields = &llEntryValuePtr(ser->currentEntryNode)->fields;
	assert(*fields != NULL);

	assert("IMPLEMENTED" == NULL);
	serializerEndField(ser, FIELD_FLOAT);
}
// TODO implement me
void serializerPushFloats(struct serializer *ser, const char *name, long count,
                          const double *value) {}

int64_t serializerGet64(FILE *file) {
	char buffer[sizeof(int64_t)];
	fread(buffer, sizeof(int64_t), 1, file);
	return *(int64_t *)buffer;
}
#elif __ORDER_BIG_ENDIAN__
#define write64(value, file)                                                   \
	({                                                                           \
		uint64_t value64 = __builtin_bswap64(value);                               \
		fwrite(&value64, sizeof(value64), 1, file);                                \
	})
void serializerPush64(struct serializer *serializer, int64_t value) {
	assert(llEntryValuePtr(serializer->currentEntryNode)->fields != NULL);
	value = __builtin_swap64(value);
	fwrite(&value, 8, 1, serializer->dataTmp);

	serializerEndField(serializer, FIELD_INT);
}
int64_t serializerGet64(FILE *file) {
	char buffer[sizeof(int64_t)];
	fread(buffer, sizeof(int64_t), 1, file);

	return __builtin_bswap64(*(int64_t *)buffer);
}
#else
#error "Machine isn't big or little endian"
#endif
void serializerFieldPtr(struct serializer *ser, const char *name,
                        const void *ptr) {
	serializerStartField(ser, name);

	char buffer[64];
	sprintf(buffer, "%p", ptr);
loop:;
	__auto_type find = mapPtrIndexGet(ser->pointers, buffer);
	if (find == NULL) {
		mapPtrIndexInsert(ser->pointers, buffer, ser->pointerCount++);
		goto loop;
	}
	write64(*find, ser->dataTmp);

	serializerEndField(ser, FIELD_POINTER);
}
void serializerFieldStr(struct serializer *ser, const char *name,
                        const char *data, long len) {
	serializerStartField(ser, name);
	
	fwrite(data, 1, len, ser->dataTmp);
	
	serializerEndField(ser, FIELD_CHAR | FIELD_ARRAY);
}
void serializerFieldInt64(struct serializer *ser, const char *name,int64_t value) {
 serializerStartField(ser, name);
 
 write64(value,ser->dataTmp);
 
 serializerEndField(ser, FIELD_INT);
} 
void serializerFieldInt64s(struct serializer *ser, const char *name,
                           const uint64_t *ints, long len) {
	serializerStartField(ser, name);

	for (long i = 0; i != len; i++)
		write64(ints[i], ser->dataTmp);

	serializerEndField(ser, FIELD_INT | FIELD_ARRAY);
}
void serializerFieldObject(struct serializer *ser, const char *name,
                           const char *typeName, const void *data) {
	__auto_type dumper=mapSerializerFuncGet( *ser->funcs,typeName);
	assert(dumper!=NULL);
	
	serializerSerialize(ser, name, typeName, data, *dumper);
	serializerEndField(ser, FIELD_OBJECT);
};
void serializerPushObjects(struct serializer *ser, const char *name,
                           const char *typeName, const void *data,
                           long itemSize, long count) {
	serializerStartField(ser, name);

	__auto_type dumper=mapSerializerFuncGet( *ser->funcs,typeName);
	assert(dumper!=NULL);
	
	for(long i=0;i!=count;i++)
	serializerSerialize(ser, name, typeName, data+itemSize*i, *dumper);

	serializerEndField(ser, FIELD_ARRAY | FIELD_OBJECT);
};
static int nameInsertPred(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}
static void serializerRegisterName(struct serializer *serializer,
                                   const char *name) {
	// First,register object name in nameTable
	if (NULL == strNameSortedFind(serializer->names, name, nameInsertPred))
		serializer->names =
		    strNameSortedInsert(serializer->names, (char *)name, nameInsertPred);
}
void serializerStartField(struct serializer *serializer, const char *name) {

	struct serialField tmp;
	tmp.nameIndex = 0, tmp.size = 0,

	serializerRegisterName(serializer, name);

	long fieldStart =
	    sizeof(tmp) + ftell(serializer->dataTmp) - serializer->dataFileStart;

	struct __serialField feild;
	feild.name = name, feild.offset = fieldStart, feild.size = -1;

	// Append feild
	__auto_type fields = &llEntryValuePtr(serializer->currentEntryNode)->fields;
	*fields = strFieldAppendItem(*fields, feild);

	fwrite(&tmp, sizeof(tmp), 1, serializer->dataTmp);
}
static void serializerEndField(struct serializer *serializer,
                               enum fieldType type) {
	__auto_type fields = &llEntryValuePtr(serializer->currentEntryNode)->fields;
	if (*fields == NULL)
		return;
	__auto_type last = &fields[0][strFieldSize(fields[0]) - 1];
	if (last->size == -1) {
		long fieldEnd = ftell(serializer->dataTmp) - serializer->dataFileStart;
		long fieldSize = fieldEnd - last->offset;
		last->size = fieldSize;

		long fieldHeaderOffset =
		    last->offset - ftell(serializer->dataTmp) + serializer->dataFileStart;

		// Write field size to field header
		fseek(serializer->dataTmp, fieldHeaderOffset, SEEK_CUR);
		fseek(serializer->dataTmp, offsetof(struct serialField, size), SEEK_CUR);
		write64(fieldSize, serializer->dataTmp);

		// Write fieldType
		fseek(serializer->dataTmp, fieldHeaderOffset, SEEK_CUR);
		fseek(serializer->dataTmp, offsetof(struct serialField, type), SEEK_CUR);
		write64(type, serializer->dataTmp);

		fseek(serializer->dataTmp, 0, SEEK_END);
	}
}
struct serializer *serializerCreate(const mapSerializerFunc *funcs) {
	struct serializer *retVal = malloc(sizeof(struct serializer));
	retVal->dataTmp = tmpfile();
	retVal->dataFileStart = ftell(retVal->dataTmp);
	retVal->names = NULL;
	retVal->entryCount=0;
	retVal->pointerCount= 0;
	retVal->entryOffsets=NULL;
	retVal->pointers=mapPtrIndexCreate();
	retVal->registeredItems=NULL;
	retVal->funcs=funcs;
	return retVal;
}
static void killEntry(void *ptr) {
 struct __serialEntry *ptr2=ptr;
 strFieldDestroy(&ptr2->fields) ;
}
void serializerDestroy(struct serializer **ser) {
 llEntryDestroy(&ser[0]->entryOffsets, killEntry); 
 fclose(ser[0]->dataTmp);
 for(long i=0;i!=strNameSize(ser[0]->names);i++)
		free(ser[0]->names[i]);
 strNameDestroy(&ser[0]->names);
 strPtrDestroy(&ser[0]->registeredItems); 
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}
void serializerSerialize(struct serializer *ser, const char *fieldName,
                         const char *typeName, const void *data,
                         void (*dumper)(const void *data,
                                        struct serializer *serializer)) {
	if (NULL != strPtrSortedFind(ser->registeredItems, data, ptrPtrCmp))
		return;

	ser->registeredItems =
	    strPtrSortedInsert(ser->registeredItems, (void *)data, ptrPtrCmp);

	serializerRegisterName(ser, fieldName);
	serializerRegisterName(ser, typeName);

	__auto_type originalOffset = ftell(ser->dataTmp) - ser->dataFileStart;
	__auto_type newEntryID = ser->entryCount++;

	struct serialEntry entry;
	entry.dataStartOffset = value64(ftell(ser->dataTmp) - ser->dataFileStart);
	entry.fieldsCount = value64(-1);
	entry.typeNameIndex = value64(-1);
	entry.entryID = value64(newEntryID);
	fwrite(&entry, 1, sizeof(struct serialEntry), ser->dataTmp);

	// Create new entry
	struct __serialEntry newEntry;
	newEntry.ptr = data;
	newEntry.offset = entry.dataStartOffset;
	newEntry.name = NULL; // Will be changed ahead
	newEntry.fields = NULL;
	newEntry.entryId = newEntryID;
	__auto_type newEntryNode = llEntryCreate(newEntry);
	ser->entryOffsets = llEntryInsert(ser->entryOffsets, newEntryNode, ptrPtrCmp);

	dumper(data, ser);

	llEntryValuePtr(newEntryNode)->name = fieldName; // Update name;

	__auto_type endOffset = ftell(ser->dataTmp) - ser->dataFileStart;
	fseek(ser->dataTmp, originalOffset - endOffset + ser->dataFileStart,
	      SEEK_CUR);

	// Write item size
	fseek(ser->dataTmp, offsetof(struct serialEntry, fieldsCount), SEEK_CUR);
	long fieldCount =
	    strFieldSize(llEntryValuePtr(ser->currentEntryNode)->fields);
	write64(fieldCount, ser->dataTmp);
	// Go back to front
	fseek(ser->dataTmp, 0, SEEK_END);

	// Insert offset
	struct __serialEntry entry2;
	entry2.name = fieldName, entry2.offset = originalOffset, entry2.ptr = data,
	entry2.typeName = typeName;
	ser->entryOffsets =
	    llEntryInsert(ser->entryOffsets, llEntryCreate(entry2), ptrPtrCmp);
}
void serializerPushData(struct serializer *serializer, const void *buffer,
                        long len) {
	fwrite(buffer, 1, len, serializer->dataTmp);
}
static void concatFile(FILE *dst, FILE *in) {
	fseek(in, 0, SEEK_END);
	long ntEnd = ftell(in);
	fseek(in, 0, SEEK_SET);

	__auto_type len = ntEnd - ftell(in);
	char buffer[len];
	fread(buffer, 1, len, in);

	fwrite(buffer, 1, len, dst);
}
void serializerDumpTo(struct serializer *ser, FILE *dumpTo) {
	// Create name table
	FILE *namesTable = tmpfile();
	uint64_t totalSize = 0;
	fwrite(&totalSize, sizeof(totalSize), 1, namesTable);

	for (long i = 0; i != strNameSize(ser->names); i++) {
		__auto_type len = strlen(ser->names[i]) + 1;
		fwrite(ser->names[i], 1, len, namesTable);
		totalSize += len;
	}

	/**
	 * Create pointer table
	 * [PtrCount:64]
	 *
	 * [PtrIndex-1:64]
	 * [EntryID-1:64]
	 * [PtrIndex-N:64]
	 * [EntryID-N:64]
	 * ...
	 */
	FILE *pointerTable = tmpfile();
	write64(-1, pointerTable);
	long size = 0;
	for (__auto_type node = ser->entryOffsets; node != NULL;
	     node = llEntryNext(node), size++) {
		__auto_type valuePtr = llEntryValuePtr(node);

		char buffer[64];
		sprintf(buffer, "%p", valuePtr->ptr);
		__auto_type find = mapPtrIndexGet(ser->pointers, buffer);
		if (find == NULL)
			continue;

		write64(find[0], pointerTable);
		write64(valuePtr->entryId, pointerTable);
	}
	// Write size of pointer table
	fseek(pointerTable, 0, SEEK_SET);
	write64(size, pointerTable);

	// Write size
	fseek(namesTable, 0, SEEK_SET);
	write64(totalSize, namesTable);

	// Fill in field names of entries
	for (__auto_type node = llEntryFirst(ser->entryOffsets); node != NULL;
	     node = llEntryNext(node)) {
		__auto_type entry = llEntryValuePtr(node);

		__auto_type find =
		    strNameSortedFind(ser->names, entry->name, nameInsertPred);
		assert(find != NULL);

		// Seek to type name index
		fseek(ser->dataTmp,
		      entry->offset - ftell(ser->dataTmp) + ser->dataFileStart, SEEK_CUR);
		fseek(ser->dataTmp, offsetof(struct serialEntry, typeNameIndex), SEEK_CUR);
		write64((find - ser->names) / sizeof(const char *), ser->dataTmp);

		// Fill in field names
		for (long i = 0; i != strFieldSize(entry->fields); i++) {
			__auto_type field = &entry->fields[i];

			__auto_type find =
			    strNameSortedFind(ser->names, field->name, nameInsertPred);
			assert(find != NULL);
			// Seek to field name index
			fseek(ser->dataTmp,
			      field->offset - ftell(ser->dataTmp) + ser->dataFileStart, SEEK_CUR);
			fseek(ser->dataTmp, offsetof(struct serialField, nameIndex), SEEK_CUR);
			write64((find - ser->names) / sizeof(const char *), ser->dataTmp);
		}
	}

	// Combine string table,pointer table,entries
	concatFile(dumpTo, namesTable);
	concatFile(dumpTo, pointerTable);
	concatFile(dumpTo, ser->dataTmp);

	fclose(namesTable);
	fclose(pointerTable);
}
static char *stringFromFile(FILE *file) {
	long originalPos = ftell(file), len = 0;

	while (0 != fgetc(file))
		originalPos++, ++len;

	fseek(file, originalPos - ftell(file), SEEK_CUR);
	char buffer[len + 1];
	fread(buffer, 1, len + 1, file);

	char *retVal = malloc(len + 1);
	strncpy(retVal, buffer, len + 1);

	return retVal;
}
union entryValue {
	void **object;
	int64_t *integer;
	int64_t *ptrRef;
	char *chr;
	double *flt;
};
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);

struct serializerEntryPair {
	enum fieldType type;
	union entryValue data;
};
MAP_TYPE_FUNCS(struct serializerEntryPair, SerializerKeys);

STR_TYPE_DEF(void **, PtrPtr);
STR_TYPE_FUNCS(void **, PtrPtr);
MAP_TYPE_DEF(strPtrPtr, PointerRefs);
MAP_TYPE_FUNCS(strPtrPtr, PointerRefs);
MAP_TYPE_DEF(void *, Ptr);
MAP_TYPE_FUNCS(void *, Ptr);
struct deserializer {
	strName names;
	const mapDeserializerFunc *funcs;
	mapPointerRefs ptrRefs;
	mapPtr entryID2Item;
};

static void destroylSerializerEntry(void *data) {
	struct serializerEntryPair *pair = data;
	free(&pair->data.chr);
}
static void *deserializeProcessEntry(struct deserializer *des, FILE *file);
static mapSerializerKeys deserializeProcessFields(struct deserializer *des,
                                                  struct serialEntry *entry,
                                                  FILE *file) {
	mapSerializerKeys entries = mapSerializerKeysCreate();
	for (long i = 0; i != entry->fieldsCount; i++) {
		struct serialField field;
		fread(&field, sizeof(struct serialField), 1, file);
		field.nameIndex = value64(field.nameIndex);
		field.size = value64(field.size);
		field.type = value64(field.type);

		if (mapSerializerKeysGet(entries, des->names[field.nameIndex]))
			continue;

		int isArray = field.type & FIELD_ARRAY;

		long fPosEnd = ftell(file) + field.size;
		strChar data = NULL;
	loop:
		if (field.type == FIELD_CHAR) {
			data = strCharAppendItem(data, fgetc(file));
		} else if (field.type == FIELD_FLOAT) {
			// TODO implement me
		} else if (field.type == FIELD_INT) {
			int64_t newInt;
			fread(&newInt, sizeof(int64_t), 1, file);
			newInt = value64(newInt);
			data = strCharAppendData(data, (void *)&newInt, sizeof(int64_t));
		} else if (field.type == FIELD_OBJECT) {
			void *item = deserializeProcessEntry(des, file);
			data = strCharAppendData(data, (void *)&item, sizeof(item));
		} else if (field.type == FIELD_POINTER) {
			int64_t ptr = read64(file);
			data = strCharAppendData(data, (void *)&ptr, sizeof(int64_t));
		} else {
			assert(0);
		}

		if (isArray && fPosEnd > ftell(file))
			goto loop;

		assert(ftell(file) == fPosEnd);

		struct serializerEntryPair tmp;
		// All items in entryValue are pointers
		tmp.data.chr = malloc(strCharSize(data));
		memcpy(tmp.data.chr, data, strCharSize(data));
		strCharDestroy(&data);

		tmp.type = field.type;
		mapSerializerKeysInsert(entries, des->names[field.nameIndex], tmp);
	}

	return entries;
}
void deserializeSetPointerCallback(struct deserializer *des, long whichPtr,
                                   void **writeTo) {
loop:;
	char buffer[64];
	sprintf(buffer, "%li", whichPtr);
	__auto_type find = mapPointerRefsGet(des->ptrRefs, buffer);
	if (find == NULL) {
		mapPointerRefsInsert(des->ptrRefs, buffer, NULL);
		goto loop;
	}

	*find = strPtrPtrAppendItem(*find, writeTo);
}
static void *deserializeProcessEntry(struct deserializer *des, FILE *file) {
	struct serialEntry entry;

	fread(&entry, sizeof(struct serialEntry), 1, file);
	entry.dataStartOffset = value64(entry.dataStartOffset);
	entry.fieldsCount = value64(entry.fieldsCount);
	entry.typeNameIndex = value64(entry.typeNameIndex);
	entry.entryID=value64(entry.entryID);

	__auto_type entries = deserializeProcessFields(des, &entry, file);

	__auto_type typeFunc =
	    mapDeserializerFuncGet(*des->funcs, des->names[entry.typeNameIndex]);
	assert(typeFunc != NULL);
	__auto_type retVal = typeFunc[0](entries);

	mapSerializerKeysDestroy(entries, destroylSerializerEntry);

	// Associate the computed result with the entry ID
	char buffer[64];
	sprintf(buffer,"%li",(long)entry.entryID);
	assert(NULL==mapPtrGet(des->entryID2Item,buffer));
	mapPtrInsert(des->entryID2Item,buffer,retVal);
	
	return retVal;
}
static void strPtrPtrDestroy2(void *ptr) {
	strPtrPtr *str = ptr;
	strPtrPtrDestroy(str);
}

MAP_TYPE_DEF(long, Long);
MAP_TYPE_FUNCS(long, Long);
void **deserializeFile(const mapDeserializerFunc structures, FILE *file,
                       long *count) {
	struct deserializer tmp;
	tmp.names = NULL;
	tmp.funcs = &structures;
	tmp.ptrRefs = mapPointerRefsCreate();
	tmp.entryID2Item = mapPtrCreate();

	fseek(file, 0, SEEK_END);
	long fileEnd = ftell(file);
	fseek(file, 0, SEEK_SET);
	long fileStart = ftell(file);

	/**
	 * Read name table
	 */
	__auto_type len = read64(file);
	for (long i = ftell(file); fileStart + len != i; i = ftell(file))
		tmp.names = strNameAppendItem(tmp.names, stringFromFile(file));

	/**
	 * Read pointer table
	 */
	len = read64(file);
	mapLong pointerIDsToEntryID = mapLongCreate();
	for (long i = 0; i != len; i++) {
		long mapID = read64(file);
		long entryID = read64(file);

		char buffer[64];
		sprintf(buffer, "%li", mapID);
		assert(NULL != mapLongGet(pointerIDsToEntryID, buffer));

		mapLongInsert(pointerIDsToEntryID, buffer, entryID);
	}
	/**
	 * Process entries
	 */
	strPtr retValPtrs = NULL;
	long count2;
	for (count2 = 0; fileEnd != ftell(file); count2++)
		retValPtrs =
		    strPtrAppendItem(retValPtrs, deserializeProcessEntry(&tmp, file));

	void **retVal = malloc(sizeof(void *) * count2);
	memcpy(retVal, retValPtrs, sizeof(void *) * count2);
 
	if(count!=NULL)
	 *count=count2;
	/**
	 * Replace pointer references
	 */
	long refedPtrCount;
	mapPointerRefsKeys(tmp.ptrRefs, NULL, &refedPtrCount);
	const char *refedPtrsIDs[refedPtrCount];
	mapPointerRefsKeys(tmp.ptrRefs, refedPtrsIDs, &refedPtrCount);
	for (long i = 0; i != refedPtrCount; i++) {
		__auto_type refs = mapPointerRefsGet(tmp.ptrRefs, refedPtrsIDs[i]);
		assert(refs != NULL);

		__auto_type entryID = mapLongGet(pointerIDsToEntryID, refedPtrsIDs[i]);
		void *ptr;
		if (entryID == NULL) {
			ptr = NULL;
		} else {
		 char buffer[64];
		 sprintf(buffer,"%li",(long)entryID);
		 
		 ptr=mapPtrGet(tmp.entryID2Item,buffer);
		 assert(ptr!=NULL);
		}
		
		for(long i2=0;i2!=strPtrPtrSize(*refs);i2++)
		 *refs[0][i2]=ptr;
	}

	// Destroy deserializer
	mapPointerRefsDestroy(tmp.ptrRefs, strPtrPtrDestroy2);
	for (long i = 0; i != strNameSize(tmp.names); i++)
		free(tmp.names[i]);
	strNameDestroy(&tmp.names);
	mapLongDestroy(tmp.entryID2Item,NULL);

	mapLongDestroy(pointerIDsToEntryID, NULL);
	return retVal;
}
static void writeSize(void *ptr,int64_t value,long size) {
 if(size==1) {
		*(int8_t*)ptr=value;
	 } else if(size==2) {
		*(int16_t*)ptr=value;
	 } else if(size==4) {
		*(int32_t*)ptr=value;
	 } else if(size==8) {
		*(int64_t*)ptr=value;
	 } else {
		assert(0);
	 }
}
int deserializerWriteInt(void *ptr,long size,const char *name,const mapSerializerKeys keys) {
 __auto_type find=mapSerializerKeysGet(keys,name);
 
 if(find!=NULL) {
	if(find->type==FIELD_INT) {
	 int64_t value=*find->data.integer;
	 writeSize(ptr,value,size);
	}
	return 1;
 }
 return 0;
}
int deserializerWriteString(void *ptr,long size,const char *name,const mapSerializerKeys keys) {
 __auto_type find=mapSerializerKeysGet(keys,name);
 
 if(find!=NULL) {
	if(find->type==(FIELD_ARRAY|FIELD_CHAR)) {
	 char *value=find->data.chr;
	 value|=;
	}
 }
}
