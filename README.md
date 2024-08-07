# ![](https://raw.githubusercontent.com/alexander-naumov/gnu-screen/main/favicon.png) GNU Screen - screen manager with VT100/ANSI terminal emulation

[![#screen on libera.chat](https://img.shields.io/badge/IRC-%23screen-blue)](https://kiwiirc.com/nextclient/irc.libera.chat/#screen)
[![License](https://img.shields.io/github/license/alexander-naumov/gnu-screen)](https://github.com/alexander-naumov/gnu-screen/COPYING)
[![Buid Status](https://app.travis-ci.com/alexander-naumov/gnu-screen.svg?branch=main&status=started)](https://app.travis-ci.com/github/alexander-naumov/gnu-screen)
[![Functional test](https://github.com/alexander-naumov/gnu-screen/actions/workflows/test.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/test.yml)
[![build result](https://build.opensuse.org/projects/Base:System/packages/screen/badge.svg?type=default)](https://build.opensuse.org/package/show/Base:System/screen)
[![ML](https://img.shields.io/badge/mailing%20list-screen%20devel@gnu.org-blue)](https://lists.gnu.org/mailman/listinfo/screen-devel)

Screen is a full-screen window manager that multiplexes a physical
terminal between several processes (typically interactive shells).
Each virtual terminal provides the functions of a DEC VT100 terminal
and, in addition, several control functions from the ISO 6429
(ECMA 48, ANSI X3.64) and ISO 2022 standards (e.g. insert/delete
line and support for  multiple character sets).
There is a scrollback history buffer for each virtual terminal and
a copy-and-paste mechanism that allows moving text regions between
windows.

|  ```__________OS__________```| ```__________x86_64__________``` | ```__________aarch64__________``` | ```__________riscv64__________``` |
|--|:---:|:---:|:---:|
| <h2>Ubuntu</h2><br>(clang and gcc build)|[![Ubuntu 24.04 x86_64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_24_04_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_24_04_x86_64.yml)<br>[![Ubuntu 22.04 x86_64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_x86_64.yml)|[![Ubuntu 22.04 aarch64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_aarch64.yml)|[![Ubuntu 22.04 riscv64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_riscv64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/ubuntu_22_04_riscv64.yml)|
| <h2>Alpine Linux</h2><br>(gcc build with [musl libc](https://musl.libc.org/))<br>(--disable-pam)||[![Alpine latest aarch64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/alpine_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/alpine_aarch64.yml)||
| <h2>macOS</h2><br>(clang build)<br>(--disable-pam)||[![macOS 23.4.0 aarch64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/macos_23_4_0_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/macos_23_4_0_aarch64.yml)|
| <h2>FreeBSD</h2><br>(clang and gcc build)|[![FreeBSD 14.0 x86_64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/14.0_x86.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/14.0_x86.yml)<br>[![FreeBSD 13.3 x86_64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.3_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.3_x86_64.yml)<br>[![FreeBSD 13.2 x86_64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.2_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.2_x86_64.yml)    |[![FreeBSD 14.0 aarch64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/14.0_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/14.0_aarch64.yml)<br>[![FreeBSD 13.3 aarch64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.3_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.3_aarch64.yml)<br>[![FreeBSD 13.2 aarch64](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.2_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-freebsd/actions/workflows/13.2_aarch64.yml)|
| <h2>OpenBSD</h2><br>(clang build)<br>(--disable-pam)|[![OpenBSD 7.5 x86_64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_5_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_5_x86_64.yml)[![OpenBSD 7.4 x86_64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_4_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_4_x86_64.yml)|[![OpenBSD 7.5 aarch64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_5_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_5_aarch64.yml)[![OpenBSD 7.4 aarch64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_4_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/openbsd_7_4_aarch64.yml)|
| <h2>NetBSD</h2><br>(clang build)|[![NetBSD 10.0 x86_64](https://github.com/alexander-naumov/gnu-screen-on-netbsd/actions/workflows/netbsd_10_0_x86_64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-netbsd/actions/workflows/netbsd_10_0_x86_64.yml)|[![NetBSD 10.0 aarch64](https://github.com/alexander-naumov/gnu-screen-on-netbsd/actions/workflows/netbsd_10_0_aarch64.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen-on-netbsd/actions/workflows/netbsd_10_0_aarch64.yml)|
| <h2>Solaris</h2>(gcc build)<br>(--disable-pam)|[![Solaris 11.4 x86_64](https://github.com/alexander-naumov/gnu-screen/actions/workflows/Solaris.yml/badge.svg)](https://github.com/alexander-naumov/gnu-screen/actions/workflows/Solaris.yml)||

<img align="center" src="screenshot.png" height="550">

This project is just a sandbox for experiments with screen's sources.
Please use official GNU git-repo. Source code here can be *very* broken.
