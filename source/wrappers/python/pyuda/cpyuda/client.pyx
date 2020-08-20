#cython: language_level=3

cimport uda
cimport numpy as np
from libc cimport string
from libc.stdlib cimport malloc, free
from libc.string cimport strlen


def set_property(prop_name, value):
    if prop_name in ('timeout', 'altRank'):
        prop_string = prop_name + '=' + str(value)
    else:
        prop_string = prop_name
    uda.setIdamProperty(prop_string.encode())


def get_property(prop_name):
    prop = uda.getIdamProperty(prop_name).decode()
    if prop_name in ('timeout', 'altRank'):
        return prop
    else:
        return bool(prop)


def get_server_host_name():
    return uda.getIdamServerHost().decode()


def get_server_port():
    return uda.getIdamServerPort()


def set_server_host_name(host_name):
    uda.putIdamServerHost(host_name.encode())


def set_server_port(port):
    uda.putIdamServerPort(port)


def get_data(signal, source):
    handle = uda.idamGetAPI(signal.encode(), source.encode())
    cdef const char* err_msg
    cdef int err_code
    if handle < 0:
        err_msg = uda.getIdamErrorMsg(handle)
        err_code = uda.getIdamErrorCode(handle)
        if err_msg == NULL or string.strlen(err_msg) == 0:
            raise UDAException("unknown error occured")
        elif err_code < 0:
            raise ClientException(err_msg.decode())
        else:
            raise ServerException(err_msg.decode())
    return Result(handle)


cdef put_nothing(const char* instruction):
    cdef int handle = uda.idamPutAPI(instruction, NULL)
    return Result(handle)


cdef int UDA_TYPE_UNKNOWN = 0,
cdef int UDA_TYPE_CHAR = 1,
cdef int UDA_TYPE_SHORT = 2,
cdef int UDA_TYPE_INT = 3,
cdef int UDA_TYPE_UNSIGNED_INT = 4,
cdef int UDA_TYPE_LONG = 5,
cdef int UDA_TYPE_FLOAT = 6,
cdef int UDA_TYPE_DOUBLE = 7,
cdef int UDA_TYPE_UNSIGNED_CHAR = 8,
cdef int UDA_TYPE_UNSIGNED_SHORT = 9,
cdef int UDA_TYPE_UNSIGNED_LONG = 10,
cdef int UDA_TYPE_LONG64 = 11,
cdef int UDA_TYPE_UNSIGNED_LONG64 = 12,
cdef int UDA_TYPE_COMPLEX = 13,
cdef int UDA_TYPE_DCOMPLEX = 14,
cdef int UDA_TYPE_UNDEFINED = 15,
cdef int UDA_TYPE_VLEN = 16,
cdef int UDA_TYPE_STRING = 17,
cdef int UDA_TYPE_COMPOUND = 18,
cdef int UDA_TYPE_OPAQUE = 19,
cdef int UDA_TYPE_ENUM = 20,
cdef int UDA_TYPE_VOID  = 21,
cdef int UDA_TYPE_STRING2 = 99

numpy2uda_type_map = {
    np.NPY_BOOL: UDA_TYPE_INT,
    np.NPY_BYTE: UDA_TYPE_CHAR,
    np.NPY_UBYTE: UDA_TYPE_UNSIGNED_CHAR,
    np.NPY_SHORT: UDA_TYPE_SHORT,
    np.NPY_USHORT: UDA_TYPE_UNSIGNED_SHORT,
    np.NPY_INT: UDA_TYPE_INT,
    np.NPY_UINT: UDA_TYPE_UNSIGNED_INT,
    np.NPY_LONG: UDA_TYPE_LONG,
    np.NPY_ULONG: UDA_TYPE_UNSIGNED_LONG,
    np.NPY_LONGLONG: UDA_TYPE_LONG64,
    np.NPY_ULONGLONG: UDA_TYPE_LONG64,
    np.NPY_FLOAT: UDA_TYPE_FLOAT,
    np.NPY_DOUBLE: UDA_TYPE_DOUBLE,
    np.NPY_LONGDOUBLE: UDA_TYPE_UNKNOWN,
    np.NPY_CFLOAT: UDA_TYPE_COMPLEX,
    np.NPY_CDOUBLE: UDA_TYPE_DCOMPLEX,
    np.NPY_CLONGDOUBLE: UDA_TYPE_UNKNOWN,
    np.NPY_OBJECT: UDA_TYPE_UNKNOWN,
    np.NPY_STRING: UDA_TYPE_STRING,
    np.NPY_UNICODE: UDA_TYPE_STRING,
    np.NPY_VOID: UDA_TYPE_UNKNOWN,
}


cdef numpy_type_to_UDA_type(int type):
    return numpy2uda_type_map[type]


cdef put_ndarray(const char* instruction, np.ndarray data):
    cdef uda.PUTDATA_BLOCK put_data
    uda.initIdamPutDataBlock(&put_data)

    cdef int rank = np.PyArray_NDIM(data)
    cdef np.npy_intp* shape = np.PyArray_DIMS(data)
    cdef int size = data.dtype.itemsize

    put_data.data_type = numpy_type_to_UDA_type(np.PyArray_TYPE(data))
    put_data.rank = rank
    put_data.count = np.PyArray_SIZE(data)
    put_data.shape = <int *> malloc(rank * size)
    cdef int i = 0
    while i < rank:
        put_data.shape[i] = shape[i]
        i += 1
    put_data.data = np.PyArray_BYTES(data)

    cdef int handle = uda.idamPutAPI(instruction, &put_data)
    free(put_data.shape)
    return Result(handle)


cdef put_scalar(const char* instruction, object data):
    cdef uda.PUTDATA_BLOCK put_data

    cdef np.dtype type = np.PyArray_DescrFromScalar(data)
    cdef char* bytes = <char *> malloc(type.itemsize)
    np.PyArray_ScalarAsCtype(data, bytes)

    put_data.data_type = numpy_type_to_UDA_type(type.type_num)
    put_data.rank = 0
    put_data.count = 1
    put_data.shape = NULL
    put_data.data = bytes

    cdef int handle = uda.idamPutAPI(instruction, &put_data)
    free(bytes)
    return Result(handle)


cdef put_string(const char* instruction, const char* data):
    cdef uda.PUTDATA_BLOCK put_data
    uda.initIdamPutDataBlock(&put_data)

    cdef int string_length = strlen(data)

    put_data.data_type = 17 # UDA_TYPE_STRING
    put_data.rank = 0
    put_data.count = string_length + 1
    put_data.shape = NULL
    put_data.data = data

    cdef int handle = uda.idamPutAPI(instruction, &put_data)
    return Result(handle)


def put_data(instruction, data=None):
    if isinstance(data, np.ndarray):
        return put_ndarray(instruction, data)
    elif np.PyArray_CheckScalar(data):
        return put_scalar(instruction, data)
    elif isinstance(data, bytes):
        return put_string(instruction, data)
    else:
        return put_nothing(instruction)

