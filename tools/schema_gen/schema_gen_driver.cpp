/// schema_gen_driver.cpp — SeRTial automatic schema generator driver
///
/// Fixed source file compiled with user-supplied defines. rfl::json::write
/// serializes the full Record struct, so CommRaT (or any downstream user)
/// gets its own fields in the output without any extra wiring.
///
/// Required defines
/// ----------------
///   SERTIAL_REGISTRY_HEADER   Quoted path to the header that defines the
///                             collection type, e.g.:
///                               "/src/myapp/registry.hpp"
///                             which contains something like:
///                               using MyApp = commrat::CommRaT<Msg1, Msg2>;
///
///   SERTIAL_COLLECTION_TYPE   C++ type name for the MessageCollection, e.g.:
///                               MyApp
///
///   SERTIAL_OUTPUT_FILE       Absolute path for the generated JSON file.
///
/// Optional defines
/// ----------------
///   SERTIAL_RECORD_HEADER     Quoted path to a header that defines the
///                             Record type used for each message entry.
///                             Only needed when SERTIAL_RECORD_TYPE is set to
///                             something other than the default.
///
///   SERTIAL_RECORD_TYPE       C++ type for the per-message record.
///                             Defaults to sertial::MessageLayoutRecord.
///                             CommRaT sets this to commrat::CommRaTMessageRecord
///                             (which uses rfl::Flatten<sertial::MessageLayoutRecord>)
///                             so that rfl::json::write emits layout + commrat
///                             fields in one pass — no manual JSON merging.
///
/// The CMake helper sertial_generate_schema() sets all of these and adds a
/// post-build command so the JSON is regenerated whenever the exe rebuilds.

#include <fstream>
#include <iostream>
#include <string>

#include <rfl/json.hpp>
#include <sertial/integration/schema_generator.hpp>

#ifndef SERTIAL_REGISTRY_HEADER
#  error "SERTIAL_REGISTRY_HEADER must be defined — path to the header containing the collection type"
#endif
#ifndef SERTIAL_COLLECTION_TYPE
#  error "SERTIAL_COLLECTION_TYPE must be defined — C++ type name of the MessageCollection"
#endif
#ifndef SERTIAL_OUTPUT_FILE
#  error "SERTIAL_OUTPUT_FILE must be defined — output path for the generated JSON schema file"
#endif

// Pull in the user's collection header.
#include SERTIAL_REGISTRY_HEADER

// Optionally pull in the record type header (e.g. CommRaT's record struct).
#ifdef SERTIAL_RECORD_HEADER
#  include SERTIAL_RECORD_HEADER
#endif

// Default record type: plain layout-only record.
// Override with -DSERTIAL_RECORD_TYPE=commrat::CommRaTMessageRecord (or similar).
#ifndef SERTIAL_RECORD_TYPE
#  define SERTIAL_RECORD_TYPE sertial::MessageLayoutRecord
#endif

int main() {
    // generate_typed_data<Record>() populates record.layout for each type.
    // If Record has extra fields (e.g. commrat metadata), the driver's caller
    // is responsible for populating them; plain MessageLayoutRecord needs none.
    // rfl::json::write then serializes the full Record struct — CommRaT fields
    // and all — without any string-level JSON merging.
    auto output = sertial::SchemaGenerator<SERTIAL_COLLECTION_TYPE>
                      ::template generate_typed_data<SERTIAL_RECORD_TYPE>();
    std::string json = rfl::json::write(output);

    std::ofstream out(SERTIAL_OUTPUT_FILE);
    if (!out) {
        std::cerr << "sertial-schema-gen: cannot open output file: "
                  << SERTIAL_OUTPUT_FILE << "\n";
        return 1;
    }

    out << json;

    std::cout << "sertial-schema-gen: wrote " << output.messages.size()
              << " type(s) to " << SERTIAL_OUTPUT_FILE << "\n";
    return 0;
}
