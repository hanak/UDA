#ifndef UDA_CLIENTSERVER_FREEDATABLOCK_H
#define UDA_CLIENTSERVER_FREEDATABLOCK_H

#include <structures/genStructs.h>
#include "udaStructs.h"

// Forward declarations

#if defined(_WIN32)
#  define LIBRARY_API __declspec(dllexport)
#else
#  define LIBRARY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

LIBRARY_API void freeDataBlock(DATA_BLOCK* data_block);
LIBRARY_API void freeReducedDataBlock(DATA_BLOCK* data_block);
LIBRARY_API void freeIdamClientPutDataBlockList(PUTDATA_BLOCK_LIST* putDataBlockList);

#ifdef __cplusplus
}
#endif

#endif // UDA_CLIENTSERVER_FREEDATABLOCK_H

