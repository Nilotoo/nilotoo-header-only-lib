# nilotoo header-only lib

nilo is a header-only cross-platform utility library, organized as a set of independent modules. Each module can be included on its own; the umbrella header pulls them all in for convenience.

============================================================
  nilo - header-only cross-platform utility library
------------------------------------------------------------
  Author  : nilotoo.
  Version : v1.0.0
  Updated : 2026-10
  License : MIT
------------------------------------------------------------
  SPDX-License-Identifier: MIT
  SPDX-FileCopyrightText: 2026 nilotoo.

  Hi, I'm nilotoo.
  Thank you for using this header-only library.
  Licensed under the MIT License. Copyright (c) 2026 nilotoo.

  This is a header-only cross-platform utility library,
  organized as a set of independent modules.
  You can include the whole library, or include only the module
  you need.

  Current modules:
  * console
      - console commands
      - output streams
      - input streams
      - buffer clearing / flushing

  Future modules can be added without changing the umbrella
  include.

  !!!Requires : C++17 or later (std::optional, std::string_view, if constexpr)
------------------------------------------------------------
  Install :
      Place the headers at:
      <your-project>/include/nilo/*.hpp
      (create the `nilo` folder under your include
      directory if it does not exist)

  include a single module only:
      #include <nilo/console.hpp>
------------------------------------------------------------
  Thread safety:
      in console.hpp:
      1. initConsole() may be called concurrently.
         Other APIs are not thread-safe.
============================================================
