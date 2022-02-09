#include "standaloneStaticLib1.h"
#include "staticLibWithChild1.h"

void sharedLib() {
    standaloneStaticLib1a();
    staticLibWithChild1a();
}
