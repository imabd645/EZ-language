#ifndef EZ_VERSION_H
#define EZ_VERSION_H

// The single source of truth for the interpreter version.
//
// Everything that reports a version -- `ez --version`, `ez help`, the
// bundler's metadata -- reads it from here. Previously no constant existed
// at all and the version only appeared in the website copy, which is how it
// drifted out of step with the actual release.
#define EZ_VERSION_MAJOR 5
#define EZ_VERSION_MINOR 1
#define EZ_VERSION_PATCH 0

#define EZ_STRINGIFY_(x) #x
#define EZ_STRINGIFY(x) EZ_STRINGIFY_(x)

#define EZ_VERSION_STRING \
    EZ_STRINGIFY(EZ_VERSION_MAJOR) "." \
    EZ_STRINGIFY(EZ_VERSION_MINOR) "." \
    EZ_STRINGIFY(EZ_VERSION_PATCH)

#endif // EZ_VERSION_H
