/*---------------------------------------------------------------
  Change the Status of an Open Provenance UUID registered record
 
  UUID_Register table contains the following
  
  UUID		Unique Identifier: Year+'/'+Ordinal Date+'/'+Issue Sequence
  Owner	name of the user or group that owns the UUID
  Class 
  Title 
  Description	
  icatRef	ICAT reference Id (foreign key)
  status	UUID status [open, closed, delete | firm | pending]
  creation	Date of registration
*---------------------------------------------------------------------------------------------------------------*/
#include "status.h"

#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#include <clientserver/initStructs.h>
#include <structures/struct.h>
#include <structures/accessors.h>
#include <clientserver/stringUtils.h>

#include "provenance.h"

int status(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{

    int err = 0;
    int i, offset = 0;

    char sql[MAXSQL];
    int stringLength;

//----------------------------------------------------------------------------------------
// Standard v1 Plugin Interface

    DATA_BLOCK* data_block;
    REQUEST_BLOCK* request_block;

    USERDEFINEDTYPE usertype;
    COMPOUNDFIELD field;

    static PGconn* DBConnect = NULL;

    PGresult* DBQuery = NULL;

    if (idam_plugin_interface->interfaceVersion == 1) {

        idam_plugin_interface->pluginVersion = 1;

        data_block = idam_plugin_interface->data_block;
        request_block = idam_plugin_interface->request_block;

        DBConnect = (PGconn*) idam_plugin_interface->sqlConnection;

    } else {
        err = 999;
        IDAM_LOG(UDA_LOG_ERROR, "ERROR Provenance: Plugin Interface Version Unknown\n");

        addIdamError(&idamerrorstack, CODEERRORTYPE, "Provenance", err,
                     "Plugin Interface Version is Not Known: Unable to execute the request!");
        return err;
    }

    IDAM_LOG(UDA_LOG_DEBUG, "Provenance: Plugin Interface transferred\n");

//----------------------------------------------------------------------------------------
// Common Name Value pairs

// Keywords have higher priority

//----------------------------------------------------------------------------------------
// Error trap

    err = 0;

    do {

        IDAM_LOG(UDA_LOG_DEBUG, "Provenance: entering function 'status'\n");

// specific Name Value pairs (Keywords have higher priority)

        unsigned short isUUID = 0, isStatus = 0, isReturnStatus = 0;
        char* uuid = NULL;
        char status;

        for (i = 0; i < request_block->nameValueList.pairCount; i++) {
            IDAM_LOGF(UDA_LOG_DEBUG, "[%d] %s = %s\n", i, request_block->nameValueList.nameValue[i].name,
                    request_block->nameValueList.nameValue[i].value);

            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "uuid") ||
                STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "uid") ||
                STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "doi")) {
                uuid = request_block->nameValueList.nameValue[i].value;
                isUUID = 1;
                continue;
            }
            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "status")) {
                preventSQLInjection(DBConnect, &request_block->nameValueList.nameValue[i].value, 1);
                status = toupper(request_block->nameValueList.nameValue[i].value[0]);
                isStatus = 1;
                continue;
            }
            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "returnStatus")) {
                isReturnStatus = 1;
                continue;
            }
        }

        if (!isUUID) {
            err = 999;
            IDAM_LOG(UDA_LOG_ERROR, "ERROR Provenance status: Requires a uuid!\n");
            addIdamError(&idamerrorstack, CODEERRORTYPE, "Provenance status", err,
                         "Requires both the uuid and the Status!");
            break;
        }

// Put a new status to an Open record 

        if (isStatus) {

            sprintf(sql, "BEGIN; "
                            "UPDATE uuid_register SET status = '%c' WHERE uuid='%s' AND status = 'O'; "
                            "END; ",
                    status, uuid);

            IDAM_LOGF(UDA_LOG_DEBUG, "%s\n", sql);

// execute

            if ((DBQuery = PQexec(DBConnect, sql)) == NULL || PQresultStatus(DBQuery) != PGRES_COMMAND_OK) {
                err = 999;
                addIdamError(&idamerrorstack, CODEERRORTYPE, "Provenance", err, "SQL Execution Failed!");
                IDAM_LOG(UDA_LOG_ERROR, "ERROR Provenance status: SQL Execution Failed\n");
                PQclear(DBQuery);
                break;
            }

            PQclear(DBQuery);

            IDAM_LOG(UDA_LOG_DEBUG, "Provenance: exiting function status\n");

            initDataBlock(data_block);

            data_block->data = (char*) malloc(sizeof(char));
            data_block->data[0] = status;

// Pass the Data back	 

            data_block->data_type = TYPE_CHAR;
            data_block->rank = 0;
            data_block->data_n = 1;
            strcpy(data_block->data_desc, "Nothing to return!");

            break;

        } else {

// Get the current Status      

            sprintf(sql, "SELECT uuid, owner, class, title, description, icatref, status, creation "
                    "FROM uuid_register WHERE uuid = '%s';", uuid);

            IDAM_LOGF(UDA_LOG_DEBUG, "%s\n", sql);

// Execute

            if ((DBQuery = PQexec(DBConnect, sql)) == NULL || PQresultStatus(DBQuery) != PGRES_TUPLES_OK) {
                err = 999;
                addIdamError(&idamerrorstack, CODEERRORTYPE, "Provenance", err, "SQL Execution Failed!");
                IDAM_LOG(UDA_LOG_ERROR, "ERROR Provenance status: SQL Execution Failed\n");
                PQclear(DBQuery);
                break;
            }

// Write Return Structure

            int nrows = PQntuples(DBQuery);

            if (nrows != 1) {
                IDAM_LOG(UDA_LOG_ERROR, "ERROR Provenance status: A UUID record could not be found!\n");
                err = 999;
                addIdamError(&idamerrorstack, CODEERRORTYPE, "Provenance status", err,
                             "A UUID record could not be found!");
                PQclear(DBQuery);
                break;
            }

            if (isReturnStatus) {

                data_block->data = (char*) malloc(2 * sizeof(char));
                data_block->data[0] = PQgetvalue(DBQuery, 0, 6)[0];
                data_block->data[1] = '\0';

                PQclear(DBQuery);

// Pass the Data back	 

                data_block->data_type = TYPE_STRING;
                data_block->rank = 0;
                data_block->data_n = 2;
                strcpy(data_block->data_desc, "UUID Status");

                IDAM_LOGF(UDA_LOG_DEBUG, "Provenance uuid: %s\n", uuid);
                IDAM_LOGF(UDA_LOG_DEBUG, "status         : %s\n", data_block->data);
            } else {

// Create the Data Structure to be returned	 

                PROVENANCEUUID* data = NULL;
                data = (PROVENANCEUUID*) malloc(sizeof(PROVENANCEUUID));    // Structured Data Must be a heap variable

                stringLength = strlen(PQgetvalue(DBQuery, 0, 0)) + 1;
                data->uuid = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->uuid, PQgetvalue(DBQuery, 0, 0));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->uuid, 1, stringLength * sizeof(char), "char");

                stringLength = strlen(PQgetvalue(DBQuery, 0, 1)) + 1;
                data->owner = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->owner, PQgetvalue(DBQuery, 0, 1));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->owner, 1, stringLength * sizeof(char), "char");

                stringLength = strlen(PQgetvalue(DBQuery, 0, 2)) + 1;
                data->class = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->class, PQgetvalue(DBQuery, 0, 2));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->class, 1, stringLength * sizeof(char), "char");

                stringLength = strlen(PQgetvalue(DBQuery, 0, 3)) + 1;
                data->title = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->title, PQgetvalue(DBQuery, 0, 3));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->title, 1, stringLength * sizeof(char), "char");

                stringLength = strlen(PQgetvalue(DBQuery, 0, 4)) + 1;
                data->description = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->description, PQgetvalue(DBQuery, 0, 4));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->description, 1, stringLength * sizeof(char), "char");

                stringLength = strlen(PQgetvalue(DBQuery, 0, 5)) + 1;
                data->icatRef = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->icatRef, PQgetvalue(DBQuery, 0, 5));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->icatRef, 1, stringLength * sizeof(char), "char");

                data->status = PQgetvalue(DBQuery, 0, 6)[0];

                stringLength = strlen(PQgetvalue(DBQuery, 0, 7)) + 1;
                data->creation = (char*) malloc(stringLength * sizeof(char));
                strcpy(data->creation, PQgetvalue(DBQuery, 0, 7));
                addMalloc(idam_plugin_interface->logmalloclist, (void*) data->creation, 1, stringLength * sizeof(char), "char");

                PQclear(DBQuery);

                IDAM_LOGF(UDA_LOG_DEBUG, "Provenance uuid: %s\n", data->uuid);
                IDAM_LOGF(UDA_LOG_DEBUG, "owner          : %s\n", data->owner);
                IDAM_LOGF(UDA_LOG_DEBUG, "class          : %s\n", data->class);
                IDAM_LOGF(UDA_LOG_DEBUG, "title          : %s\n", data->title);
                IDAM_LOGF(UDA_LOG_DEBUG, "description    : %s\n", data->description);
                IDAM_LOGF(UDA_LOG_DEBUG, "icatRefId      : %s\n", data->icatRef);
                IDAM_LOGF(UDA_LOG_DEBUG, "status         : %c\n", data->status);
                IDAM_LOGF(UDA_LOG_DEBUG, "creation       : %s\n", data->creation);

// the Returned Structure Definition

                data->structVersion = 1;                // This structure's version number

                initUserDefinedType(&usertype);            // New structure definition

                strcpy(usertype.name, "PROVENANCEUUID");
                usertype.size = sizeof(PROVENANCEUUID);

                strcpy(usertype.source, "Provenance::admin()");
                usertype.ref_id = 0;
                usertype.imagecount = 0;                // No Structure Image data
                usertype.image = NULL;
                usertype.idamclass = TYPE_COMPOUND;

                offset = 0;

                defineField(&field, "structVersion", "This Data Structure's version number", &offset, SCALARUSHORT);
                addCompoundField(&usertype, field);
                defineField(&field, "uuid", "Unique Identifier", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "owner", "Owner", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "class", "Class", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "title", "Title", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "description", "Description", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "icatRef", "ICAT Reference ID", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);
                defineField(&field, "status", "Status", &offset, SCALARCHAR);
                addCompoundField(&usertype, field);
                defineField(&field, "creation", "Creation or registration date", &offset, SCALARSTRING);
                addCompoundField(&usertype, field);

                USERDEFINEDTYPELIST* userdefinedtypelist = idam_plugin_interface->userdefinedtypelist;
                addUserDefinedType(userdefinedtypelist, usertype);

// Register the Pointer to the Data Structure 	 

                addMalloc(idam_plugin_interface->logmalloclist, (void*) data, 1, sizeof(PROVENANCEUUID), "PROVENANCEUUID");

// Pass the Data back	 

                data_block->data_type = TYPE_COMPOUND;
                data_block->rank = 0;
                data_block->data_n = 1;
                data_block->data = (char*) data;

                strcpy(data_block->data_desc, "A Provenance UUID Record");
                strcpy(data_block->data_label, "");
                strcpy(data_block->data_units, "");

                data_block->opaque_type = OPAQUE_TYPE_STRUCTURES;
                data_block->opaque_count = 1;
                data_block->opaque_block = (void*) findUserDefinedType(userdefinedtypelist, "PROVENANCEUUID", 0);

            }

            break;
        }

    } while (0);

    return err;
}
