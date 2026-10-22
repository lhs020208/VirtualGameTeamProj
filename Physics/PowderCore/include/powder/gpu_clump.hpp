#pragma once
#include "powder/clump_worker.hpp"
namespace powder {
// Fresh-state, dry linear spheres and verified boxes only. Throws on unsupported physics.
std::shared_ptr<ClumpBackend> make_gpu_backend(const ClumpWorld&);
}
