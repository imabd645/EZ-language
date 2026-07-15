#ifndef EZUPVALUE_H
#define EZUPVALUE_H

#include "runtime/Value.h"
#include <atomic>
#include <memory>

// Upvalue instances are co-owned via shared_ptr: closures own the upvalues they
// capture, and the VM's open-upvalue list owns them (via `next`) while they are
// still open. This lets a closure that escapes its creating VM (returned from a
// spawn()/async worker, stored in a global, etc.) keep its upvalues alive after
// that VM — and its stack — is gone. The VM closes any still-open upvalues in
// its destructor so escaped closures become self-contained (value in `closed`).
struct UpvalueObj {
    std::atomic<Value*> location;         // Points to stack slot (open) or &closed (closed)
    Value  closed;                        // When closed, location == &closed
    std::shared_ptr<UpvalueObj> next;     // Next in the VM's open-upvalue list
};

#endif // EZUPVALUE_H
