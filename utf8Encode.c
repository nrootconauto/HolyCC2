#include <stdlib.h>
void utf8Encode(unsigned int codePoint,char *text,int *length) {
 if(codePoint>=0&&codePoint<=0x7f) {
	if(length!=NULL)
	 *length=1;
	if(text!=NULL)
	 *text=0b10000000&codePoint;
 } else if(codePoint>=0x80&&codePoint<=0x7ff) {
	if(length!=NULL)
	 *length=2;
	if(text==NULL)
	 return;
	text[0]=0b00111111&codePoint;
	text[1]=0b00011111&(codePoint>>6);
 } else if(codePoint>=0x800&&codePoint<=0xffff) {
	if(length!=NULL)
	 *length=3;
	if(text==NULL)
	 return;
	text[0]=0b00111111&codePoint;
	text[1]=0b00111111&(codePoint>>6);
	text[2]=0b00001111&(codePoint>>12);
 } else if(codePoint>=0x10000&&codePoint<=0x10ffff) {
	if(length!=NULL)
	 *length=4;
	if(text==NULL)
	 return;
	text[0]=0b00111111&codePoint;
	text[1]=0b00111111&(codePoint>>6);
	text[2]=0b00111111&(codePoint>>12);
	text[3]=0b00000111&(codePoint>>18);
 }
}
