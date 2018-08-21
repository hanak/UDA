#include "pluginUtils.h"

#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <strings.h>

#include <cache/cache.h>
#include <client/udaClient.h>
#include <clientserver/expand_path.h>
#include <clientserver/freeDataBlock.h>
#include <clientserver/initStructs.h>
#include <clientserver/printStructs.h>
#include <clientserver/protocol.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaErrors.h>
#include <structures/struct.h>
#include <clientserver/makeRequestBlock.h>
#include <clientserver/makeRequestBlock.h>

#define REQUEST_READ_START      1000
#define REQUEST_PLUGIN_MCOUNT   100    // Maximum initial number of plugins that can be registered
#define REQUEST_PLUGIN_MSTEP    10    // Increase heap by 10 records once the maximum is exceeded

int initPlugin(const IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    idamSetLogLevel((LOG_LEVEL)plugin_interface->environment->loglevel);

    return 0;
}

int setReturnDataFloatArray(DATA_BLOCK* data_block, float* values, size_t rank, const size_t* shape,
                            const char* description)
{
    initDataBlock(data_block);

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = (int)rank;
    data_block->dims = malloc(rank * sizeof(DIMS));

    size_t len = 1;

    size_t i;
    for (i = 0; i < rank; ++i) {
        initDimBlock(&data_block->dims[i]);

        data_block->dims[i].data_type = UDA_TYPE_UNSIGNED_INT;
        data_block->dims[i].dim_n = (int)shape[i];
        data_block->dims[i].compressed = 1;
        data_block->dims[i].dim0 = 0.0;
        data_block->dims[i].diff = 1.0;
        data_block->dims[i].method = 0;

        len *= shape[i];
    }

    float* data = malloc(len * sizeof(float));
    memcpy(data, values, len * sizeof(float));

    data_block->data_type = UDA_TYPE_FLOAT;
    data_block->data = (char*)data;
    data_block->data_n = (int)len;

    return 0;
}

int setReturnDataDoubleArray(DATA_BLOCK* data_block, double* values, size_t rank, const size_t* shape,
                             const char* description)
{
    initDataBlock(data_block);

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = (int)rank;
    data_block->dims = malloc(rank * sizeof(DIMS));

    size_t len = 1;

    size_t i;
    for (i = 0; i < rank; ++i) {
        initDimBlock(&data_block->dims[i]);

        data_block->dims[i].data_type = UDA_TYPE_UNSIGNED_INT;
        data_block->dims[i].dim_n = (int)shape[i];
        data_block->dims[i].compressed = 1;
        data_block->dims[i].dim0 = 0.0;
        data_block->dims[i].diff = 1.0;
        data_block->dims[i].method = 0;

        len *= shape[i];
    }

    double* data = malloc(len * sizeof(double));
    memcpy(data, values, len * sizeof(double));

    data_block->data_type = UDA_TYPE_DOUBLE;
    data_block->data = (char*)data;
    data_block->data_n = (int)len;

    return 0;
}

int
setReturnDataIntArray(DATA_BLOCK* data_block, int* values, size_t rank, const size_t* shape, const char* description)
{
    initDataBlock(data_block);

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = (int)rank;
    data_block->dims = malloc(rank * sizeof(DIMS));

    size_t len = 1;

    size_t i;
    for (i = 0; i < rank; ++i) {
        initDimBlock(&data_block->dims[i]);

        data_block->dims[i].data_type = UDA_TYPE_UNSIGNED_INT;
        data_block->dims[i].dim_n = (int)shape[i];
        data_block->dims[i].compressed = 1;
        data_block->dims[i].dim0 = 0.0;
        data_block->dims[i].diff = 1.0;
        data_block->dims[i].method = 0;

        len *= shape[i];
    }

    int* data = malloc(len * sizeof(int));
    memcpy(data, values, len * sizeof(int));

    data_block->data_type = UDA_TYPE_INT;
    data_block->data = (char*)data;
    data_block->data_n = (int)len;

    return 0;
}

int setReturnDataDoubleScalar(DATA_BLOCK* data_block, double value, const char* description)
{
    initDataBlock(data_block);

    double* data = (double*)malloc(sizeof(double));
    data[0] = value;

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_DOUBLE;
    data_block->data = (char*)data;
    data_block->data_n = 1;

    return 0;
}

int setReturnDataFloatScalar(DATA_BLOCK* data_block, float value, const char* description)
{
    initDataBlock(data_block);

    float* data = (float*)malloc(sizeof(float));
    data[0] = value;

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_FLOAT;
    data_block->data = (char*)data;
    data_block->data_n = 1;

    return 0;
}

int setReturnDataIntScalar(DATA_BLOCK* data_block, int value, const char* description)
{
    initDataBlock(data_block);

    int* data = (int*)malloc(sizeof(int));
    data[0] = value;

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_INT;
    data_block->data = (char*)data;
    data_block->data_n = 1;

    return 0;
}

int setReturnDataLongScalar(DATA_BLOCK* data_block, long value, const char* description)
{
    initDataBlock(data_block);

    long* data = (long*)malloc(sizeof(long));
    data[0] = value;

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_LONG;
    data_block->data = (char*)data;
    data_block->data_n = 1;

    return 0;
}

int setReturnDataShortScalar(DATA_BLOCK* data_block, short value, const char* description)
{
    initDataBlock(data_block);

    short* data = (short*)malloc(sizeof(short));
    data[0] = value;

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_SHORT;
    data_block->data = (char*)data;
    data_block->data_n = 1;

    return 0;
}

int setReturnDataString(DATA_BLOCK* data_block, const char* value, const char* description)
{
    initDataBlock(data_block);

    data_block->data_type = UDA_TYPE_STRING;
    data_block->data = strdup(value);

    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

    unsigned int i;
    for (i = 0; i < data_block->rank; i++) {
        initDimBlock(&data_block->dims[i]);
    }

    if (description != NULL) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;
    data_block->dims[0].dim_n = (int)strlen(value) + 1;
    data_block->dims[0].compressed = 1;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    data_block->dims[0].method = 0;

    data_block->data_n = data_block->dims[0].dim_n;

    return 0;
}

void allocPluginList(int count, PLUGINLIST* plugin_list)
{
    if (count >= plugin_list->mcount) {
        plugin_list->mcount = plugin_list->mcount + REQUEST_PLUGIN_MSTEP;
        plugin_list->plugin = (PLUGIN_DATA*)realloc((void*)plugin_list->plugin,
                                                    plugin_list->mcount * sizeof(PLUGIN_DATA));
    }
}

void closePluginList(const PLUGINLIST* plugin_list)
{
    int i;
    REQUEST_BLOCK request_block;
    IDAM_PLUGIN_INTERFACE idam_plugin_interface;
    initRequestBlock(&request_block);
    strcpy(request_block.function, "reset");

    idam_plugin_interface.interfaceVersion = 1;
    idam_plugin_interface.housekeeping = 1;            // Force a full reset
    idam_plugin_interface.request_block = &request_block;
    for (i = 0; i < plugin_list->count; i++) {
        if (plugin_list->plugin[i].pluginHandle != NULL) {
            plugin_list->plugin[i].idamPlugin(&idam_plugin_interface);        // Call the housekeeping method
        }
    }
}

void freePluginList(PLUGINLIST* plugin_list)
{
    int i;
    closePluginList(plugin_list);
    for (i = 0; i < plugin_list->count; i++) {
        if (plugin_list->plugin[i].pluginHandle != NULL) {
            dlclose(plugin_list->plugin[i].pluginHandle);
        }
    }
    free((void*)plugin_list->plugin);
    plugin_list->plugin = NULL;
    plugin_list->count = 0;
    plugin_list->mcount = 0;
}

void initPluginData(PLUGIN_DATA* plugin)
{
    plugin->format[0] = '\0';
    plugin->library[0] = '\0';
    plugin->symbol[0] = '\0';
    plugin->extension[0] = '\0';
    plugin->desc[0] = '\0';
    plugin->example[0] = '\0';
    plugin->method[0] = '\0';
    plugin->deviceProtocol[0] = '\0';
    plugin->deviceHost[0] = '\0';
    plugin->devicePort[0] = '\0';
    plugin->request = REQUEST_READ_UNKNOWN;
    plugin->plugin_class = PLUGINUNKNOWN;
    plugin->external = PLUGINNOTEXTERNAL;
    plugin->status = PLUGINNOTOPERATIONAL;
    plugin->is_private = PLUGINPRIVATE;                    // All services are private: Not accessible to external users
    plugin->cachePermission = PLUGINCACHEDEFAULT;       // Data are OK or Not for the Client to Cache
    plugin->interfaceVersion = 1;                       // Maximum Interface Version
    plugin->pluginHandle = NULL;
    plugin->idamPlugin = NULL;
}

void printPluginList(FILE* fd, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        fprintf(fd, "Request    : %d\n", plugin_list->plugin[i].request);
        fprintf(fd, "Format     : %s\n", plugin_list->plugin[i].format);
        fprintf(fd, "Library    : %s\n", plugin_list->plugin[i].library);
        fprintf(fd, "Symbol     : %s\n", plugin_list->plugin[i].symbol);
        fprintf(fd, "Extension  : %s\n", plugin_list->plugin[i].extension);
        fprintf(fd, "Method     : %s\n", plugin_list->plugin[i].method);
        fprintf(fd, "Description: %s\n", plugin_list->plugin[i].desc);
        fprintf(fd, "Example    : %s\n", plugin_list->plugin[i].example);
        fprintf(fd, "Protocol   : %s\n", plugin_list->plugin[i].deviceProtocol);
        fprintf(fd, "Host       : %s\n", plugin_list->plugin[i].deviceHost);
        fprintf(fd, "Port       : %s\n", plugin_list->plugin[i].devicePort);
        fprintf(fd, "Class      : %d\n", plugin_list->plugin[i].plugin_class);
        fprintf(fd, "External   : %d\n", plugin_list->plugin[i].external);
        fprintf(fd, "Status     : %d\n", plugin_list->plugin[i].status);
        fprintf(fd, "Private    : %d\n", plugin_list->plugin[i].is_private);
        fprintf(fd, "cachePermission : %d\n", plugin_list->plugin[i].cachePermission);
        fprintf(fd, "interfaceVersion: %d\n\n", plugin_list->plugin[i].interfaceVersion);
    }
}

/**
 * Find the Plugin identity: return the reference id or -1 if not found.
 * @param request
 * @param plugin_list
 * @return
 */
int findPluginIdByRequest(int request, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        if (plugin_list->plugin[i].request == request) return i;
    }
    return -1;
}

/**
 * Find the Plugin identity: return the reference id or -1 if not found.
 * @param format
 * @param plugin_list
 * @return
 */
int findPluginIdByFormat(const char* format, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        if (STR_IEQUALS(plugin_list->plugin[i].format, format)) return i;
    }
    return -1;
}

/**
 * Find the Plugin identity: return the reference id or -1 if not found.
 * @param device
 * @param plugin_list
 * @return
 */
int findPluginIdByDevice(const char* device, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        if (plugin_list->plugin[i].plugin_class == PLUGINDEVICE && STR_IEQUALS(plugin_list->plugin[i].format, device)) {
            return i;
        }
    }
    return -1;
}

/**
 * Find the Plugin Request: return the request or REQUEST_READ_UNKNOWN if not found.
 * @param format
 * @param plugin_list
 * @return
 */
int findPluginRequestByFormat(const char* format, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        if (STR_IEQUALS(plugin_list->plugin[i].format, format)) return plugin_list->plugin[i].request;
    }
    return REQUEST_READ_UNKNOWN;
}

/**
 * Find the Plugin Request: return the request or REQUEST_READ_UNKNOWN if not found.
 * @param extension
 * @param plugin_list
 * @return
 */
int findPluginRequestByExtension(const char* extension, const PLUGINLIST* plugin_list)
{
    int i;
    for (i = 0; i < plugin_list->count; i++) {
        if (STR_IEQUALS(plugin_list->plugin[i].extension, extension)) return plugin_list->plugin[i].request;
    }
    return REQUEST_READ_UNKNOWN;
}

int idamServerRedirectStdStreams(int reset)
{
    // Any OS messages will corrupt xdr streams so re-divert IO from plugin libraries to a temporary file

    static FILE* originalStdFH = NULL;
    static FILE* originalErrFH = NULL;
    static FILE* mdsmsgFH = NULL;

    char* env;
    static char tempFile[MAXPATH];

    static int singleFile = 0;

    if (!reset) {
        if (!singleFile) {
            env = getenv("UDA_PLUGIN_DEBUG_SINGLEFILE");        // Use a single file for all plugin data requests
            if (env != NULL) singleFile = 1;                    // Define IDAM_PLUGIN_DEBUG to retain the file
        }

        if (mdsmsgFH != NULL && singleFile) {
            stdout = mdsmsgFH;                                  // Redirect all IO to a temporary file
            stderr = mdsmsgFH;
            return 0;
        }

        originalStdFH = stdout;                                 // Retain current values
        originalErrFH = stderr;
        mdsmsgFH = NULL;

        UDA_LOG(UDA_LOG_DEBUG, "Redirect standard output to temporary file\n");

        env = getenv("UDA_PLUGIN_REDIVERT");

        if (env == NULL) {
            if ((env = getenv("UDA_WORK_DIR")) != NULL) {
                sprintf(tempFile, "%s/idamPLUGINXXXXXX", env);
            } else {
                strcpy(tempFile, "/tmp/idamPLUGINXXXXXX");
            }
        } else {
            strcpy(tempFile, env);
        }

        // Open the message Trap

        errno = 0;
        int fd = mkstemp(tempFile);
        if (fd < 0 || errno != 0) {
            int err = (errno != 0) ? errno : 994;
            THROW_ERROR(err, "Unable to Obtain a Temporary File Name");
        }

        mdsmsgFH = fdopen(fd, "a");

        if (mdsmsgFH == NULL || errno != 0) {
            THROW_ERROR(999, "Unable to Trap Plugin Error Messages.");
        }

        stdout = mdsmsgFH; // Redirect to a temporary file
        stderr = mdsmsgFH;
    } else {
        if (mdsmsgFH != NULL) {
            UDA_LOG(UDA_LOG_DEBUG, "Resetting original file handles and removing temporary file\n");

            if (!singleFile) {
                if (mdsmsgFH != NULL) {
                    errno = 0;
                    int rc = fclose(mdsmsgFH);
                    if (rc) {
                        int err = errno;
                        THROW_ERROR(err, strerror(err));
                    }
                }
                mdsmsgFH = NULL;
                if (getenv("UDA_PLUGIN_DEBUG") == NULL) {
                    errno = 0;
                    int rc = remove(tempFile);    // Delete the temporary file
                    if (rc) {
                        int err = errno;
                        THROW_ERROR(err, strerror(err));
                    }
                    tempFile[0] = '\0';
                }
            }

            stdout = originalStdFH;
            stderr = originalErrFH;

        } else {

            UDA_LOG(UDA_LOG_DEBUG, "Resetting original file handles\n");

            stdout = originalStdFH;
            stderr = originalErrFH;
        }
    }

    return 0;
}

/**
 * Look for an argument with the given name in the provided NAMEVALUELIST and return it's associated value.
 *
 * If the argument is found the value associated with the argument is provided via the value parameter and the function
 * returns 1. Otherwise value is set to NULL and the function returns 0.
 * @param namevaluelist
 * @param value
 * @param name
 * @return
 */
bool findStringValue(const NAMEVALUELIST* namevaluelist, const char** value, const char* name)
{
    char** names = SplitString(name, "|");
    *value = NULL;

    bool found = 0;
    int i;
    for (i = 0; i < namevaluelist->pairCount; i++) {
        size_t n;
        for (n = 0; names[n] != NULL; ++n) {
            if (STR_IEQUALS(namevaluelist->nameValue[i].name, names[n])) {
                *value = namevaluelist->nameValue[i].value;
                found = 1;
                break;
            }
        }
    }

    FreeSplitStringTokens(&names);
    return found;
}

/**
 * Look for an argument with the given name in the provided NAMEVALUELIST and return it's associate value as an integer.
 *
 * If the argument is found the value associated with the argument is provided via the value parameter and the function
 * returns 1. Otherwise value is not set and the function returns 0.
 * @param namevaluelist
 * @param value
 * @param name
 * @return
 */
bool findIntValue(const NAMEVALUELIST* namevaluelist, int* value, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        *value = atoi(str);
    }
    return found;
}

/**
 * Look for an argument with the given name in the provided NAMEVALUELIST and return it's associate value as a short.
 *
 * If the argument is found the value associated with the argument is provided via the value parameter and the function
 * returns 1. Otherwise value is not set and the function returns 0.
 * @param namevaluelist
 * @param value
 * @param name
 * @return
 */
bool findShortValue(const NAMEVALUELIST* namevaluelist, short* value, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        *value = (short)atoi(str);
    }
    return found;
}

/**
 * Look for an argument with the given name in the provided NAMEVALUELIST and return it's associate value as a short.
 *
 * If the argument is found the value associated with the argument is provided via the value parameter and the function
 * returns 1. Otherwise value is not set and the function returns 0.
 * @param namevaluelist
 * @param value
 * @param name
 * @return
 */
bool findCharValue(const NAMEVALUELIST* namevaluelist, char* value, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        *value = (char)atoi(str);
    }
    return found;
}

/**
 * Look for an argument with the given name in the provided NAMEVALUELIST and return it's associate value as a float.
 *
 * If the argument is found the value associated with the argument is provided via the value parameter and the function
 * returns 1. Otherwise value is not set and the function returns 0.
 * @param namevaluelist
 * @param value
 * @param name
 * @return
 */
bool findFloatValue(const NAMEVALUELIST* namevaluelist, float* value, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        *value = strtof(str, NULL);
    }
    return found;
}

bool findIntArray(const NAMEVALUELIST* namevaluelist, int** values, size_t* nvalues, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        char** tokens = SplitString(str, ";");
        size_t n;
        size_t num_tokens = 0;
        for (n = 0; tokens[n] != NULL; ++n) {
            ++num_tokens;
        }
        *values = calloc(num_tokens, sizeof(int));
        for (n = 0; tokens[n] != NULL; ++n) {
            (*values)[n] = (int)strtol(tokens[n], NULL, 10);
        }
        FreeSplitStringTokens(&tokens);
        *nvalues = num_tokens;
    }
    return found;
}

bool findFloatArray(const NAMEVALUELIST* namevaluelist, float** values, size_t* nvalues, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        char** tokens = SplitString(str, ";");
        size_t n;
        size_t num_tokens = 0;
        for (n = 0; tokens[n] != NULL; ++n) {
            ++num_tokens;
        }
        *values = calloc(num_tokens, sizeof(float));
        for (n = 0; tokens[n] != NULL; ++n) {
            (*values)[n] = strtof(tokens[n], NULL);
        }
        FreeSplitStringTokens(&tokens);
        *nvalues = num_tokens;
    }
    return found;
}

bool findDoubleArray(const NAMEVALUELIST* namevaluelist, double** values, size_t* nvalues, const char* name)
{
    const char* str;
    bool found = findStringValue(namevaluelist, &str, name);
    if (found) {
        char** tokens = SplitString(str, ";");
        size_t n;
        size_t num_tokens = 0;
        for (n = 0; tokens[n] != NULL; ++n) {
            ++num_tokens;
        }
        *values = calloc(num_tokens, sizeof(double));
        for (n = 0; tokens[n] != NULL; ++n) {
            (*values)[n] = strtod(tokens[n], NULL);
        }
        FreeSplitStringTokens(&tokens);
        *nvalues = num_tokens;
    }
    return found;
}

bool findValue(const NAMEVALUELIST* namevaluelist, const char* name)
{
    char** names = SplitString(name, "|");

    bool found = false;
    int i;
    for (i = 0; i < namevaluelist->pairCount; i++) {
        size_t n = 0;
        while (names[n] != NULL) {
            if (STR_IEQUALS(namevaluelist->nameValue[i].name, names[n])) {
                found = 1;
                break;
            }
            ++n;
        }
    }

    FreeSplitStringTokens(&names);
    return found;
}

int callPlugin(const PLUGINLIST* pluginlist, const char* request, const IDAM_PLUGIN_INTERFACE* old_plugin_interface)
{
    IDAM_PLUGIN_INTERFACE idam_plugin_interface = *old_plugin_interface;
    REQUEST_BLOCK request_block = *old_plugin_interface->request_block;
    idam_plugin_interface.request_block = &request_block;

    request_block.source[0] = '\0';
    strcpy(request_block.signal, request);
    makeRequestBlock(&request_block, *pluginlist, old_plugin_interface->environment);

    request_block.request = findPluginRequestByFormat(request_block.format, pluginlist);

    if (request_block.request < 0) {
        RAISE_PLUGIN_ERROR("Plugin not found!");
    }

    int err = 0;
    int id = findPluginIdByRequest(request_block.request, pluginlist);
    PLUGIN_DATA* plugin = &(pluginlist->plugin[id]);
    if (id >= 0 && plugin->idamPlugin != NULL) {
        err = plugin->idamPlugin(&idam_plugin_interface);    // Call the data reader
    } else {
        RAISE_PLUGIN_ERROR("Data Access is not available for this data request!");
    }

    return err;
}