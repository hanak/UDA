//
// Created by jholloc on 08/03/16.
//

#include "result.hpp"

#include <sstream>
#include <complex>

#include <client/accAPI.h>
#include <client/udaClient.h>
#include <clientserver/udaTypes.h>

#include "string.hpp"
#include "array.hpp"

static const std::type_info* idamTypeToTypeID(int type)
{
    switch (type) {
        case UDA_TYPE_CHAR:
            return &typeid(char);
        case UDA_TYPE_SHORT:
            return &typeid(short);
        case UDA_TYPE_INT:
            return &typeid(int);
        case UDA_TYPE_UNSIGNED_INT:
            return &typeid(unsigned int);
        case UDA_TYPE_LONG:
            return &typeid(long);
        case UDA_TYPE_FLOAT:
            return &typeid(float);
        case UDA_TYPE_DOUBLE:
            return &typeid(double);
        case UDA_TYPE_UNSIGNED_CHAR:
            return &typeid(unsigned char);
        case UDA_TYPE_UNSIGNED_SHORT:
            return &typeid(unsigned short);
        case UDA_TYPE_UNSIGNED_LONG:
            return &typeid(unsigned long);
        case UDA_TYPE_LONG64:
            return &typeid(long long);
        case UDA_TYPE_UNSIGNED_LONG64:
            return &typeid(unsigned long long);
        case UDA_TYPE_COMPLEX:
            return &typeid(std::complex<float>);
        case UDA_TYPE_DCOMPLEX:
            return &typeid(std::complex<double>);
        case UDA_TYPE_STRING:
            return &typeid(char*);
        default:
            return &typeid(void);
    }
}

const std::string uda::Result::errorMessage() const
{
    char* error = getIdamErrorMsg(handle_);
    return error == nullptr ? "" : error;
}

int uda::Result::errorCode() const
{
    return getIdamErrorCode(handle_);
}

namespace {
//std::string to_string(int num)
//{
//    std::ostringstream ss;
//    ss << num;
//    return ss.str();
//}
}

uda::Result::Result(int handle)
        : handle_(handle), label_(handle >= 0 ? getIdamDataLabel(handle) : ""),
          units_(handle >= 0 ? getIdamDataUnits(handle) : ""),
          desc_(handle >= 0 ? getIdamDataDesc(handle) : ""),
          type_(handle >= 0 ? idamTypeToTypeID(getIdamDataType(handle)) : &typeid(void)),
          data_(handle >= 0 ? getIdamData(handle) : nullptr),
          rank_(handle >= 0 ? static_cast<dim_type>(getIdamRank(handle)) : 0),
          size_(handle >= 0 ? static_cast<std::size_t>(getIdamDataNum(handle)) : 0)
{
    if (handle >= 0 && (bool)getIdamProperties(handle)->get_meta) {
//        DATA_SOURCE* source = getIdamDataSource(handle);
//        meta_["path"] = source->path;
//        meta_["filename"] = source->filename;
//        meta_["format"] = source->format;
//        meta_["exp_number"] = to_string(source->exp_number);
//        meta_["pass"] = to_string(source->pass);
//        meta_["pass_date"] = source->pass_date;
    }
    istree_ = (setIdamDataTree(handle) != 0);
}

uda::Result::~Result()
{
    idamFree(handle_);
}

template<typename T>
static uda::Dim getDim(int handle, uda::dim_type num, uda::Result::DataType data_type)
{
    if (data_type == uda::Result::DataType::DATA) {
        std::string label = getIdamDimLabel(handle, num);
        std::string units = getIdamDimUnits(handle, num);
        auto size = static_cast<size_t>(getIdamDimNum(handle, num));
        auto data = reinterpret_cast<T*>(getIdamDimData(handle, num));
        return uda::Dim(num, data, size, label, units);
    }

    std::string label = getIdamDimLabel(handle, num);
    std::string units = getIdamDimUnits(handle, num);
    auto size = static_cast<size_t>(getIdamDimNum(handle, num));
    auto data = reinterpret_cast<T*>(getIdamDimError(handle, num));
    return uda::Dim(num, data, size, label + " error", units);
}

bool uda::Result::hasTimeDim() const
{
    return getIdamOrder(handle_) >= 0;
}

uda::Dim uda::Result::timeDim(DataType data_type) const
{
    auto order = getIdamOrder(handle_);
    if (order >= 0) {
        return dim(static_cast<dim_type>(order), data_type);
    }
    return Dim::Null;
}

uda::Dim uda::Result::dim(uda::dim_type num, DataType data_type) const
{
    int type = 0;
    if (data_type == DATA) {
        type = getIdamDimType(handle_, num);
    } else {
        type = getIdamDimErrorType(handle_, num);
    }

    switch (type) {
        case UDA_TYPE_CHAR:
            return getDim<char>(handle_, num, data_type);
        case UDA_TYPE_SHORT:
            return getDim<short>(handle_, num, data_type);
        case UDA_TYPE_INT:
            return getDim<int>(handle_, num, data_type);
        case UDA_TYPE_UNSIGNED_INT:
            return getDim<unsigned int>(handle_, num, data_type);
        case UDA_TYPE_LONG:
            return getDim<long>(handle_, num, data_type);
        case UDA_TYPE_FLOAT:
            return getDim<float>(handle_, num, data_type);
        case UDA_TYPE_DOUBLE:
            return getDim<double>(handle_, num, data_type);
        case UDA_TYPE_UNSIGNED_CHAR:
            return getDim<unsigned char>(handle_, num, data_type);
        case UDA_TYPE_UNSIGNED_SHORT:
            return getDim<unsigned short>(handle_, num, data_type);
        case UDA_TYPE_UNSIGNED_LONG:
            return getDim<unsigned long>(handle_, num, data_type);
        case UDA_TYPE_LONG64:
            return getDim<long long>(handle_, num, data_type);
        case UDA_TYPE_UNSIGNED_LONG64:
            return getDim<unsigned long long>(handle_, num, data_type);
        default:
            return Dim::Null;
    }

    return Dim::Null;
}

template<typename T>
uda::Data* getDataAs(int handle, uda::Result::DataType data_type, std::vector<uda::Dim>& dims)
{
    T* data = nullptr;
    if (data_type == uda::Result::DataType::DATA) {
        data = reinterpret_cast<T*>(getIdamData(handle));
    } else {
        data = reinterpret_cast<T*>(getIdamError(handle));
    }

    if (getIdamRank(handle) == 0) {
        if (getIdamDataNum(handle) > 1) {
            return new uda::Vector(data, (size_t)getIdamDataNum(handle));
        }
        return new uda::Scalar(data[0]);
    } else {
        return new uda::Array(data, dims);
    }
}

uda::Data* getDataAsString(int handle)
{
    char* data = getIdamData(handle);

    return new uda::String(data);
}

uda::Data* getDataAsStringArray(int handle, std::vector<uda::Dim>& dims)
{
    char* data = getIdamData(handle);

    size_t str_len = dims[0].size();
    size_t arr_len = getIdamDataNum(handle) / str_len;

    auto strings = new std::vector<std::string>;

    for (size_t i = 0; i < arr_len; ++i) {
        char* str = &data[i * str_len];
        strings->push_back(std::string(str, strlen(str)));
    }

    std::vector<uda::Dim> string_dims;

    for (size_t i = 1; i < dims.size(); ++i) {
        string_dims.push_back(dims[i]);
    }

    return new uda::Array(strings->data(), string_dims);
}

uda::Data* uda::Result::data() const
{
    std::vector<Dim> dims;
    auto rank = static_cast<dim_type>(getIdamRank(handle_));
    for (dim_type i = 0; i < rank; ++i) {
        dims.push_back(dim(i, DATA));
    }

    int type = getIdamDataType(handle_);

    switch (type) {
        case UDA_TYPE_CHAR:
            return getDataAs<char>(handle_, DATA, dims);
        case UDA_TYPE_SHORT:
            return getDataAs<short>(handle_, DATA, dims);
        case UDA_TYPE_INT:
            return getDataAs<int>(handle_, DATA, dims);
        case UDA_TYPE_UNSIGNED_INT:
            return getDataAs<unsigned int>(handle_, DATA, dims);
        case UDA_TYPE_LONG:
            return getDataAs<long>(handle_, DATA, dims);
        case UDA_TYPE_FLOAT:
            return getDataAs<float>(handle_, DATA, dims);
        case UDA_TYPE_DOUBLE:
            return getDataAs<double>(handle_, DATA, dims);
        case UDA_TYPE_UNSIGNED_CHAR:
            return getDataAs<unsigned char>(handle_, DATA, dims);
        case UDA_TYPE_UNSIGNED_SHORT:
            return getDataAs<unsigned short>(handle_, DATA, dims);
        case UDA_TYPE_UNSIGNED_LONG:
            return getDataAs<unsigned long>(handle_, DATA, dims);
        case UDA_TYPE_LONG64:
            return getDataAs<long long>(handle_, DATA, dims);
        case UDA_TYPE_UNSIGNED_LONG64:
            return getDataAs<unsigned long long>(handle_, DATA, dims);
        case UDA_TYPE_STRING:
            if (rank == 1 || rank == 0) {
                return getDataAsString(handle_);
            } else {
                return getDataAsStringArray(handle_, dims);
            }
        default:
            return &Array::Null;
    }
}


bool uda::Result::hasErrors() const
{
    return getIdamErrorType(handle_) != UDA_TYPE_UNKNOWN;
}

uda::Data* uda::Result::errors() const
{
    if (!hasErrors()) {
        return nullptr;
    }

    std::vector<Dim> dims;
    auto rank = static_cast<dim_type>(getIdamRank(handle_));
    for (dim_type i = 0; i < rank; ++i) {
        // XXX: error dimension data doesn't seem to actually be returned, so stick with standard dims for now
        dims.push_back(dim(i, DATA));
    }

    int type = getIdamErrorType(handle_);

    switch (type) {
        case UDA_TYPE_CHAR:
            return getDataAs<char>(handle_, ERRORS, dims);
        case UDA_TYPE_SHORT:
            return getDataAs<short>(handle_, ERRORS, dims);
        case UDA_TYPE_INT:
            return getDataAs<int>(handle_, ERRORS, dims);
        case UDA_TYPE_UNSIGNED_INT:
            return getDataAs<unsigned int>(handle_, ERRORS, dims);
        case UDA_TYPE_LONG:
            return getDataAs<long>(handle_, ERRORS, dims);
        case UDA_TYPE_FLOAT:
            return getDataAs<float>(handle_, ERRORS, dims);
        case UDA_TYPE_DOUBLE:
            return getDataAs<double>(handle_, ERRORS, dims);
        case UDA_TYPE_UNSIGNED_CHAR:
            return getDataAs<unsigned char>(handle_, ERRORS, dims);
        case UDA_TYPE_UNSIGNED_SHORT:
            return getDataAs<unsigned short>(handle_, ERRORS, dims);
        case UDA_TYPE_UNSIGNED_LONG:
            return getDataAs<unsigned long>(handle_, ERRORS, dims);
        case UDA_TYPE_LONG64:
            return getDataAs<long long>(handle_, ERRORS, dims);
        case UDA_TYPE_UNSIGNED_LONG64:
            return getDataAs<unsigned long long>(handle_, ERRORS, dims);
        case UDA_TYPE_STRING:
            if (rank == 1) {
                return getDataAsString(handle_);
            } else {
                return getDataAsStringArray(handle_, dims);
            }
        default:
            return &Array::Null;
    }
}

uda::TreeNode uda::Result::tree() const
{
    return TreeNode(handle_, getIdamDataTree(handle_));
}
