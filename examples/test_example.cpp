/// Example: Runtime Serialization Testing with One-Liner API
///
/// This demonstrates how to test serialization round-trips for your message
/// types using SeRTial's templated interface.
///
/// Build: cmake --build build --target test_example
/// Run:   ./build/test_example

#include <sertial/sertial.hpp>
#include <sertial/integration/message_collection.hpp>
#include <sertial/integration/runtime_test.hpp>

// Include your message definitions
#include "defines/defines.hpp"
#include "messages/messages.hpp"

using namespace sertial;
using namespace examples::defines;
using namespace examples::messages;

// ============================================================================
// Step 1: Define your MessageCollection (same as schema generation)
// ============================================================================

struct ComplexData {
    uint32_t id;
    sertial::fixed_string<64> name;
    sertial::fixed_vector<float, 128> values;
    int32_t group_id;
    sertial::fixed_vector<float, 128> values2;
    Timestamp<> timestamp;
};

using MyMessages = MessageCollection<
    // Basic field types
    Point3D<float>,
    Point3D<double>,
    Quaternion<float>,
    Timestamp<>,
    
    // Full message types
    Header<>,
    Position<>,
    PointCloud<>,
    PointCloudSmall,
    CameraInfo<>,
    Imu<>,
    ComplexData
>;

// ============================================================================
// Step 2: Run tests with one line!
// ============================================================================

int main() {
    // One-liner test execution!
    auto results = RuntimeTest<MyMessages>::run_all();
    
    // Exit with appropriate code
    return results.all_passed() ? 0 : 1;
}
