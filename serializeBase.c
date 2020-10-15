#include <hashTable.h>
#include <linkedList.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <str.h>
#include <unistd.h>
#define POINTER_SIZE 8
struct serialEntry {
	uint64_t dataStartOffset;
	uint64_t typeNameOffset;
	uint64_t size;
};
struct serializer;
struct toSerialize {
	const void *item;
};
LL_TYPE_DEF(struct serialEntry, SerialEntry);
LL_TYPE_FUNCS(struct serialEntry, SerialEntry);

STR_TYPE_DEF(char *, Name);
STR_TYPE_FUNCS(char *, Name);

struct relocation {
	long dataOffset;
	const void *pointsTo;
};
STR_TYPE_DEF(struct relocation, Relocation);
STR_TYPE_FUNCS(struct relocation, Relocation);
MAP_TYPE_DEF(strRelocation, Relocations);
MAP_TYPE_FUNCS(strRelocation, Relocations);

struct __serialEntry {
	long offset;
	const char *name;
};
LL_TYPE_DEF(struct __serialEntry, Entry);
LL_TYPE_FUNCS(struct __serialEntry, Entry);
struct serializer {
	long nameFileStart, dataFileStart;
	FILE *nameTableTmp;
	FILE *dataTmp;
	long currentNameTablePos;
	strName names;
	mapRelocations relocations;
	llEntry entryOffsets;
};
void serializerPush8(struct serializer *serializer, int8_t value) {
	fputc(value, serializer->dataTmp);
}
void serializerPush16(struct serializer *serializer, int16_t value) {
#if __ORDER_LITTLE_ENDIAN__
#elif __ORDER_BIG_ENDIAN__
	value = __builtin_bswap16(value);
#else
#error "Machine isn't big or little endian"
#endif
	fwrite(&value, 2, 1, serializer->dataTmp);
}
void serializerPush32(struct serializer *serializer, int32_t value) {
#if __ORDER_LITTLE_ENDIAN__
#elif __ORDER_BIG_ENDIAN__
	value = __builtin_bswap32(value);
#else
#error "Machine isn't big or little endian"
#endif
	fwrite(&value, 4, 1, serializer->dataTmp);
}
void serializerPush64(struct serializer *serializer, int64_t value) {
#if __ORDER_LITTLE_ENDIAN__
#elif __ORDER_BIG_ENDIAN__
	value = __builtin_bswap64(value);
#else
#error "Machine isn't big or little endian"
#endif
	fwrite(&value, 8, 1, serializer->dataTmp);
}
struct serializer *serializerCreate() {
	struct serializer *retVal = malloc(sizeof(struct serializer));
	retVal->nameTableTmp = tmpfile();
	retVal->dataTmp = tmpfile();
	retVal->dataFileStart = ftell(retVal->dataTmp);
	retVal->nameFileStart = ftell(retVal->nameTableTmp);
	retVal->names = NULL;
	return retVal;
}
static int nameInsertPred(const void *a, const void *b) { return strcmp(a, b); }
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}
void serializerSerialize(struct serializer *serializer,
                         void (*dumper)(const void *data, const char **typeName,
                                        struct serializer *serializer),
                         const void *data) {
	const char *typeName;
	__auto_type originalOffset =
	    ftell(serializer->dataTmp) - serializer->dataFileStart;

	struct serialEntry entry;
	entry.dataStartOffset =
	    ftell(serializer->dataTmp) - serializer->dataFileStart;
	entry.size = -1;
	entry.typeNameOffset = -1;
	fwrite(&entry, 1, sizeof(struct serialEntry), serializer->dataTmp);

	dumper(data, &typeName, serializer);

	// First,register object name in nameTable
	if (NULL == strNameSortedFind(serializer->names, typeName, nameInsertPred))
		serializer->names = strNameSortedInsert(serializer->names, (char *)typeName,
		                                        nameInsertPred);

	__auto_type endOffset =
	    ftell(serializer->dataTmp) - serializer->dataFileStart;
	fseek(serializer->dataTmp, originalOffset - endOffset, SEEK_CUR);

	// Write item size
	fseek(serializer->dataTmp, offsetof(struct serialEntry, size), SEEK_CUR);
	serializerPush64(serializer,
	                 endOffset - originalOffset - sizeof(struct serialEntry));
	// Go back to front
	fseek(serializer->dataTmp, 0, SEEK_END);

	// Insert offset
	struct __serialEntry entry2;
	entry2.name = typeName, entry2.offset = originalOffset;
	serializer->entryOffsets =
	    llEntryInsert(serializer->entryOffsets, llEntryCreate(entry2), ptrCmp);
}
void serializerPushData(struct serializer *serializer, const void *buffer,
                        long len) {
	fwrite(buffer, 1, len, serializer->dataTmp);
}
void serializerPushPointer(struct serializer *serializer, const void *ptr) {
	__auto_type offset = ftell(serializer->dataTmp) - serializer->dataFileStart;

	char buffer[POINTER_SIZE];
	memset(buffer, 0, POINTER_SIZE);
	fwrite(buffer, 1, POINTER_SIZE, serializer->dataTmp);

	struct relocation relocation;
	relocation.dataOffset = offset;
	relocation.pointsTo = ptr;

	char buffer2[64];
	sprintf(buffer2, "%p", ptr);
	__auto_type find = mapRelocationsGet(serializer->relocations, buffer2);
	if (find == NULL) {
		mapRelocationsInsert(serializer->relocations, buffer,
		                     strRelocationAppendItem(NULL, relocation));
	} else {
		*find = strRelocationAppendItem(*find, relocation);
	}
}

#if __ORDER_LITTLE_ENDIAN__
#define write64(value,file) ({uint64_t value64= value;fwrite(&value64,sizeof(value64),1,file);})
#elif __ORDER_BIG_ENDIAN__
	#define write64(value,file)  ({uint64_t value64= __builtin_bswap64(value);fwrite(&value64,sizeof(value64),1,file);})
#else
#error "Machine isn't big or little endian"
#endif
void serializerDumpTo(const struct serializer *serializer, FILE *dumpTo) {
 //Create name table
 FILE *namesTable=tmpfile();
 uint64_t totalSize=0;
 fwrite(&totalSize,sizeof(totalSize),1,namesTable);
 
 for(long i=0;i!=strNameSize(serializer->names);i++) {
	__auto_type len=strlen(serializer->names[i])+1;
		fwrite(serializer->names[i],1,len,namesTable);
		totalSize+=len;
 }
 
 //Write size
 fseek(namesTable,0,SEEK_SET);
 write64(totalSize,namesTable);

 /** Create Pointer table
	* [PointerTableEntryCount:64]
	* 
	* [Pointer1:64]
	* [EntryTableOffset1:64]
	* [ReferenceCount1:64]
	* [referenceOffsets1:64]...
	* 
	* [PointerN:64]
	* [EntryTableOffsetN:64]
	* [ReferenceCountN:64]
	* [referenceOffsetsN:64]...
	*/
 FILE *pointerTable=tmpfile();
 totalSize=0;
 write64(totalSize,pointerTable);
}
