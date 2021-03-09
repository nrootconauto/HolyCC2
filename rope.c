#include <rope.h>
#include <str.h>
#include <cleanup.h>
#include <stdlib.h>
#define ALLOCATE(x) ({typeof(x)* ptr=calloc(sizeof(x),1);*ptr=x;ptr;})
#define ROPE_LENGTH 4
struct rope {
		struct rope *left,*right,*parent;
		long cCount;
};
struct ropeLeaf {
		struct rope base;
		char text[ROPE_LENGTH+1];
};
STR_TYPE_DEF(struct rope*,Rope);
STR_TYPE_FUNCS(struct rope*,Rope);
long ropeSize(struct rope *r) {
		long weight=r->cCount;
		if(r->left)
				weight+=ropeSize(r->left);
		if(r->right)
				weight+=ropeSize(r->right);
		return weight;
}
void ropeDestroy(struct rope **r) {
		if(r[0]->left)
				ropeDestroy(&r[0]->left);
		if(r[0]->right)
				ropeDestroy(&r[0]->right);
		free(r[0]);
}
static struct rope *__ropeIndex(struct rope *r,long i,long *strI) {
		if(!r->left&&!r->right) {
				if(r->cCount<=i)
						return NULL;
				if(strI)
						*strI=i;
				return r;
		}
		if(ropeSize(r->left)<=i&&r->right) {
				return __ropeIndex(r->right,i-ropeSize(r->left),strI);
		} else if(r->left) {
				return __ropeIndex(r->left,i,strI);
		}
		return NULL;
}
static void __ropeDisconnect(struct rope *r) {
		if(r->parent) {
				if(r->parent->left==r)
						r->parent->left=NULL;
				if(r->parent->right==r)
						r->parent->right=NULL;
		}
		r->parent=NULL;
}
static struct rope *__ropeMasterPar(struct rope *a) {
		while(a->parent)
				a=a->parent;
		return a;
}
struct rope *ropeConcat(struct rope *a,struct rope *b) {
		if(!a)
				return b;
		a=__ropeMasterPar(a);
		b=__ropeMasterPar(b);
		
		long aSize=ropeSize(a);
		//Merge text on first of a and last of b to save space
		if(aSize) {
				__auto_type bFirstNode=(struct ropeLeaf*)__ropeIndex(b, 0, NULL);
				__auto_type aLastNode=(struct ropeLeaf*)__ropeIndex(a, aSize-1, NULL);
				if(bFirstNode) {
						long aExtra=ROPE_LENGTH-aLastNode->base.cCount;
						long bSize=bFirstNode->base.cCount;
						long toMove=(aExtra<bSize)?aExtra:bSize;
						strncpy(aLastNode->text+aLastNode->base.cCount, bFirstNode->text, toMove);
						aLastNode->base.cCount+=toMove;
						memmove(bFirstNode->text, bFirstNode->text+toMove, bFirstNode->base.cCount-toMove);
						bFirstNode->base.cCount-=toMove;

						if(!bFirstNode->base.cCount) {
								__auto_type bPar=b->parent;
								__ropeDisconnect(b);
								free(b);
								if(!bPar)
										return a;
								b=__ropeMasterPar(bPar);
						}
				}
		}
		
		struct rope join;
		join.cCount=0;
		join.left=a;
		join.right=b;
		join.parent=NULL;
		__auto_type retVal=ALLOCATE(join);
		
		if(a)
				a->parent=retVal;
		if(b)
				b->parent=retVal;
		return retVal;
}
struct rope *ropeFromText(const char *text) {
		long len=strlen(text);
		struct rope *retVal=NULL;
		for(long c=0;c!=len;) {
				struct ropeLeaf leaf;
				leaf.base.cCount=ROPE_LENGTH;
				leaf.base.left=leaf.base.right=leaf.base.parent=NULL;
				if(len-c>=ROPE_LENGTH) {
						strncpy(leaf.text,text+c,ROPE_LENGTH);
						leaf.text[ROPE_LENGTH]='\0';
						c+=ROPE_LENGTH;
						leaf.base.cCount=ROPE_LENGTH;
				} else {
						strcpy(leaf.text,text+c);
						leaf.base.cCount=len-c;
						c=len;
				}
				retVal=ropeConcat(retVal, (struct rope*)ALLOCATE(leaf));
		}

		return retVal;
}
void ropeSplit(struct rope *r,long i,struct rope **a,struct rope **b) {
		if(ropeSize(r)==i) {
				if(a)
						*a=r;
				if(b) {
						struct rope empty;
						empty.cCount=0;
						empty.left=0;
						empty.left=empty.parent=empty.right=NULL;
						*b=ALLOCATE(empty);
				}
				return;
		}

		long strIndex;
		__auto_type splitNode=(struct ropeLeaf*)__ropeIndex(r,i,&strIndex);
		
		strRope disconnectedNodes CLEANUP(strRopeDestroy)=NULL;
		if(strIndex==0) {
				disconnectedNodes=strRopeAppendItem(disconnectedNodes, (struct rope*)splitNode);
				__ropeDisconnect((struct rope*)splitNode);
		} else {
				splitNode->text[splitNode->base.cCount]='\0';
				
				disconnectedNodes=strRopeAppendItem(disconnectedNodes, ropeFromText(splitNode->text+strIndex));

				splitNode->text[strIndex]='\0';
				splitNode->base.cCount=strIndex;
		}
		
		for(;;) {
				__auto_type curr=__ropeIndex(r, i, NULL);
				if(!curr)
						break;
				__ropeDisconnect(curr);
				disconnectedNodes=strRopeAppendItem(disconnectedNodes, curr);
		}

		struct rope *retVal=disconnectedNodes[0];
		for(long r=1;r!=strRopeSize(disconnectedNodes);r++)
				retVal=ropeConcat(retVal, disconnectedNodes[r]);

		if(b)
				*b=retVal;
		if(a)
				*a=r;
}
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
static strChar __ropeToText(struct rope *r) {
		if(r->cCount)
				return strCharAppendData(NULL, ((struct ropeLeaf*)r)->text, r->cCount);
		strChar retVal=NULL;
		if(r->left)
				retVal=__ropeToText(r->left);
		if(r->right)
				retVal=strCharConcat(retVal,__ropeToText(r->right));
		return retVal;
}
char *ropeToText(struct rope *r) {
		strChar text CLEANUP(strCharDestroy)=__ropeToText(r);
		long len=ropeSize(r);
		char *retVal=strncpy(calloc(len+1,1), text,len);
		retVal[len]='\0';
		return retVal;
}
struct rope *ropeInsertText(struct rope *r,const char *text,long i) {
		struct rope *a=r,*b;
		ropeSplit(r, i, &a, &b);
		return ropeConcat(ropeConcat(a, ropeFromText(text)),b);
}
struct rope *ropeDeleteText(struct rope *r,long s,long e) {
		struct rope *a=r,*b,*c;
		ropeSplit(r, s, &a, &b);
		ropeSplit(b, e-s, &b, &c);
		ropeDestroy(&b);
		return ropeConcat(a, c);
}
char *ropeSliceText(struct rope *r,long s,long e) {
		struct rope *a=r,*b,*c;
		ropeSplit(r, s, &a, &b);
		ropeSplit(b, e-s, &b, &c);
		char *retVal=ropeToText(b);
		a=ropeConcat(a, b);
		b=ropeConcat(a, c);
		
		//Reconnect to pointer r
		*r=*a;
		if(r->left)
		r->left->parent=r;
		if(r->right)
		r->right->parent=r;
		
		return retVal;
}
