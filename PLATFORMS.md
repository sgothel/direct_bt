# Direct-BT LE and BREDR Library

[Original document location](https://jausoft.com/cgit/direct_bt.git/about/).

## Supported Platforms
Minimum language requirements
- C++17
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

**Debian 12 Bookworm (GNU/Linux)**
- linux 5.18
- glibc 2.33
- compiler
  - gcc 11.3.0
  - clang 13.0.1
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Debian 11 Bullseye (GNU/Linux)**
- linux 5.10
- glibc 2.31
- compiler
  - gcc 10.2.1
  - clang 11.0.1
  - openjdk 17
- architectures
  - amd64 (validated, Generic)
  - arm64 (validated, Raspberry Pi 3+ and 4)
  - arm32 (validated, Raspberry Pi 3+ and 4)

**Debian 10 Buster (GNU/Linux)**
- *deprecated*
- linux 4.19 (amd64), 5.10 (raspi)
- glibc 2.28
- compiler
  - gcc 8.3.0
  - openjdk 11
- architectures
  - amd64 (validated, Generic)
  - arm64 (validated, Raspberry Pi 3+ and 4)
  - arm32 (validated, Raspberry Pi 3+ and 4)
- potential issues with *capsh*, see below.

**Ubuntu 22.04 (GNU/Linux)**
- linux 5.15
- glibc 2.35
- compiler
  - gcc 11.2.0
  - clang 14.0.0
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Ubuntu 20.04 (GNU/Linux)**
- linux 5.4
- glibc 2.31
- compiler
  - gcc 9.4.0
  - clang 10.0.0
  - openjdk 17
- architectures
  - amd64 (validated, Generic)

**Ubuntu 18.04 (GNU/Linux)**
- *deprecated*
- compiler
  - gcc 8.3
  - openjdk 11
- architectures
  - amd64 (validated, Generic)
- potential issues with *capsh*, see below.

