#include <gmock/gmock.h>
#include <string>
#include <list>
#include <vector>
#include "src/pos_replicator/pos_replicator_io_completion.h"

namespace pos
{
class MockPosReplicatorIOCompletion : public PosReplicatorIOCompletion
{
public:
    using PosReplicatorIOCompletion::PosReplicatorIOCompletion;
    MOCK_METHOD(bool, _DoSpecificJob, (), (override));
};

} // namespace pos
