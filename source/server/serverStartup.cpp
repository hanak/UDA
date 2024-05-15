#ifdef _WIN32
#include <windows.h>
#include <Shlobj.h>
#include <algorithm>
#endif

#include "serverStartup.h"

#include <cstdlib>
#include <cerrno>
#include <string>

#include <logging/logging.h>
#include <clientserver/errorLog.h>

#include "getServerEnvironment.h"

int startup()
{
    //----------------------------------------------------------------
    // Read Environment Variable Values (Held in a Global Structure)

    const ENVIRONMENT* environment = getServerEnvironment();

    //---------------------------------------------------------------
    // Open the Log Files

    udaSetLogLevel((LOG_LEVEL)environment->loglevel);

    if (environment->loglevel <= UDA_LOG_ACCESS) {
        #ifdef _WIN32
        std::string logdir(environment->logdir);
        if (!logdir.empty() && logdir != "./") {
            std::replace(logdir.begin(), logdir.end(), '/', '\\');
            if (logdir.back() == '\\') {
                logdir.resize(logdir.length() - 1);
            }
            SHCreateDirectoryExA(NULL, logdir.c_str(), NULL);
            const DWORD lastError = GetLastError();
            const DWORD logdirAttr = GetFileAttributesA(logdir.c_str());
            if (logdirAttr == INVALID_FILE_ATTRIBUTES || !(logdirAttr & FILE_ATTRIBUTE_DIRECTORY)) {
                // No logging facilities are set. Thus, no log will appear anywhere.
                // Therefore, dump information to STDERR.
                fprintf(stderr, "Creating log directory %s failed (last error %u).\n", environment->logdir, lastError);
                UDA_THROW_ERROR(999, "creating log directory failed");
            }
        }
        #else
        char cmd[STRING_LENGTH];
        snprintf(cmd, STRING_LENGTH, "mkdir -p %s 2>/dev/null", environment->logdir);
        if (system(cmd) != 0) {
            UDA_THROW_ERROR(999, "mkdir command failed");
        }
        #endif

        errno = 0;
        std::string log_file = std::string{ environment->logdir } + "Access.log";
        FILE* accout = fopen(log_file.c_str(), environment->logmode);

        if (errno != 0) {
            addIdamError(UDA_SYSTEM_ERROR_TYPE, "startup", errno, "Access Log: ");
            if (accout != nullptr) {
                fclose(accout);
            }
        } else {
            udaSetLogFile(UDA_LOG_ACCESS, accout);
        }
    }

    if (environment->loglevel <= UDA_LOG_ERROR) {
        errno = 0;
        std::string log_file = std::string{ environment->logdir } + "Error.log";
        FILE* errout = fopen(log_file.c_str(), environment->logmode);

        if (errno != 0) {
            addIdamError(UDA_SYSTEM_ERROR_TYPE, "startup", errno, "Error Log: ");
            if (errout != nullptr) {
                fclose(errout);
            }
        } else {
            udaSetLogFile(UDA_LOG_ERROR, errout);
        }
    }

    if (environment->loglevel <= UDA_LOG_WARN) {
        errno = 0;
        std::string log_file = std::string{ environment->logdir } + "DebugServer.log";
        FILE* dbgout = fopen(log_file.c_str(), environment->logmode);

        if (errno != 0) {
            addIdamError(UDA_SYSTEM_ERROR_TYPE, "startup", errno, "Debug Log: ");
            if (dbgout != nullptr) {
                fclose(dbgout);
            }
        } else {
            udaSetLogFile(UDA_LOG_WARN, dbgout);
            udaSetLogFile(UDA_LOG_DEBUG, dbgout);
            udaSetLogFile(UDA_LOG_INFO, dbgout);
        }
    }

    printServerEnvironment(environment);

    return 0;
}
