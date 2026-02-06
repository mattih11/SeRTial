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
#include "defines/sensor_history.hpp"  // RingBuffer example
#include "messages/messages.hpp"

using namespace sertial;
using namespace examples::defines;
using namespace examples::messages;

// ============================================================================
// Step 1: Define your MessageCollection
// ============================================================================

struct ComplexData
{
    int id;
    sertial::fixed_string<64> name;
    sertial::fixed_vector<float, 10> values;
    int32_t group_id;
    sertial::fixed_vector<float, 10> values2;
    Timestamp<> timestamp;
    int16_t flags;
    int8_t status;
};


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
    ImuDouble,
    SensorHistory,  // RingBuffer example
    ComplexData
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
