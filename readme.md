# Cross-Platform JPEG Viewer in C

This project is made to fulfill the requirements for the postgraduate course 'Techniques and Technologies for Scientific Software Engineering'.

We were allowed to pick any project by ourselves, requiring mainly that it challenges us at our current skill level.

Additional formal requirements are (non-exhaustive):
1. There should be at least one secondary branch in the repo, providing some feature or addition that's not covered in the report.
   - present in [cmake-build](https://github.com/slaide/c-rt/tree/cmake-build), which offers an alternative build system: CMake instead of makefile.
2. There should be a clear, defined open-source license for the repository that is compatible with any dependencies and pre-existing code or other assets that are used.
    - TODO
3. There should be at least one github issue that's been resolved with an associated code change (commit or pull request).
    - see [issue #1](https://github.com/slaide/c-rt/issues/1).
4. There should be some use of distributed or parallel computing within the project.
    - this project makes use SIMD instructions and multi-threading.
