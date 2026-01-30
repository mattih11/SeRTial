/// Example: Schema Generation with One-Liner API
///
/// This demonstrates how to generate JSON schemas for your message types
/// using SeRTial's templated interface.
///
/// Build: cmake --build build --target schema_example
/// Run:   ./build/schema_example [output.json]

#include <sertial/sertial.hpp>
#include <sertial/integration/message_collection.hpp>
#include <sertial/integration/schema_generator.hpp>

// Include your message definitions
#include "defines/defines.hpp"
#include "messages/messages.hpp"

using namespace sertial;
using namespace examples::defines;
using namespace examples::messages;

// ============================================================================
// Step 1: Define your MessageCollection
// ============================================================================

// Register all message types you want to generate schemas for
using MyMessages = MessageCollection<
    // Basic field types
    Point3D<float>,
    Point3D<double>,
    Quaternion<float>,
    Timestamp<>,
    
    // Full message types  
    Header<>,
    Position<>,
    PositionDouble,
    PointCloud<>,
    PointCloudSmall,
    PointCloudMedium,
    CameraInfo<>,
    Imu<>,
    ImuDouble
>;

// ============================================================================
// Step 2: Generate schemas with one line!
// ============================================================================

int main(int argc, char* argv[]) {
    std::string output_file = "my_schemas.json";
    if (argc > 1) {
        output_file = argv[1];
    }
    
    // One-liner schema generation!
    bool success = SchemaGenerator<MyMessages>::write_verbose(output_file);
    
    return success ? 0 : 1;
}
