#ifndef IDAM_READBYTESNONOPTIMALLY_H
#define IDAM_READBYTESNONOPTIMALLY_H

#include <clientserver/udaStructs.h>

int readBytes(DATA_SOURCE data_source, SIGNAL_DESC signal_desc, DATA_BLOCK *data_block, const ENVIRONMENT* environment);

#ifndef NOBINARYPLUGIN

#define BYTEFILEDOESNOTEXIST 	100001
#define BYTEFILEATTRIBUTEERROR 	100002
#define BYTEFILEISNOTREGULAR 	100003 
#define BYTEFILEOPENERROR 	    100004
#define BYTEFILEHEAPERROR 	    100005
#define BYTEFILEMD5ERROR	    100006
#define BYTEFILEMD5DIFF		    100007

#endif

#endif // IDAM_READBYTESNONOPTIMALLY_H
