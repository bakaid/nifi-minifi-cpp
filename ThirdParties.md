<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# Apache MiNiFi C++ Third Parties guide

Apache MiNiFi C++ uses many third party libraries, both for core functionality and for extensions.

This document describes the way we build and use third parties and provides a guide for adding new ones.

## Table of Contents

- [Choosing a third party](#choosing-a-third-party)
  - [When do you need a third party?](#when-do-you-need-a-third-party)
  - [License](#license)


## Choosing a third party

Deciding if a third party is needed for a particular task and if so, choosing between the different implementations is difficult. A few points that have to considered are:
 - every third party introduces risk, both operational and security
 - every third party adds a maintenance burden: it has to be tracked for issues, updated, adapted to changes in the build framework
 - not using a third party and relying on less tested homegrown solutions however usually carry a greater risk than using one
 - introducing a new third party dependency to the core should be done with the utmost care. If we make a third party a core dependency, it will increase build time, executable size and the burden to maintain API compatibility.

A few tips to choose a third party:
 - you have to choose a third party with a [proper license](#license)
 - prefer well-maintained third parties. Abandoned projects will have a huge maintenance burden.
 - prefer third parties with frequent/regular releases. There are some projects with a huge number of commits and a very long time since the last release, and we are at a disadvantage in determining whether the actual state of the master is stable: the maintainers should be the judges of that.
 - prefer third parties with the smaller number of transitive dependencies. If the third party itself needs other third parties, that increases the work greatly to get it done properly at the first time and then maintain it afterwards.

### License
Only third parties with an Apache License 2.0-compatible license may be linked with this software.

To make sure the third party's license is compatible with Apache License 2.0, refer to the [ASF 3RD PARTY LICENSE POLICY
](https://www.apache.org/legal/resolved.html). Please also note that license compatibility is a one-way street: a license may be compatible with Apache License 2.0 but not the other way round.

GPL and LGPL are generally not compatible.

## Built-in or system dependency
When deciding whether a third party dependency should be provided by the system, or compiled and shipped by us, there are many factors to consider.

|          | Advantages                                                                          | Disadvantages                                              |
|----------|-------------------------------------------------------------------------------------|------------------------------------------------------------|
| System   | Smaller executable size                                                             | Less control over third-party                              |
|          | Faster compilation                                                                  | Can't add patches                                          |
|          |                                                                                     | Has to be supported out-of-the box on all target platforms |
|          |                                                                                     | Usually not available on Windows                           |
| Built-in | High level of control over third-party (consistent version and features everywhere) | Larger executable size                                     |
|          | Can add patches                                                                     | Slower compilation                                         |
|          | Does not have to be supported by the system                                         |                                                            |
|          | Works on Windows                                                                    |                                                            |

Even if choosing a system dependency, a built-in version for Windows usually have to be made.

Both a system and a built-in version can be supported, in which case the choice should be configurable via CMake options.

**The goal is to abstract the nature of the third party from the rest of the project**, and create targets from them, that automatically take care of building or finding the third party and any dependencies, be it target, linking or include.

## System dependency

To add a new system dependency, you have to follow the following steps:

### bootstrap.sh

If you are using a system dependency, you have to ensure that the development packages are installed on the build system if the extension is selected.

To ensure this, edit `bootstrap.sh` and all the platform-specific scripts (`centos.sh`, `fedora.sh`, `debian.sh`, `suse.sh`, `rheldistro.sh`, `darwin.sh`).

### Find\<Package\>.cmake

If a `Find<Package>.cmake` is provided for your third party by not unreasonably new (not later than 3.2) CMake versions out of the box, then you have nothing further to do, unless they don't create imported targets.

If it is not provided, you have three options
 - if a newer CMake version provides it, you can try "backporting it"
 - you can search for an already implemented one in other projects with an acceptable license
 - if everything else fails, you can write one yourself

If you don't end up writing it from scratch, make sure that you indicate the original source in the `NOTICE` file.

If you need to add a `Find<Package>.cmake` file, add it as `cmake/<package>/sys/Find<Package>.cmake`, and add it to the `CMAKE_MODULE_PATH`.

### find_package

After you have a working `Find<Package>.cmake`, you have to call `find_package` to actually find the package, most likely with the REQUIRED option to set, to make it fail if it can't find it.

Example:
```
find_package(Lib<Package> REQUIRED)
```

## Built-in dependency
We thrive to build all third party dependencies using the [External Projects](https://cmake.org/cmake/help/latest/module/ExternalProject.html) CMake feature. This has many advantages over adding the third party source to our own CMake-tree with add_subdirectory:
 - ExternalProject_Add works with non-CMake third parties
 - we have greater control over what variables are passed to the third party project
 - we don't have to patch the third parties to avoid target and variable name collisions
 - we don't have to include the third party sources in our repository

There are some exceptions to using External Projects:
 - header only libraries don't require it (you could still use ExternalProject_Add to download and unpack the sources, but it is easier to just include the source in our repository and create an INTERFACE target from them).
 - there are some libraries (notably OpenCV) which generate so many targets in so many configurations and interdependencies between the targets that it is impractical to use imported targets with them
 - there are a few third parties that have not yet been converted to an External Project, but they will be, eventually

To add a new built-in dependency, the easiest way is to use an already existing one as a template.

You will need to do the following steps:
 - create `cmake/Bundled<Package>.cmake`
 - (optional) if you want to use this from other third parties, create `cmake/<package>/dummy/Find<Package>.cmake`
 - call the function created in `Bundled<Package>.cmake` in the main `CMakeLists.txt`:
     ```
     include(Bundled<Package>)
     use_bundled_<package>(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
     ```
     If you created `cmake/<package>/dummy/Find<Package>.cmake` you should also add that to the module path:
     ```
     list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/<package>/dummy")
     ```
     These should be in an extension's enabled conditional path, if the third party is only used by one extension, or in the section for third parties used my multiple packages, if used by more.
 - Link your extension with the imported third party targets. If everything is done right, dependencies, transitive library linkings and include paths should work automatically.

### ExternalProject_Add
`ExternalProject_Add` creates a custom target that will build the third party according to our configuration.

It has many options, some of which are described in greater detail later. Let's take a look at the most important ones:

#### `URL` and `GIT`
Used for fetching the source. In the case of `URL`, it is automatically unpacked. In the case of `GIT` the specified tag is checked out.

See [Choosing a source](#choosing-a-source) for greater detail.

Example:
```
GIT "https://github.com/<package>/<package>.git"
GIT_TAG "v1.0.0"
```
```
URL "https://github.com/<package>/<package>/archive/v1.0.0.tar.gz"
URL_HASH "SHA256=9b640b13047182761a99ce3e4f000be9687566e0828b4a72709e9e6a3ef98477"
```

#### `SOURCE_DIR`
The directory to which will be unpacked/cloned. Must be in the `BINARY_DIR`, so that we don't contaminate our source.

Example:
```
SOURCE_DIR "${BINARY_DIR}/thirdparty/package-src"
```

#### `PATCH_COMMAND`
Specifies a custom command to run after the source has been downloaded/updated. Needed for applying patches and in the case of non-CMake projects run custom scripts.

See [Patching](#patching) for greater detail.

#### `BUILD_BYPRODUCTS`
`ExternalProject_Add` needs to know the list of artifacts that are generated by the third party build (and that we care about), so that it can track their modification dates.

This can be usually set to the list of library archives generated by the third party.

Example:
```
BUILD_BYPRODUCTS "${<PACKAGE>_BYPRODUCT_DIR}/lib/lib<package>.lib"
```

#### `EXCLUDE_FROM_ALL`
This is required so that the custom target created by `ExternalProject_Add` does not get added to the default `ALL` target. This is something we generally want to avoid, as third party dependencies only make sense, if our code depends on them. We don't want them to be top-level targets and built unconditionally.

#### `LIST_SEPARATOR`
CMake lists [are](https://cmake.org/cmake/help/v3.12/command/list.html#introduction) `;` separated group of strings. When we pass `ExternalProject_Add` a list of arguments in `CMAKE_ARGS` to pass to the third party project, some of those arguments might be lists themselves (list of `CMAKE_MODULES_PATH`-s, for example), which causes issues.

To avoid this, when passing list arguments, the `;`-s should be replaced with `%`-s, and the `LIST_SEPARATOR` set to `%` (it could be an another character, but as `%` is pretty uncommon both in paths and other arguments, it is a good choice).

Even if you don't yourself use list arguments, many parts of the build infrastructure do, like exported targets, so to be safe, set this.

Example:
```
string(REPLACE ";" "%" LIST_ARGUMENT_TO_PASS "${LIST_ARGUMENT}")

[...]

LIST_SEPARATOR %
CMAKE_ARGS -DFOO=ON
           -DBAR=OFF
           "-DLIST_ARGUMENT=${LIST_ARGUMENT_TO_PASS}"
```

### Choosing a source
Prefer artifacts from the official release site or a reliable mirror. If that is not available, use the https links for releases from GitHub.

Only use a git repo in a last resort:
 - applying patches to git clones is very flaky in CMake
 - it usually takes longer to clone a git repo than to download a specific version

When using the `URL` download method, **always** use `URL_HASH` with SHA256 to verify the integrity of the downloaded artifact.

When using the `GIT` download method, prefer to use the textual tag of the release instead of the commit id as the `GIT_TAG`.

### Patching
Adding patches to a third party is sometimes necessary, but maintaining a local patch set is error-prone and takes a lot of work.

Before patching, please consider whether your goal could be achieved by other ways. Perhaps there is a CMake option that can disable the particular feature you want to comment out. If the third party is not the latest released version, there might be a fix upstream already released, and you can update the third party.

If after all you decide the best option is patching, please follow these guidelines:
 - keep the patch minimal: it is easier to maintain a smaller patch
 - separate logically different patches to separate patch files: if something is fixed upstream, it is easy to remove the specific patch file for it
 - place the patch files into the `thirdparty/<third party name>/` directory and use them from there
 - write ExternalProject_Add's patch step in a platform-independent way: the patch executable on the system is determined in the main CMakeLists.txt, you should use that. An example command looks like this:
   ```
   "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/<package>/<package>.patch"
   ```

### Build options
Both CMake and configure.sh based third parties usually come with many configuration options.
When integrating a new third party, these should be reviewed and the proper ones set.

Make sure you disable any parts that is not needed (tests, examples, unneeded features). Doing this has multiple advantages:
 - faster compilation
 - less security risk: if something is not compiled in, a vulnerability in that part can't affect us
 - greater control: e.g. we don't accidentially link with an another third party just because it was available on the system and was enabled by default

### find_package-like variables
When using imported targets, having the variables generated by `find_package(Package)`, like `PACKAGE_FOUND`, `PACKAGE_LIBRARIES` and `PACKAGE_INCLUDE_PATHS` is not necessary, because these are already handled by the imported target's interface link and include dependencies.

However, these are usually provided by built-in packages, for multiple reasons:
 - backwards compatibility: proprietary extensions might depend on them (for already existing third parties)
 - defining these is required for importing the target, and defining its link and include interface dependencies, so we might just add them
 - if we want to export this third party to other third parties, the dummy `Find<Package>.cmake` will require these variables anyway

### Imported targets



### Exporting to other third parties

### Using third parties in other third parties

#### Dependencies
#### Compilation
#### Linking