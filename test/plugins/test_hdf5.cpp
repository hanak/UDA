#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <c++/UDA.hpp>

TEST_CASE( "Test NEWHDF5::help() function", "[NEWHDF5][plugins]" )
{
#include "setup.inc"

    uda::Client client;

    const uda::Result& result = client.get("NEWHDF5::help()", "");

    REQUIRE( result.errorCode() == 0 );
    REQUIRE( result.errorMessage().empty() );

    uda::Data* data = result.data();

    REQUIRE( data != nullptr );
    REQUIRE( !data->isNull() );
    REQUIRE( data->type().name() == typeid(char*).name() );

    auto str = dynamic_cast<uda::String*>(data);

    REQUIRE( str != nullptr );

    std::string expected = "\nnewHDF5: get - Read data from a HDF5 file\n\n";

    REQUIRE( str->str() == expected );
}
