# Cross-Platform JPEG Viewer in C

## User Guide

### Compilation

This project mainly uses makefile for building. There is a [branch][cmake-branch] that uses CMake instead. It lags behind the main branch a bit, but has build feature parity.

This project works on MacOS/ARM64 and Linux/x64 (it requires a GPU with vulkan support on Linux). The build script detects the host OS, so the quickest way to compile is to just run `make` in the project root, which puts the executable and other required files into the `bin` subdirectory. This project has the following build-time dependencies:
- A version of the clang compiler that supports the gnu2x standard.
- Vulkan header files: available on most package managers, e.g. [here](https://archlinux.org/packages/core/x86_64/linux-headers/), via the [Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools), or on [github](https://github.com/KhronosGroup/Vulkan-Headers).
- Linux-specific: libxcb (header files + library). Available in most package managers, e.g. [here](https://archlinux.org/packages/extra/x86_64/libxcb/).
- MacOS-specific: MoltenVK, a Vulkan compatibility layer for MacOS on Apple Silicon. These include the vulkan header files. Available via [homebrew](https://formulae.brew.sh/formula/molten-vk) or on [github](https://github.com/KhronosGroup/MoltenVK#developing_vulkan).

There are multiple features available that can be set at compile-time:
1. Increased decoding precision: By default, the jpeg decoder uses fixed-point arithmetic to speed up computations. The compiler flag `HIGH_PRECISION=YES` enables floating point precision. This slows down decoding by about 10-20%.
2. Parallel decoding: By default, the jpeg decoder runs on a single thread. The `DECODE_PARALLEL=YES` flag enables pipelining using 4 threads (main + 3 workers), which roughly halves decoding time.
3. jemalloc allocator: The libc allocator can be replaced with the jemalloc allocator, if installed on the host (does not ship with this repository), by using the `JEMALLOC=YES` flag. This makes the time to decode an image more consistent (little variation between the 1st and 5th image decoding at runtime), but it reduces the best-case performance by about 10%.
4. `Release` build mode: By default, the project is compiled in debugrelease mode, which includes many optimisations but also some debug information. This most notably includes parsing each image 5 times, printing the time taken for each iteration in the terminal. Setting `MODE=release` will enable more optimisations (in practice, for no additional speedup), parse each image only once and not time the decoding process.

These features can be arbitrarily combined. The decoder is not compatible with all possible jpeg images, but should support most. Some of the optimisations are specific to certain jpeg encoding schemes, so some images may be slower to decode than others of similar size.

The CMake version also supports these features, though the flags there are implemented as CMake `option`s. The default build mode there is `RelWithDebInfo` (the equivalent of `debugrelease` in makefile), and the release mode is called `Release`.

### Running the application

The `main` executable requires at least one argument to run, a path to a valid jpeg file. The application needs to be run with the `bin` directory as working directory to resolve the runtime dependency on the shader files. The jpeg file path does not have this restriction.

The application supports scroll-to-zoom on the image, and drag-to-move. Pressing the 'R' key will reset the image view (i.e. reset the position and size to center it in the window). The application also supports resizing the window.

Additionally, the application supports multiple arguments (at least 1 is required), all of which must be valid jpeg file paths. The application only displays one image at a time, and you can switch between the images using the right/left arrow keys. Moving and scaling each image is done separately, they don't share these properties. The Window title changes to display the name of the currently visible image. The 'R' key resets the view of all images, not just the currently visible one. [The CMake version only supports a single argument, and does not support the "press 'R' to reset view" feature.]

### Testing

One of my goals for this project was to write a jpeg parser than can compete with libjpeg's decompression speed. For this purpose, there is a makefile target called `test`. That target compiles a simple C application that uses libjpeg (it is therefore a requirement to have libjpeg installed), and this application, and runs both with all images in the `bin/images` directory (this path is hardcoded in the makefile). Both applications measure the time to decompress each image 5 times, and print it to the terminal. The build command accepts all build options mentioned above.

We have used this script with the [test images](https://drive.google.com/drive/folders/1eGyp0XP7DvyJD8yVl6GLlflGLXQac2kW?usp=sharing) linked in the section below. In combination with the multi-argument functionality this can be used to quickly evaluate the time taken to decompress these images and also investigate the decoded images visually.

## Context
This project is made to fulfill the requirements for the postgraduate course 'Techniques and Technologies for Scientific Software Engineering'.

We were allowed to pick any project by ourselves, requiring mainly that it challenges us at our current skill level.

Additional formal requirements are (non-exhaustive):
1. There should be at least one secondary branch in the repo, providing some feature or addition that's not covered in the report.
   - Present in [cmake-build][cmake-branch], which offers an alternative build system: CMake instead of makefile.
2. There should be a clear, defined open-source license for the repository that is compatible with any dependencies and pre-existing code or other assets that are used.
    - Licensed under GPL3. All code in this repository is my original work. Dependencies and test data are available externally.
3. There should be at least one github issue that's been resolved with an associated code change (commit or pull request).
    - See [issue #1][#4].
    - Also [#4][#4] and [#5][#5].
4. There should be some use of distributed or parallel computing within the project.
    - This project makes use SIMD instructions and multi-threading.
5. There should be at least five tests using some testing framework for correctness, robustness to spurious input, and performance.
    - Test images available on [Google Drive](https://drive.google.com/drive/folders/1eGyp0XP7DvyJD8yVl6GLlflGLXQac2kW?usp=sharing) because they are not my original work and may not be compatible with license of this repository.

## License
This project is licensed under the GNU GPLv3 License - see the [LICENSE](LICENSE.txt) file for details.

[cmake-branch]: https://github.com/slaide/c-rt/tree/cmake-build
[#1]: https://github.com/slaide/c-rt/issues/1
[#4]: https://github.com/slaide/c-rt/issues/4
[#5]: https://github.com/slaide/c-rt/issues/5
