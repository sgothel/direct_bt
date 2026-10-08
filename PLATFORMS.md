# Direct-BT LE and BREDR Library

[Original document location](https://jausoft.com/cgit/direct_bt.git/about/).

## Supported Platforms
Minimum language requirements
- C++20 or better, see [jaulib C++ Minimum Requirements](https://jausoft.com/projects/jaulib/README.md#cpp_min_req).
- Standard C Libraries
  - [FreeBSD libc](https://www.freebsd.org/)
  - [GNU glibc](https://www.gnu.org/software/libc/)
  - [musl](https://musl.libc.org/)
- Java 11 (optional)

The following **platforms** are tested and hence supported

**FreeBSD 13.1 (FreeBSD)**
- *Compile clean only, Bluetooth not working*
- libc++ 13.0.0
- compiler
  - clang 13.0.0
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Alpine Linux 3.16 (Linux)**
- linux 5.15
- [musl 1.2.3](https://musl.libc.org/)
- compiler
  - gcc 11.2.1
  - clang 13.0.1
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Debian 13 Trixie (GNU/Linux)**
- linux 6.12
- glibc 2.41
- compiler
  - gcc 14.2.0
  - clang 19.1.7
  - openjdk 21
- architectures
  - amd64 (validated, Generic)

**Debian 12 Bookworm (GNU/Linux)**
- linux 5.19
- glibc 2.35
- compiler
  - gcc 12.2.0
  - clang 16.0.6
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Ubuntu 24.04 (GNU/Linux)**
- linux 6 (?)
- glibc 2.41 (?)
- compiler
  - gcc 14 (13 is default)
  - clang 19 (18 is default)
  - openjdk 21
- architectures
  - amd64 (validated, Generic)

**Ubuntu 22.04 (GNU/Linux)**
- linux 5.15
- glibc 2.35
- compiler
  - gcc 12 (11 is default)
  - clang 19 (18 is default)
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

