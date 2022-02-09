# CMake object libraries

When you link a binary against an object file (`.o`), everything in that object file becomes part of that binary.
However, when you link against a static library (`.a` / `.lib`), only the things you actually *use* become part of the
binary.

Normally in CMake, a binary is built up from static libraries, and only what is used from the libraries gets included in
the binary. By using CMake **object libraries**, you can change that, so *all* the contents of the libraries get
included in the binary. This is useful if you for instance want to build a shared library which is just a combination on
a bunch of static libraries.

## The difference between linking to object files and linking to static libraries

As an example, say you have a static library [StaticLib](object-libs/src/StaticLib/CMakeLists.txt), with two source files:

```cmake
project(StaticLib)
add_library(${PROJECT_NAME} STATIC staticLib1.cpp staticLib2.cpp)
```

In [staticLib1.cpp](object-libs/src/StaticLib/staticLib1.cpp) we define two functions `staticLib1a()` and `staticLib1b()`:

```c++
void staticLib1a() {}
void staticLib1b() {}
```

In [staticLib2.cpp](object-libs/src/StaticLib/staticLib2.cpp) we define two functions `staticLib2a()` and `staticLib2b()`:

```c++
void staticLib2a() {}
void staticLib2b() {}
```

Let's say you're also building an executable [Main](object-libs/src/Main/CMakeLists.txt):

```cmake
project(Main)
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} StaticLib)
```

And in [main.cpp](object-libs/src/Main/main.cpp) we call `staticLib1a()` from `StaticLib`:

```c++
#include "staticLib1.h"

int main()
{
    staticLib1a();
}
```

Based on the above, if you now list the symbols in `Main`, which symbols from `StaticLib` do you think are included?

```
$ nm -C clion-debug/Main/Main | grep staticLib
0000000000001169 T staticLib1a()
0000000000001174 T staticLib1b()
```

Based on the opening paragraph in this article, you probably correctly expected to not see `staticLib2a()`
and `staticLib2b()` here, since they are not referenced in `main.cpp`. You probably also guessed that you'd
see `staticLib1a()` here, since that one is called from `main()`. But why is `staticLib1b()` there?

Let's first quickly look at how a linker works. `Main` links to `main.o`, which contains `main()`. It also links
to `libStaticLib.a`, which contains all the `staticLib...` functions. The linker now reads through `main.o` and includes
all the symbols from it in the `Main` binary. Here it finds the definition for the symbol `main`. It also finds that the
symbol `staticLib1a` is needed, but not defined. It then looks through `libStaticLib.a` for any *needed* symbols which
have not yet been resolved. Here it finds `staticLib1a()`, so it decides it needs to include the definition of that
function in the `Main` binary.

The linker operates on the level of *sections*, not individual functions and variables. Functions from the same source
file typically ends up in the same section, and that happens here too. `staticLib1a` and `staticLib1b` both end up in
the `.text` segment of `staticLib1.o`. Similarly, `staticLib2a` and `staticLib2b` both end up in the `.text` segment
of `staticLib2.o`.

```
|==============|   |==============|
| staticLib1.o |   | staticLib2.o |
|==============|   |==============|
| .text        |   | .text        |
|--------------|   |--------------|
| staticLib1a  |   | staticLib2a  |
| staticLib1b  |   | staticLib2b  |
|==============|   |==============|
| .data        |   | .data        |
|--------------|   |--------------|
| (...)        |   | (...)        |
|==============|   |==============|
```

`libStaticLib.a` is just an archive of those two object files, so we still have those two separate `.text` sections with
the functions from each object file. You can think of it like this, just the two object files concatenated into one
file:

```
|==============|
| staticLib1.o |
|==============|
| .text        |
|--------------|
| staticLib1a  |
| staticLib1b  |
|==============|
| .data        |
|--------------|
| (...)        |
|==============|
| staticLib2.o |
|==============|
| .text        |
|--------------|
| staticLib2a  |
| staticLib2b  |
|==============|
| .data        |
|--------------|
| (...)        |
|==============|
```

So you can see how `staticLib1a()` and `staticLib1b()` goes in one `.text` section, and `staticLib2a()`
and `staticLib2b()` goes in another `.text` section.

Now let's get back to our linking of `Main`. We left off just as it was looking through `libStaticLib.a` and found a
definition for the needed `staticLib1a()`. Since the linker operates on sections rather than individual functions, it
then has to include the entire `.text` section that contains `staticLib1a()` in order to link it into `Main`. And since
that section is also where `staticLib1b()` is defined, it too ends up in `Main`, even though it's not needed anywhere.
On the other hand, neither `staticLib2a` nor `staticLib2b` are needed when linking `Main`, so that entire section gets
left out, and we save some space in `Main`.

## Making a shared library

The above is all good if one wants to build an executable. The executable includes all the definitions the executable
needs in order to run, and some stuff we didn't need was excluded. However, what if we instead are building a shared
library, which is composed of a set of separate static libraries? The shared library itself might not have any code in
it which references all the functions we want to expose in the shared library. If we want all the four `staticLib...`
functions to be part of the API of that shared library, but there's no code in the shared library that
references `staticLib2a` nor `staticLib2b`, the definitions of those functions will be excluded from `libSharedLib.so`,
and the consumers of it will not be able to use them!

In this case, we need to tell the linker to include all symbols from the linked to libraries, just as it would if we
were linking directly to object files rather than to static libraries. And what better way to do that than to actually
*do* link to object files? This is where CMake's "Object libraries" come in.

In a normal static library, each `*.cpp` / `*.c` file turns into one `*.o` file. Then all of those are concatenated into
a `*.a`. Given this [CMakeLists.txt](object-libs/src/StaticLib/CMakeLists.txt):

```cmake
project(StaticLib)
add_library(${PROJECT_NAME} STATIC staticLib1.cpp staticLib2.cpp)
```

We get these files:

```
$ find StaticLib/ -name '*.o' -o -name '*.a'
StaticLib/CMakeFiles/StaticLib.dir/staticLib1.cpp.o
StaticLib/CMakeFiles/StaticLib.dir/staticLib2.cpp.o
StaticLib/libStaticLib.a
```

Two object files (`*.o`), and one archive file (`*.a`) containing the two object files.

Let's make a new project [ObjectLib](object-libs/src/ObjectLib), which is identical to [StaticLib](object-libs/src/StaticLib), except that the
files are named `objectLib*` instead of `staticLib*`, and the four functions are named `objectLib[12][ab]` instead
of `staticLib[12][ab]`. We also make this an object library, by simply passing `OBJECT` instead of `STATIC`
to `add_library`, like in [CMakeLists.txt](object-libs/src/ObjectLib/CMakeLists.txt):

```cmake
project(ObjectLib)
add_library(${PROJECT_NAME} OBJECT objectLib1.cpp objectLib2.cpp)
```

Then we get these files, notice there's no `.a` here.:

```
$ find ObjectLib/ -name '*.o' -o -name '*.a'|xargs ls -l
-rw-rw-r-- 1 knatten knatten 3200 feb.   2 14:42 ObjectLib/CMakeFiles/ObjectLib.dir/objectLib1.cpp.o
-rw-rw-r-- 1 knatten knatten 3200 feb.   2 14:42 ObjectLib/CMakeFiles/ObjectLib.dir/objectLib2.cpp.o
```

Linking to an object library works exactly like linking to a static library.
In [object-libs/src/SharedLib/CMakeLists.txt](object-libs/src/SharedLib/CMakeLists.txt):

```cmake
target_link_libraries(${PROJECT_NAME} StaticLib)
target_link_libraries(${PROJECT_NAME} ObjectLib)
```

Now when we link this, you can already see a difference on the command line to how the libraries are linked against:

```
/usr/bin/g++-10 -fPIC -g   -shared -Wl,-soname,libSharedLib.so -o SharedLib/libSharedLib.so \
ObjectLib/CMakeFiles/ObjectLib.dir/objectLib1.cpp.o ObjectLib/CMakeFiles/ObjectLib.dir/objectLib2.cpp.o \
SharedLib/CMakeFiles/SharedLib.dir/sharedLib.cpp.o \
StaticLib/libStaticLib.a
```

Notice here that it's directly linking `objectLib1.cpp.o` and `objectLib2.cpp.o` from the object library, but instead
linking just to the library `libStaticLib.a` for the static library. As mentioned above, when you link directly against
an object file, all of its definitions are included, but when you link against a library, only the needed definitions
are included. So there's no wonder then when we list the symbols in `libSharedLib.so`:

```
$ nm -C SharedLib/libSharedLib.so | grep -E \(object\|static\)Lib
0000000000001139 T objectLib1a()
0000000000001144 T objectLib1b()
000000000000114f T objectLib2a()
000000000000115a T objectLib2b()
000000000000117a T staticLib1a()
0000000000001185 T staticLib1b()
```

We can see that all the functions from the object library were included, but only the needed functions (and functions
that happened to be in the same section as needed functions) from the static library.

So if you want to build a shared library from a bunch of static libraries, object libraries might be the way to go.

## Problems

Normally, if a static library `lib1` depends on `lib2`, and you link a binary `bin` against `lib1`, you get both `lib1`
and `lib2` passed to the linker for `bin`, which is what you want. For object libraries, you only get `lib1` on the
command line. So in case of a dependency tree, this does not work well at all.

This has been a known problem for years, for instance
in [this issue](https://gitlab.kitware.com/cmake/cmake/-/issues/18090). What's holding them back is that it's an error
to link twice to the same symbol in `.o` files, but if you link twice to the same symbol in `.a` files, it just picks
the first one it finds. So if `lib1` and `lib2` both depend on `lib3`, it's no problem to link a binary `bin` which
depends on `lib1` and `lib2` and then gets `lib3` from both of them. But if these are all object libraries, `bin` now
gets the object files from `lib3` passed multiple times, which is an error.
