class CKeyValPair {
						CKeyValPair *prev,*next;
						U8i *text;
						U8i *ptr;
};
class CHashTable {
						CKeyValPair *buckets;
						I32i count;
};
#define KEY_EXISTS 1
I32i KeyValPairSize(CKeyValPair *pairs) {
					if(!pairs) return 0;
					I32i size=1;
					for(CKeyValPair *right=pairs->next;right;right=right->next) size+=1;
					for(CKeyValPair *left=pairs->prev;left;left=left->prev) size+=1;
					return size;
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
											while(table->next) table=table->next;

											loop:
											if(table==NULL) goto insert;
											I32i curLen=StrLen(table->text);
											I32i min;
											if(curLen>hasLen) min=hasLen; else min=curLen;
											min+=1; //Account for NULL byte
											I32i cmp=StrNCmp(table->text,text,min);
											if(cmp==0) throw(KEY_EXISTS);
											else if(cmp<0) {
																nextItem=table;
																table=table->prev;
											} else if(cmp>0) {
											     insert:
																CKeyValPair *next=nextItem;
																CKeyValPair new;
																new.prev=table;
																new.next=next;
																new.ptr=ptr;
																new.text=StrDupLen(text,StrLen(text));
																CKeyValPair *New=malloc(sizeof(CKeyValPair));
																*New=new;

																if(table) table->next=New;
																if(next) next->prev=New;
																return New;
										}
										goto loop;
}
U0 KeyValPairRemove(CKeyValPair *table,U8i *text) {
												if(!table) return;
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
																								if(table->prev) table->prev->next=table->next;
																								if(table->next) table->next->prev=table->prev;
																								free(table->text);
																								free(table);
																								return ;
												} else if(cmp<0) {
														table=table->prev;
												} else if(cmp>0) return;
												goto loop;
}
U0 *KeyValPairGet(CKeyValPair *table,U8i *text) {
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
															return table->ptr;
			} else if(cmp<0) {
			table=table->prev;
			} else if(cmp>0) return NULL;
			goto loop;
}
I32i CHashTableSize(CHashTable *tab) {
					
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