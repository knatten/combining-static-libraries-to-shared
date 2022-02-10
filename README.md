# Making a shared library from a collection of sub libraries

Let's say you have a bunch of static libraries that you want to bundle up and ship as a shared library. Something like
this:

```
   |------|
   |shared|
   |------|
    /    \
|----|  |----|
|lib1|  |lib2|
|----|  |----|
```

In order for that to work, there is some normally useful behaviour of the linker that we need to overcome, in order for
all the symbols of the sub libraries to actually be included in the final shared library.

When you link a binary (an executable or a shared library) against an object file (`.o`), everything in that object file
becomes part of the binary. However, when you link a binary against a *static* library (`.a` / `.lib`), only the things
you actually *use* become part of the binary. The rest can be omitted, resulting in a (sometimes much) smaller binary.

However, if your binary is a shared library which will serve as a bundle of all the sub libraries, this can become
problematic. If there is a function in one of the sub libraries which is supposed to be part of the interface of the
shared library, but it is not actually *used* (directly or transitively) by the shared library, the linker will just
omit it, assuming that it's not needed.

This article looks at two possible ways of solving that issue, to make sure all the functions from the sub libraries
become included in the shared library.

## The difference between linking to object files and linking to static libraries

As an example, say you have a static library [StaticLib](object-libs/src/StaticLib/CMakeLists.txt), with two source
files:

```cmake
project(StaticLib)
add_library(${PROJECT_NAME} STATIC staticLib1.cpp staticLib2.cpp)
```

In [staticLib1.cpp](object-libs/src/StaticLib/staticLib1.cpp) we define two functions `staticLib1a()`
and `staticLib1b()`:

```c++
void staticLib1a() {}
void staticLib1b() {}
```

In [staticLib2.cpp](object-libs/src/StaticLib/staticLib2.cpp) we define two functions `staticLib2a()`
and `staticLib2b()`:

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
and `staticLib2b()` here, since they are not referenced (directly or transitively) from `main.cpp`. You probably also
guessed that you'd see `staticLib1a()` here, since that one is called from `main()`. But why is `staticLib1b()` there?

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
were linking directly to object files rather than to static libraries. There are (at least) two ways to solve this:

- CMake object libraries
- Using `--whole-archive`

It's probably also possible to solve this with linker scripts, and approach I didn't try.

## CMake object libraries

One seemingly very promising solution to this is to use CMake's "Object libraries".

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

Let's make a new project [ObjectLib](object-libs/src/ObjectLib), which is identical
to [StaticLib](object-libs/src/StaticLib), except that the files are named `objectLib*` instead of `staticLib*`, and the
four functions are named `objectLib[12][ab]` instead of `staticLib[12][ab]`. We also make this an object library, by
simply passing `OBJECT` instead of `STATIC`
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

Linking to an object library works exactly (so far, but see below for a problem) like linking to a static library.
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

### Problems with object libraries

Normally, if a static library `lib1` depends on `lib2`, and you link a binary `bin` against `lib1`, you get both `lib1`
and `lib2` passed to the linker for `bin`, which is what you want. For object libraries, you only get `lib1` on the
command line. So in case of a dependency tree, this does not work well at all.

```
|----|
|bin1|
|----|
  |
|----|
|lib1|
|----|
  |
|----|
|lib2|
|----|
```

In the figure above, `bin1` links to both `lib1` and `lib2` in the case of static libraries, but only to `lib1` in the
case of object libraries!

This has been a known problem for years, for instance
in [this issue](https://gitlab.kitware.com/cmake/cmake/-/issues/18090). What's holding them back is that it's an error
to link twice to the same symbol in `.o` files, but if you link twice to the same symbol in `.a` files, it just picks
the first one it finds. So if `lib1` and `lib2` both depend on `lib3`, it's no problem to link a binary `bin` which
depends on `lib1` and `lib2` and then gets `lib3` from both of them. But if these are all object libraries, `bin` now
gets the object files from `lib3` passed multiple times, which is an error. Figure:

```
   |------|
   |shared|
   |------|
    /    \
|----|  |----|
|lib1|  |lib2|
|----|  |----|
    \   /
    |----|
    |lib3|
    |----|
```

There are some workarounds to this listed in the issue, but they don't seem to be working very well. Let's investigate
our second option:

## Using `--whole-archive`

`man ld` says it best:

> --whole-archive
>
> For each archive mentioned on the command line after the `--whole-archive` option, include every object file in the
> archive in the link, rather than searching the archive for the required object files. *This is normally used to turn
> an archive file into a shared library* [my emphasis]

This sounds like exactly what we want. We just have to make sure to turn it off again, so we only enable it for our own
sub libraries, and not for instance all of boost which we might only be using a few functions from. Again, `man ld`:

> --no-whole-archive
>
>   Turn off the effect of the `--whole-archive` option for subsequent archive files.

So we need to convince CMake to output a linker command which puts all the sub libraries of our shared library
between `--whole-archive` and `--no-whole-archive`, and other libraries out of those.

Let's look at a similar project to the previous one. This time we're trying to make a `SharedLib` from the static
library `SharedLibWithChild` and its child `SharedLibChild`:

```
    |---------|
    |SharedLib|
    |---------|
         |
|------------------|
|SharedLibWithChild|
|------------------|
         |
  |--------------|
  |SharedLibChild|
  |--------------|
```

This project is similar to the previous one, in that `StaticLibWithChild` has two source files with two functions each:
In [staticLibWithChild1.cpp](whole-archive/src/StaticLibWithChild/staticLibWithChild1.cpp):

```c++
void staticLibWithChild1a() { staticLibChild1a(); }
void staticLibWithChild1b() {}
```

In [staticLibWithChild2.cpp](whole-archive/src/StaticLibWithChild/staticLibWithChild2.cpp):

```c++
void staticLibWithChild2a() {}
void staticLibWithChild2b() {}
```

Then we also have `StaticLibChild` with two source files:

In [staticLibChild1.cpp](whole-archive/src/StaticLibChild/staticLibChild1.cpp):

```c++
void staticLibChild1a() {}
void staticLibChild1b() {}
```

In [staticLibChild2.cpp](whole-archive/src/StaticLibChild/staticLibChild2.cpp):

```c++
void staticLibChild2a() {}
void staticLibChild2b() {}
```

Then in [SharedLib/sharedLib.cpp](whole-archive/src/SharedLib/sharedLib.cpp), we use `staticLibWithChild1a`:

```c++
void sharedLib() {
    staticLibWithChild1a();
}
```

Notice that `staticLibWithChild1a` calls `staticLibChild1a` from the lowermost library, but none of the other functions
in the static libs call anything. So `staticLibChild1a` is transitively needed by `SharedLib`, but neither of the other
functions are.

[SharedLib](whole-archive/src/SharedLib/CMakeLists.txt) depends on `StaticLibWithChild`:

```cmake
project(SharedLib)

add_library(${PROJECT_NAME} SHARED sharedLib.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC StaticLibWithChild)
```

This works, `SharedLib` now depends on `StaticLibWithChild`, but also on `StaticLibChild`, since `StaticLibWithcChild`
depends on `StaticLibChild` publicly:

[StaticLibWithChild](whole-archive/src/StaticLibWithChild/CMakeLists.txt):

```cmake
target_link_libraries(${PROJECT_NAME} PUBLIC StaticLibChild)
```

So when we link `SharedLib` and list the included symbols, we get:

```
$ nm -C whole-archive/clion-debug/SharedLib/libSharedLib.so |grep staticLib
0000000000001164 T staticLibChild1a()
000000000000116f T staticLibChild1b()
0000000000001149 T staticLibWithChild1a()
0000000000001159 T staticLibWithChild1b()
```

This is similar to what we've seen before. `staticLibWithChild1a` is called from `SharedLib`, so it gets included in the
binary. `staticLibWithChild1b` gets included because it's in the same section as `staticLibWithChild1a`
. `staticLibChild1a` gets included because it's called from `staticLibWithChild1a`, even though it's not called directly
from anywhere in `SharedLib` itself. And `staticLibChild1b` gets included because it's in the same section.

The functions ending in `2` are not included since they're never called from `SharedLib`, neither directly nor
transitively.

## TODO

- Ensure that build order is enforced. When linking to a library target name, build order will be enforced. I'm not sure
  that this happens for generator expressions
- Ensure that rebuilds are enforced when dependencies change. When linking to a library target name, rebuilds are
  enforced. I'm not sure that this happens for generator expressions
- Make sure we get the correct include directories set. I'm not sure that this happens for generator expressions.
- Note, the INTERFACE_LINK_LIBRARIES property of a target does not seem to include the target itself.
- Note, it seems to work to put a library both inside --whole-archive and after it, but not before it. Makes sense since
  it will then have resolved symbols and then find them again and being asked to include all of them. Check on Windows
  too
- Note, maybe it's a good idea to have two calls to target_link_libraries, one with the ones we actually depend on, so
  that we get include directories and build order guaranteed (at least the former seems to be needed), and one for the
  whole-archive stuff. As long as we put the whole-archive stuff first, it seems to work fine, at least on Linux. Then
  the whole archive thing can also be PRIVATE. And it's nice that we can simply add a bunch of these in a loop when we
  know which public dependencies exist.
- Note, https://cmake.org/cmake/help/latest/command/target_link_libraries.html says that one can
  call `target_link_libraries` from another directory than the one the target was created in. So we can do this from a
  function somewhere probably! As long as the shared library target has been created before we do it of course.
- Maybe see in zivid-sdk if I can simply iterate over EXPOSED_LIBRARIES in Core and do this trick and see if it just
  works?