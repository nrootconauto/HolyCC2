#include <IR.h>
#include <str.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
STR_TYPE_DEF(char ,Char);
STR_TYPE_FUNCS(char ,Char);
static strChar ptr2Str(const void* a) {
		long len=sprintf(NULL,"%p",a);
		char buffer[len];
		sprintf(buffer,"%p",a);

		return strCharAppendData(NULL, buffer, strlen(buffer)+1);
}
static graphEdgeIR getNodeByLab(strGraphEdgeP edges,enum IRConnType type) {
		for(long i=0;strGraphEdgePSize(edges);i++)
				if(type==*graphEdgeIRValuePtr(edges[i]))
						return edges[i];

		return NULL;
}
#define STR_FROM_FORMAT(fmt,...) ({ \
						long count=sprintf(NULL,fmt,__VA_ARGS__);								\
						char buffer[count+1]; \
						sprintf(buffer,fmt,__VA_ARGS__);																						\
						strCharAppendData(NULL,buffer,strlen(buffer)+1);						\
})
static strChar intLit2Str(const struct lexerInt *i) {
		strChar retVal=strCharResize(NULL, 8);
		retVal=strCharAppendData(retVal, "INT:", strlen("INT:"));
		
		__auto_type clone=*i;
		//Dump hex
		while(clone.value.uLong!=0) {
				int i=clone.value.uLong&0x0fl;
				clone.value.uLong>>=4;

				const char *digits="0123456789ABCDEF";;
				retVal=strCharAppendItem(retVal, digits[i]);
		}

		return retVal;
}
static strChar hashNode(graphNodeIR node) {
		__auto_type val=graphNodeIRValuePtr(node);

		const char *op=NULL;
		switch(val->type) {
		case IR_ADD:
				op="+";goto binopHash;
		case IR_SUB:
				op="-";goto binopHash;
		case IR_POS:
				op="+";goto unopHash;
		case IR_NEG:
				op="-";goto unopHash;
		case IR_MULT:
				op="*";goto binopHash;
		case IR_DIV:
				op="/";goto binopHash;
		case IR_MOD:
				op="%";goto binopHash;
		case IR_POW:
				op="`";goto binopHash;
		case IR_LAND:
				op="&&";goto binopHash;
		case IR_LXOR:
				op="^^";goto binopHash;
		case IR_LOR:
				op="||";goto binopHash;
		case IR_LNOT:
				op="!";goto unopHash;
		case IR_BAND:
				op="&";goto binopHash;
		case IR_BXOR:
				op="^";goto binopHash;
		case IR_BOR:
				op="|";goto binopHash;
		case IR_BNOT:
				op="~";goto unopHash;
		case IR_LSHIFT:
				op="<<";goto binopHash;
		case IR_RSHIFT:
				op=">>";goto binopHash;
		case IR_ARRAY_ACCESS:
				op="[]";goto binopHash;
		case IR_SIMD:
		case IR_GT:
				op=">";goto binopHash;
		case IR_LT:
				op="<";goto binopHash;
		case IR_GE:
				op=">=";goto binopHash;
		case IR_LE:
				op="<=";goto binopHash;
		case IR_NE:
				op="!=";goto binopHash;
		case IR_EQ:
				op="==";goto binopHash;
		case IR_VALUE: {
				struct IRNodeValue *value=(void*)val;
				switch(value->val.type) {
				case IR_VAL_FUNC:
				case IR_VAL_MEM:
				case IR_VAL_REG:
				case IR_VAL_INDIRECT:
				case IR_VAL_VAR_REF:
				case IR_VAL_INT_LIT:
				case IR_VAL_STR_LIT:
				case __IR_VAL_LABEL:
						return ptr2Str(value->val.value.__label);
				case __IR_VAL_MEM_GLOBAL:
						return STR_FROM_FORMAT("GM[%li:%i]", value->val.value.__frame.offset,value->val.value.__frame.width);
				case __IR_VAL_MEM_FRAME:;
						return ptr2Str(value->val.value.__global.symbol);
				}
		}
		default:
				return NULL;
		}
	unopHash: {
				__auto_type incoming=graphNodeIRIncoming(node);
				assert(strGraphEdgeIRPSize(incoming)==1);
				__auto_type a= getNodeByLab(incoming,IR_CONN_SOURCE_A);
				assert(a);

				__auto_type aHash=hashNode(graphEdgeIRIncoming(a));

				strChar retVal=NULL;
				if(aHash) {
						long len=sprintf(NULL, "%s [%s]", op,aHash);
						char buffer[len];
						sprintf(buffer, "%s [%s]", op,aHash);

						retVal=strCharAppendItem(NULL, strlen(buffer)+1);
				}
				
				strCharDestroy(&aHash);
				strGraphEdgeIRPDestroy(&incoming);

				return retVal;
		}
	binopHash: {
				__auto_type incoming=graphNodeIRIncoming(node);
				assert(strGraphEdgeIRPSize(incoming)==2);
				__auto_type a= getNodeByLab(incoming,IR_CONN_SOURCE_A);
				__auto_type b= getNodeByLab(incoming,IR_CONN_SOURCE_B);
				assert(a&&b);

				__auto_type aHash=hashNode(graphEdgeIRIncoming(a));
				__auto_type bHash=hashNode(graphEdgeIRIncoming(b));

				strChar retVal=NULL;
				if(aHash &&bHash) {
						long len=sprintf(NULL, "%s [%s][%s]", op,aHash, bHash);
						char buffer[len];
						sprintf(buffer, "%s [%s][%s]", op,aHash, bHash);

						retVal=strCharAppendItem(NULL, strlen(buffer)+1);
				}
				strCharDestroy(&aHash);
				strCharDestroy(&bHash);
				strGraphEdgeIRPDestroy(&incoming);
				
				return retVal;
		}
}
