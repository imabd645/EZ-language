#ifndef EZ_RECURSIVE_TEARDOWN_H
#define EZ_RECURSIVE_TEARDOWN_H

#include "runtime/Value.h"
#include <vector>

// Several EZ object kinds (arrays, tuples, dictionaries, instances, and
// concat-strings) hold their children as plain Value members/elements, and
// none of them have a custom destructor that changes how those children are
// torn down. That's fine for ordinary, shallow data -- but it means a long,
// singly-owned CHAIN of them (a linked list built via `self.next = Node(...)`,
// or a "list" built as `arr = [arr]` N times) recurses one C++ stack frame
// per link when the head is finally destroyed: each Value's destructor drops
// a shared_ptr to zero, which invokes the child object's destructor, which
// drops ITS child's shared_ptr to zero, and so on. A large enough structure
// overflows the OS stack and crashes the whole interpreter with a bare
// segfault -- no EZ-level error, no traceback. A plain linked list of
// ~100,000 nodes (verified) is enough to hit this; that's an ordinary size
// for real data, not an adversarial one.
//
// ezIterativelyReleaseChildren() is the fix, applied uniformly instead of
// hand-writing (and risking subtly re-breaking) the same worklist-based
// teardown once per container type. A type's destructor calls this with
// pointers to each of its own Value-holding children; this function detaches
// any child that is a recognized recursive-container type AND for which the
// caller is the last remaining owner (never touches a value something else
// still needs), and drains the resulting worklist iteratively rather than
// via the C++ call stack.
//
// Declared here (before EZArray.h/EZDictionary.h/EZInstance.h/EZTuple.h are
// included) so their destructors can call it; defined in
// RecursiveTeardownImpl.h, included at the end of EZObjects.h once all of
// those types -- which the implementation needs the full definitions of --
// are available.
void ezIterativelyReleaseChildren(std::vector<Value*> children);

#endif // EZ_RECURSIVE_TEARDOWN_H
