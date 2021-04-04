class CKeyValPair {
						CKeyValPair *prev,*next;
						U8i *text;
						U8i *ptr;
};
class CHashTable {
						CKeyValPair **buckets;
						I32i bucketCount;
};
#define KEY_EXISTS 1
I32i KeyValPairSize(CKeyValPair *pairs) {
					if(!pairs) return 0;
					I32i size=1;
					for(CKeyValPair *right=pairs->next;right;right=right->next) size+=1;
					for(CKeyValPair *left=pairs->prev;left;left=left->prev) size+=1;
					return size;
}
U0 KeyValPairDestroy(CKeyValPair *pairs) {
			if(!pairs) return ;
			for(;pairs->prev;pairs=pairs->prev);
			CKeyValPair *next=pairs->next;
			loop:
			next=pairs->next;
			free(pairs->text);
			free(pairs);
			pairs=next;
			if(next) goto loop;
}
U8i **KeyValPairKeys(CKeyValPair *table) {
				I32i size=KeyValPairSize(table);
				U8i **retval=malloc(size*sizeof(U8i*));
				if(!table) return retval;
				while(table->prev) table=table->prev;
				I32i index=-1;
				while(table) {
																	retval[index+=1]=table->text;
																	table=table->next;
				}
				return retval;
}
CKeyValPair *KeyValPairInsert(CKeyValPair *table,U8i *ptr,U8i *text) {
											CKeyValPair *nextItem=NULL;
											if(!table) goto insert;
											I32i hasLen=StrLen(text);

											//Move all the way to the right
											while(table->next) {table=table->next;}

											loop:
											if(table==NULL) goto insert;
											I32i cmp=StrCmp(text,table->text);
											if(cmp==0) {throw(KEY_EXISTS);};
											else if(cmp>0) {
																nextItem=table;
																table=table->prev;
											} else if(cmp<0) {
											     insert:
																CKeyValPair *next=nextItem;
																CKeyValPair new;
																new.prev=table;
																new.next=next;
																new.ptr=ptr;
																new.text=StrDupLen(text,StrLen(text));
																CKeyValPair *New=malloc(sizeof(CKeyValPair));
																*New=new;

																if(table) {table->next=New;}
																if(next) {next->prev=New; }
																return New;
										}
										goto loop;
}
CKeyValPair *KeyValPairRemove(CKeyValPair *table,U8i *text) {
												if(!table) return NULL;
												I32i hasLen=StrLen(text);
												
												//Move all the way to the right
												while(table->next) table=table->next;

												loop:
												I32i curLen=StrLen(table->text);
												I32i min;
												if(curLen>hasLen) min=hasLen; else min=curLen;
												min+=1; //Account for NULL byte
												I32i cmp=StrNCmp(table->text,text,min);
												if(cmp==0)  {
																								CKeyValPair *retval=NULL;
																								if(table->prev) retval=table->prev->next=table->next;
																								if(table->next) retval=table->next->prev=table->prev;
																								free(table->text);
																								free(table);
																								return retval;
												} else if(cmp<0) {
														table=table->prev;
												} else if(cmp>0) return table;
												goto loop;
}
U0 *KeyValPairGet(CKeyValPair *table,U8i *text) {
			if(!table) return NULL;
			I32i hasLen=StrLen(text);
												
			//Move all the way to the right
			while(table->next) table=table->next;

			loop:
			if(!table) return NULL;
			I32i cmp=StrCmp(text,table->text);
			if(cmp==0)  {
															return table->ptr;
			} else if(cmp>0) {
			table=table->prev;
			} else if(cmp<0) return NULL;
			goto loop;
}
CHashTable *HashTableCreate() {
											CHashTable table;
											table.buckets=malloc(8*sizeof(CKeyValPair*));
											table.bucketCount=8;
											I32i c=0;
											for(;c!=8;c++) table.buckets[c]=NULL;
											CHashTable *retval=malloc(sizeof(CHashTable));
											*retval=table;
											return retval;
}
I32i Hash(U8i *str) {
					I32i len=StrLen(str);
					I32i retval=0;
					while((len-=1)>=0) {
																								retval*=31;
																								retval+=*str;
																								str+=1;
					}
					return retval;
} 
U0 HashTableAdd(CHashTable *table,U0 *ptr,U8i *text) {
			I32i bucket=Hash(text)%table->bucketCount;
			table->buckets[bucket]=KeyValPairInsert(table->buckets[bucket],ptr,text);
}
U0 HashTableRemove(CHashTable *table,U8i *text) {
			I32i bucket=Hash(text)%table->bucketCount;
			I32i bucketSize=KeyValPairSize(table->buckets[bucket]); 
			KeyValPairRemove(table->buckets[bucket],text);
			if(bucketSize==1) table->buckets[bucket]=NULL;
}
U0 *HashTableGet(CHashTable *table,U8i *text) {
			I32i bucket=Hash(text)%table->bucketCount;
			return KeyValPairGet(table->buckets[bucket],text);
}
U0 HashTableDestroy(CHashTable *table) {
			for(I32i b=0;b!=table->bucketCount;b+=1) {
												KeyValPairDestroy(table->buckets[b]);
			}
			free(table->buckets);
			free(table);
}

I32i HashTableSize(CHashTable *table) {
				I32i size=0;
				for(I32i c=0;c!=table->bucketCount;c++) {
													size+=KeyValPairSize(table->buckets[c]);
				}
				return size;
}
U8i **HashTableKeys(CHashTable *table) {
				U8i **keys=malloc(sizeof(I32i)*HashTableSize(table));
				I32i i=0;
				for(I32i b=0;b!=table->bucketCount;b++) {
													U8i **tmp=KeyValPairKeys(table->buckets[b]);
													for(I32i c=0;c!=KeyValPairSize(table->buckets[b]);c++,i++) {
																						keys[i]=tmp[c];
													}
													free(tmp);
				}
				return keys;
}