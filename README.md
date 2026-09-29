# nilo-console-header-only-lib
This is a console utility toolkit, mainly focused on:     * console commands     * output streams     * input streams     * buffer clearing / flushing

============================================================
  nilo::console - header-only cross-platform console utilities
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

  This is a console utility toolkit, mainly focused on:
  * console commands
  * output streams
  * input streams
  * buffer clearing / flushing

  !!!Requires : C++17 or later (std::optional, std::string_view, if constexpr)
  ------------------------------------------------------------
  Install :
      Place this file at:
      <your-project>/include/nilo/console.hpp
      (create the `nilo` folder under your include
      directory if it does not exist)

  Then include it as:
      #include <nilo/console.hpp>
  ------------------------------------------------------------
  Thread safety:     
      initConsole() may be called concurrently.
      Other APIs are not thread-safe.
      ============================================================
