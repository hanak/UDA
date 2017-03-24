#include "geomConfig.h"

#include <strings.h>
#include <tgmath.h>

#include <structures/struct.h>
#include <structures/accessors.h>
#include <server/modules/netcdf4/readCDF4.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaTypes.h>
#include <clientserver/initStructs.h>

////////////////////
// Function to find which geomgroup (and therefore which file) a signal is in
// and then to call a script to create the appropriate geometry file from the CAD.
// This is a test function at the moment, the script called doesn't actually do anything!
// To be integrated with Ivan's python script that extracts info from step files.
//
//   Arguments
//    - signal : The signal for which the file that needs making needs identifying
//    - tor_angle : The toroidal angle at which to extract the information. 
//    - DBConnect : The connection to the geom database
//    - DBQuery : The query for the geom database
////////////////////
int generateGeom(char* signal, float tor_angle, PGconn* DBConnect, PGresult* DBQuery)
{
    char* pythonpath = getenv("PYTHONPATH");
    IDAM_LOGF(LOG_DEBUG, "python path: %s\n", pythonpath);

    char* path = getenv("PATH");
    IDAM_LOGF(LOG_DEBUG, "path: %s\n", path);

    IDAM_LOG(LOG_DEBUG, "Trying to generate geometry.\n");

    // Check if the signal exists
    char query_signal[MAXSQL];
    sprintf(query_signal,
            "SELECT g.name FROM geomgroup g, geomgroup_geomsignal_map ggm"
                    " WHERE lower(ggm.geomsignal_alias)=lower('%s')"
                    "   AND g.geomgroup_id=ggm.geomgroup_id;",
            signal);

    IDAM_LOGF(LOG_DEBUG, "Query is %s.\n", query_signal);

    if ((DBQuery = PQexec(DBConnect, query_signal)) == NULL) {
        RAISE_PLUGIN_ERROR("Database query failed.\n");
    }

    if (PQresultStatus(DBQuery) != PGRES_TUPLES_OK && PQresultStatus(DBQuery) != PGRES_COMMAND_OK) {
        RAISE_PLUGIN_ERROR("Database query failed.\n");
    }

    int nRows_signal = PQntuples(DBQuery);

    if (nRows_signal == 0) {
        RAISE_PLUGIN_ERROR("No rows were found in database matching query\n");
    }

    // Retrieve the geomgroup that we need to retrieve geometry for
    int s_group = PQfnumber(DBQuery, "name");

    if (!PQgetisnull(DBQuery, 0, s_group)) {
        char* geom_group = PQgetvalue(DBQuery, 0, s_group);

        // Run python script to slice model for this component.
        char system_call[MAXSQL];
        sprintf(system_call, "run_make_file_from_cad.sh %s %f", geom_group, tor_angle);

        IDAM_LOGF(LOG_DEBUG, "Run command %s\n", system_call);

        int ret = system(system_call);

        IDAM_LOGF(LOG_DEBUG, "Return code from geometry making code %d\n", ret);

        if (ret != 0) RAISE_PLUGIN_ERROR("Calling generate geom script failed\n");
    } else {
        RAISE_PLUGIN_ERROR("Null group\n");
    }

    return 0;
}


int do_geom_get(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{

    ////////////////////////////
    // Get function to return data from configuration file or corresponding calibration file.
    //
    // Arguments:
    // file : Location of file. If not given, then appropriate file name will be retrieved from the db
    //        for the given signal.
    // signal : Signal/Group to be retrieved from the file.
    // config : If argument is present, then the configuration file will be returned.
    // cal : If argument is present, then the calibration file will be returned.
    // version_config : Version number for Config file. If not set then the latest will be returned.
    // version_cal : Version number for calibration file. If not set then the latest will be returned.
    // three_d : If argument is present, then the 3D geometry is returned, otherwise the 2D geometry is returned.
    // tor_angle : If returning the 2D geometry, and the component is toroidal angle - dependent, then the user must
    //             give a toroidal angle at which to slice the model.

    IDAM_LOG(LOG_DEBUG, "get file and signal names.\n");

    ////////////////////////////
    // Get parameters passed by user.
    char* signal = NULL;
    FIND_REQUIRED_STRING_VALUE(idam_plugin_interface->request_block->nameValueList, signal);

    char* file = NULL;
    FIND_STRING_VALUE(idam_plugin_interface->request_block->nameValueList, file);

    int isConfig = findValue(&idam_plugin_interface->request_block->nameValueList, "config");
    int isCal = findValue(&idam_plugin_interface->request_block->nameValueList, "cal");

    float version = -1;
    FIND_FLOAT_VALUE(idam_plugin_interface->request_block->nameValueList, version);

    int ver = -1;
    int rev = -1;
    if (version >= 0) {
        ver = floor(version);
        rev = floor(version * 10);
        IDAM_LOGF(LOG_DEBUG, "Version %d, Revision %d", ver, rev);
    }

    // Toroidal angle dependence
    int three_d = findValue(&idam_plugin_interface->request_block->nameValueList, "three_d");
    float tor_angle = -1;
    FIND_FLOAT_VALUE(idam_plugin_interface->request_block->nameValueList, tor_angle);

    if (tor_angle < 0 || tor_angle > 2 * M_PI) {
        IDAM_LOG(LOG_DEBUG,
                 "If providing a toroidal angle, it must be in the range 0 - 2 pi. Angle given will be ignored.\n");
        tor_angle = -1;
    } else {
        IDAM_LOGF(LOG_DEBUG, "Tor_angle is set to %f\n", tor_angle);
    }

    int shot = idam_plugin_interface->request_block->exp_number;
    IDAM_LOGF(LOG_DEBUG, "Exp number %d\n", shot);

    IDAM_LOGF(LOG_DEBUG, "Config? %d or cal %d \n", isConfig, isCal);

    if (file == NULL && !isCal && !isConfig) {
        RAISE_PLUGIN_ERROR("Filename wasn't given and didn't request configuration or calibration data.\n");
    }

    ////////////////////////////
    // If Config or cal arguments were given,
    // connect to database to retrieve filenames
    char* signal_type = NULL;
    char* signal_for_file = NULL;

    if (isConfig || isCal) {

        //////////////////////////////
        // Open the connection
        IDAM_LOG(LOG_DEBUG, "trying to get connection\n");

        char* db_host = getenv("GEOM_DB_HOST");
        char* db_port_str = getenv("GEOM_DB_PORT");
        int db_port = -1;
        if (db_port_str != NULL) db_port = atoi(db_port_str);
        char* db_name = getenv("GEOM_DB_NAME");
        char* db_user = getenv("GEOM_DB_USER");

        if (db_host == NULL || db_port_str == NULL || db_name == NULL || db_user == NULL) {
            RAISE_PLUGIN_ERROR("Geom db host, port, name and user env variables were not set.\n");
        }

        PGconn* DBConnect = openDatabase(db_host, db_port, db_name, db_user);
        PGresult* DBQuery = NULL;

        if (PQstatus(DBConnect) != CONNECTION_OK) {
            PQfinish(DBConnect);
            RAISE_PLUGIN_ERROR("Connection to mastgeom database failed.\n");
        }

        char* signal_for_query = (char*)malloc((2 * strlen(signal) + 1) * sizeof(char));
        int err = 0;
        PQescapeStringConn(DBConnect, signal_for_query, signal, strlen(signal), &err);

        ///////////////////
        // Construct query to extract filename of the file that needs to be read in
        char query[MAXSQL];

        if (isConfig == 1) {
            // configuration files
            sprintf(query,
                    "SELECT cds.file_name, ggm.geomsignal_alias, cds.version, cds.revision, ggm.geomsignal_shortname"
                            " FROM config_data_source cds, geomgroup_geomsignal_map ggm"
                            " WHERE cds.config_data_source_id=ggm.config_data_source_id"
                            "   AND (lower(ggm.geomsignal_alias) LIKE lower('%s/%%')"
                            "        OR lower(ggm.geomsignal_shortname) LIKE lower('%s/%%'))"
                            "   AND cds.start_shot<=%d AND cds.end_shot>%d",
                    signal_for_query, signal_for_query, shot, shot);

            // Add check for toroidal angle
            if (three_d == 1) {
                strcat(query, " AND cds.tor_angle=NULL;");
            } else if (tor_angle >= 0) {
                char tor_angle_statement[MAXSQL];
                sprintf(tor_angle_statement,
                        " AND (CASE WHEN cds.tor_angle_dependent=TRUE "
                                "THEN round(cds.tor_angle::numeric, 4)=round(%f, 4) "
                                "ELSE TRUE END)", tor_angle);
                strcat(query, tor_angle_statement);
            } else {
                strcat(query, " AND cds.tor_angle_dependent=FALSE ");
            }

        } else {
            // calibration files
            sprintf(query,
                    "SELECT cds.file_name, ggm.geomsignal_alias, cds.version, cds.revision, ggm.geomsignal_shortname"
                            " FROM cal_data_source cds"
                            " INNER JOIN config_data_source cods"
                            "   ON cods.config_data_source_id=cds.config_data_source_id"
                            " INNER JOIN geomgroup_geomsignal_map ggm"
                            "   ON ggm.config_data_source_id=cods.config_data_source_id"
                            " WHERE lower(ggm.geomsignal_alias) LIKE lower('%s/%%')"
                            "   AND cds.start_shot<=%d AND cds.end_shot>%d",
                    signal, shot, shot);

            // Add check for toroidal angle
            if (three_d == 1) {
                strcat(query, " AND cds.tor_angle=NULL;");
            } else if (tor_angle >= 0) {
                char tor_angle_statement[MAXSQL];
                sprintf(tor_angle_statement,
                        " AND (CASE WHEN cods.tor_angle_dependent=TRUE "
                                "THEN round(cds.tor_angle::numeric, 4)=round(%f, 4) "
                                "ELSE TRUE END)", tor_angle);
                strcat(query, tor_angle_statement);
            } else {
                strcat(query, " AND cods.tor_angle_dependent=FALSE ");
            }
        }

        // Add version check
        if (ver >= 0) {
            char ver_str[2];
            sprintf(ver_str, "%d", ver);
            char rev_str[2];
            sprintf(rev_str, "%d", rev);

            strcat(query, " AND cds.version=");
            strcat(query, ver_str);
            strcat(query, " AND cds.revision=");
            strcat(query, rev_str);
        } else {
            strcat(query, " ORDER BY cds.version DESC, cds.revision DESC LIMIT 1;");
        }

        IDAM_LOGF(LOG_DEBUG, "query is %s\n", query);

        /////////////////////////////
        // Query database
        if ((DBQuery = PQexec(DBConnect, query)) == NULL) {
            RAISE_PLUGIN_ERROR("Database query failed.\n");
        }

        if (PQresultStatus(DBQuery) != PGRES_TUPLES_OK && PQresultStatus(DBQuery) != PGRES_COMMAND_OK) {
            PQclear(DBQuery);
            PQfinish(DBConnect);
            RAISE_PLUGIN_ERROR("Database query failed.\n");
        }

        // Retrieve number of rows found in query
        int nRows = PQntuples(DBQuery);

        /////////////////////////////
        // No rows found?
        // For the future: if they've asked for a specific toroidal angle then try to generate geom from the CAD.
        //                 see commit 91ea8223480cf28228caf30cb43d05fa1c9947db for prototype
        if (nRows == 0) {
            IDAM_LOGF(LOG_DEBUG, "no rows. isConfig %d, three_d %d, tor_angle %f\n", isConfig, three_d, tor_angle);

            PQclear(DBQuery);
            PQfinish(DBConnect);
            RAISE_PLUGIN_ERROR("No rows were found in database matching query\n");
        }

        /////////////////////////////
        // We have found a matching file, extract the filename
        int s_file = PQfnumber(DBQuery, "file_name");
        int s_alias = PQfnumber(DBQuery, "geomsignal_alias");
        int s_short = PQfnumber(DBQuery, "geomsignal_shortname");

        char* file_db;
        if (!PQgetisnull(DBQuery, 0, s_file)) {
            file_db = PQgetvalue(DBQuery, 0, s_file);

            IDAM_LOGF(LOG_DEBUG, "file from db %s\n", file_db);

            //Add on the location
            char* file_path = getenv("MAST_GEOM_DATA");

            if (file_path == NULL) {
                RAISE_PLUGIN_ERROR("Could not retrieve MAST_GEOM_DATA environment variable\n");
            }

            if (file == NULL) {
                file = (char*)malloc(sizeof(char) * (strlen(file_db) + strlen(file_path) + 2));
                strcpy(file, file_path);
                strcat(file, "/");
                strcat(file, file_db);
            }

            IDAM_LOGF(LOG_DEBUG, "file_path %s\n", file);
        }

        int use_alias = 0;

        // Check if the signal given is a match to the shortname.
        // If so, then we want to use the signal alias instead since it has the
        // full netcdf path
        if (!PQgetisnull(DBQuery, 0, s_short)) {
            char* sigShort = PQgetvalue(DBQuery, 0, s_short);

            IDAM_LOGF(LOG_DEBUG, "sig short %s, sig %s\n", sigShort, signal_for_query);

            if (!strncmp(sigShort, signal_for_query, strlen(sigShort) - 1)) {
                signal_type = (char*)malloc(sizeof(char) * 7 + 1);
                strcpy(signal_type, "element");
                use_alias = 1;
            }
        }

        // If the signal matches that asked for by the user, then this means that this
        // is the signal itself. If it does not match, then we are at the group level
        // This info is useful for the wrappers
        if (!PQgetisnull(DBQuery, 0, s_alias)) {
            char* sigAlias = PQgetvalue(DBQuery, 0, s_alias);

            IDAM_LOGF(LOG_DEBUG, "sig alias %s use alias? %d\n", sigAlias, use_alias);

            // Need to ignore last character of sigAlias, which will be a /
            if (!strncmp(sigAlias, signal_for_query, strlen(sigAlias) - 1)
                && !use_alias) {
                signal_type = (char*)malloc(sizeof(char) * 7 + 1);
                strcpy(signal_type, "element");
                signal_for_file = (char*)malloc(sizeof(char) * (strlen(signal) + 1));
                strcpy(signal_for_file, signal);
            } else if (use_alias) {
                // In this case, we want to use the signal alias since it is the full path.
                // But, it doesn't work if it has the / on the end, so don't include the last
                // character
                signal_for_file = (char*)malloc(sizeof(char) * (strlen(sigAlias) + 1));
                strncpy(signal_for_file, sigAlias, strlen(sigAlias));
                signal_for_file[strlen(sigAlias) - 1] = '\0';
            } else {
                signal_type = (char*)malloc(sizeof(char) * 5 + 1);
                strcpy(signal_type, "group");
                signal_for_file = (char*)malloc(sizeof(char) * (strlen(signal) + 1));
                strcpy(signal_for_file, signal);
            }
        }

        IDAM_LOG(LOG_DEBUG, "Close connection\n");

        //Close db connection
        PQclear(DBQuery);
        PQfinish(DBConnect);
        free(signal_for_query);
    } else {
        signal_type = (char*)malloc(sizeof(char*));
        signal_type = "a";
        signal_for_file = (char*)malloc(sizeof(char) * (strlen(signal) + 1));
        strcpy(signal_for_file, signal);
    }

    IDAM_LOGF(LOG_DEBUG, "signal_type %s \n", signal_type);

    ///////////////////////////
    // Setup signal and file path in signal_desc
    DATA_SOURCE* data_source = idam_plugin_interface->data_source;
    SIGNAL_DESC* signal_desc = idam_plugin_interface->signal_desc;
    strcpy(signal_desc->signal_name, signal_for_file);
    strcpy(data_source->path, file);

    /////////////////////////////
    // Read in the file
    IDAM_LOGF(LOG_DEBUG, "Calling readCDF to retrieve file %s with signal for file %s\n", file, signal_for_file);
    DATA_BLOCK data_block_file;
    initDataBlock(&data_block_file);

    REQUEST_BLOCK* request_block = idam_plugin_interface->request_block;
    int errConfig = readCDF(*data_source, *signal_desc, *request_block, &data_block_file);

    IDAM_LOG(LOG_DEBUG, "Read in file\n");

    if (errConfig != 0) {
        RAISE_PLUGIN_ERROR("Error reading geometry data!\n");
    }

    if (data_block_file.data_type != TYPE_COMPOUND) {
        RAISE_PLUGIN_ERROR("Non-structured type returned from data reader!\n");
    }

    /////////////////////////////
    // Combine datablock and signal_type into one structure
    // to be returned
    USERDEFINEDTYPE* udt = data_block_file.opaque_block;

    struct DATAPLUSTYPE {
        void* data;
        char* signal_type;
    };
    typedef struct DATAPLUSTYPE DATAPLUSTYPE;

    USERDEFINEDTYPE parentTree;
    COMPOUNDFIELD field;
    int offset = 0;

    //User defined type to describe data structure
    initUserDefinedType(&parentTree);
    parentTree.idamclass = TYPE_COMPOUND;
    strcpy(parentTree.name, "DATAPLUSTYPE");
    strcpy(parentTree.source, "netcdf");
    parentTree.ref_id = 0;
    parentTree.imagecount = 0;
    parentTree.image = NULL;
    parentTree.size = sizeof(DATAPLUSTYPE);

    //Compound field for calibration file
    initCompoundField(&field);
    strcpy(field.name, "data");
    field.atomictype = TYPE_UNKNOWN;
    strcpy(field.type, udt->name);
    strcpy(field.desc, "data");

    field.pointer = 1;
    field.count = 1;
    field.rank = 0;
    field.shape = NULL;            // Needed when rank >= 1

    field.size = field.count * sizeof(void*);
    field.offset = newoffset(offset, field.type);
    field.offpad = padding(offset, field.type);
    field.alignment = getalignmentof(field.type);
    offset = field.offset + field.size;    // Next Offset
    addCompoundField(&parentTree, field);

    //For signal_type
    initCompoundField(&field);
    defineField(&field, "signal_type", "signal_type", &offset, SCALARSTRING);
    addCompoundField(&parentTree, field);

    addUserDefinedType(userdefinedtypelist, parentTree);

    DATAPLUSTYPE* data;
    size_t stringLength = strlen(signal_type) + 1;
    data = (DATAPLUSTYPE*)malloc(sizeof(DATAPLUSTYPE));
    data->signal_type = (char*)malloc(stringLength * sizeof(char));
    data->data = data_block_file.data;
    strcpy(data->signal_type, signal_type);

    addMalloc((void*)data, 1, sizeof(DATAPLUSTYPE), "DATAPLUSTYPE");
    addMalloc((void*)data->signal_type, 1, stringLength * sizeof(char), "char");

    //Return data
    DATA_BLOCK* data_block = idam_plugin_interface->data_block;

    initDataBlock(data_block);

    data_block->data_type = TYPE_COMPOUND;
    data_block->rank = 0;            // Scalar structure (don't need a DIM array)
    data_block->data_n = 1;
    data_block->data = (char*)data;

    strcpy(data_block->data_desc, "Data plus type");
    strcpy(data_block->data_label, "Data plus type");
    strcpy(data_block->data_units, "");

    data_block->opaque_type = OPAQUE_TYPE_STRUCTURES;
    data_block->opaque_count = 1;
    data_block->opaque_block = (void*)findUserDefinedType("DATAPLUSTYPE", 0);

    // Free heap data associated with the two DATA_BLOCKS
    // Nothing to free?
    data_block_file.data = NULL;

    free(signal_for_file);

    return 0;
}

int do_config_filename(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{

    ////////////////////////////
    // Return filenames for given geom signal aliases
    //
    // Arguments:
    // signal: Geom signal group
    ////////////////////////////
    char* signal = NULL;
    FIND_REQUIRED_STRING_VALUE(idam_plugin_interface->request_block->nameValueList, signal);

    IDAM_LOGF(LOG_DEBUG, "Using signal name: %s\n", signal);

    int shot = idam_plugin_interface->request_block->exp_number;
    IDAM_LOGF(LOG_DEBUG, "Exp number %d\n", shot);

    //////////////////////////////
    // Open the connection
    IDAM_LOG(LOG_DEBUG, "trying to get connection\n");

    char* db_host = getenv("GEOM_DB_HOST");
    char* db_port_str = getenv("GEOM_DB_PORT");
    int db_port = -1;
    if (db_port_str != NULL) db_port = atoi(db_port_str);
    char* db_name = getenv("GEOM_DB_NAME");
    char* db_user = getenv("GEOM_DB_USER");

    if (db_host == NULL || db_port_str == NULL || db_name == NULL || db_user == NULL) {
        RAISE_PLUGIN_ERROR("Geom db host, port, name and user env variables were not set.\n");
    }

    PGconn* DBConnect = openDatabase(db_host, db_port, db_name, db_user);

    PGresult* DBQuery = NULL;

    if (PQstatus(DBConnect) != CONNECTION_OK) {
        PQfinish(DBConnect);
        RAISE_PLUGIN_ERROR("Connection to mastgeom database failed.\n");
    }

    char* signal_for_query = (char*)malloc((2 * strlen(signal) + 1) * sizeof(char));
    int err = 0;
    PQescapeStringConn(DBConnect, signal_for_query, signal, strlen(signal), &err);

    ///////////////////
    // Construct query to extract filename of the file that needs to be read in
    char query[MAXSQL];

    sprintf(query, "SELECT distinct(cds.file_name), cds.geomgroup "
            " FROM config_data_source cds, geomgroup_geomsignal_map ggm"
            " WHERE cds.config_data_source_id=ggm.config_data_source_id"
            " AND (lower(ggm.geomsignal_alias) LIKE lower('%s/%%')"
            "      OR lower(ggm.geomsignal_shortname) LIKE lower('%s/%%'))"
            " AND cds.start_shot <= %d"
            " AND cds.end_shot > %d;",
            signal_for_query, signal_for_query, shot, shot);

    IDAM_LOGF(LOG_DEBUG, "query is %s\n", query);

    /////////////////////////////
    // Query database
    if ((DBQuery = PQexec(DBConnect, query)) == NULL) {
        RAISE_PLUGIN_ERROR("Database query failed.\n");
    }

    if (PQresultStatus(DBQuery) != PGRES_TUPLES_OK && PQresultStatus(DBQuery) != PGRES_COMMAND_OK) {
        PQclear(DBQuery);
        PQfinish(DBConnect);
        RAISE_PLUGIN_ERROR("Database query failed.\n");
    }

    // Retrieve number of rows found in query
    int nRows = PQntuples(DBQuery);

    if (nRows == 0) {
        PQclear(DBQuery);
        PQfinish(DBConnect);
        RAISE_PLUGIN_ERROR("No rows were found in database matching query\n");
    }

    /////////////////////////////
    // We have found a matching file, extract the filename and associated group
    int s_file = PQfnumber(DBQuery, "file_name");
    int s_group = PQfnumber(DBQuery, "geomgroup");

    struct DATASTRUCT {
        char** filenames;
        char** geomgroups;
    };
    typedef struct DATASTRUCT DATASTRUCT;

    DATASTRUCT* data_out;
    data_out = (DATASTRUCT*)malloc(sizeof(DATASTRUCT));
    addMalloc((void*)data_out, 1, sizeof(DATASTRUCT), "DATASTRUCT");

    data_out->filenames = (char**)malloc((nRows) * sizeof(char*));
    addMalloc((void*)data_out->filenames, nRows, sizeof(char*), "STRING *");
    data_out->geomgroups = (char**)malloc((nRows) * sizeof(char*));
    addMalloc((void*)data_out->geomgroups, nRows, sizeof(char*), "STRING *");

    int i = 0;
    int stringLength;

    for (i = 0; i < nRows; i++) {
        if (!PQgetisnull(DBQuery, i, s_file)) {
            char* file_name = PQgetvalue(DBQuery, i, s_file);
            stringLength = strlen(file_name) + 1;
            data_out->filenames[i] = (char*)malloc(sizeof(char) * stringLength);
            strcpy(data_out->filenames[i], file_name);
            addMalloc((void*)data_out->filenames[i], stringLength, sizeof(char), "char");
        }

        if (!PQgetisnull(DBQuery, i, s_group)) {
            char* group = PQgetvalue(DBQuery, i, s_group);
            stringLength = strlen(group) + 1;
            data_out->geomgroups[i] = (char*)malloc(sizeof(char) * stringLength);
            strcpy(data_out->geomgroups[i], group);
            addMalloc((void*)data_out->geomgroups[i], stringLength, sizeof(char), "char");
        }
    }

    //Close db connection
    PQclear(DBQuery);
    PQfinish(DBConnect);
    free(signal_for_query);

    /////////////////////////////
    // Put into datablock to be returned
    /////////////////////////////
    USERDEFINEDTYPE parentTree;
    COMPOUNDFIELD field;
    int offset = 0;

    //User defined type to describe data structure
    initUserDefinedType(&parentTree);
    parentTree.idamclass = TYPE_COMPOUND;
    strcpy(parentTree.name, "DATASTRUCT");
    strcpy(parentTree.source, "IDAM3");
    parentTree.ref_id = 0;
    parentTree.imagecount = 0;
    parentTree.image = NULL;
    parentTree.size = sizeof(DATASTRUCT);

    // For filenames
    initCompoundField(&field);
    strcpy(field.name, "filenames");
    defineField(&field, "filenames", "filenames", &offset, ARRAYSTRING);
    addCompoundField(&parentTree, field);

    // For geomgroup
    initCompoundField(&field);
    strcpy(field.name, "geomgroups");
    defineField(&field, "geomgroups", "geomgroups", &offset, ARRAYSTRING);
    addCompoundField(&parentTree, field);

    addUserDefinedType(userdefinedtypelist, parentTree);

    //Return data
    DATA_BLOCK* data_block = idam_plugin_interface->data_block;

    initDataBlock(data_block);

    data_block->data_type = TYPE_COMPOUND;
    data_block->rank = 0;            // Scalar structure (don't need a DIM array)
    data_block->data_n = 1;
    data_block->data = (char*)data_out;

    strcpy(data_block->data_desc, "Data");
    strcpy(data_block->data_label, "Data");
    strcpy(data_block->data_units, "");

    data_block->opaque_type = OPAQUE_TYPE_STRUCTURES;
    data_block->opaque_count = 1;
    data_block->opaque_block = (void*)findUserDefinedType("DATASTRUCT", 0);

    // Free heap data associated with the two DATA_BLOCKS
    // Nothing to free?

    return 0;
}
