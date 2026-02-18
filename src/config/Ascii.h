// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information

#pragma once

#include <string>

const std::string windowsAsciiArt =
      "\n _    _______ _   __ ______\n"
      "| |  | | ___ \\ | / /|___  /\n"
      "| |  | | |_/ / |/ /    / / \n"
      "| |/\\| |    /|    \\   / /  \n"
      "\\  /\\  / |\\ \\| |\\  \\./ /___\n"
      " \\/  \\/\\_| \\_\\_| \\_/\\_____/\n";

const std::string nonWindowsAsciiArt = 
      "\n                                             \n"
      " â–„â–ˆ     â–ˆâ–„     â–„â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆ    â–„â–ˆ   â–„â–ˆâ–„  â–„â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–„  \n"
      "â–ˆâ–ˆâ–ˆ     â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ    â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ â–„â–ˆâ–ˆâ–ˆâ–€ â–ˆâ–ˆâ–€     â–„â–ˆâ–ˆ \n"
      "â–ˆâ–ˆâ–ˆ     â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ    â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆâ–â–ˆâ–ˆâ–€         â–„â–ˆâ–ˆâ–ˆâ–€ \n"
      "â–ˆâ–ˆâ–ˆ     â–ˆâ–ˆâ–ˆ  â–„â–ˆâ–ˆâ–ˆâ–„â–„â–„â–„â–ˆâ–ˆâ–€  â–„â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€     â–€â–ˆâ–€â–„â–ˆâ–ˆâ–ˆâ–€â–„â–„ \n"
      "â–ˆâ–ˆâ–ˆ     â–ˆâ–ˆâ–ˆ â–€â–€â–ˆâ–ˆâ–ˆâ–€â–€â–€â–€â–€   â–€â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–„      â–„â–ˆâ–ˆâ–ˆâ–€   â–€ \n"
      "â–ˆâ–ˆâ–ˆ     â–ˆâ–ˆâ–ˆ â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆâ–â–ˆâ–ˆâ–„   â–„â–ˆâ–ˆâ–ˆâ–€       \n"
      "â–ˆâ–ˆâ–ˆ â–„â–ˆâ–„ â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ    â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ â–€â–ˆâ–ˆâ–ˆâ–„ â–ˆâ–ˆâ–ˆâ–„     â–„â–ˆ \n"
      " â–€â–ˆâ–ˆâ–ˆâ–€â–ˆâ–ˆâ–ˆâ–€    â–ˆâ–ˆâ–ˆ    â–ˆâ–ˆâ–ˆ   â–ˆâ–ˆâ–ˆ   â–€â–ˆâ–€  â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€ \n"
      "              â–ˆâ–ˆâ–ˆ    â–ˆâ–ˆâ–ˆ   â–€                     \n";

/* Windows has some characters it won't display in a terminal. If your ascii
   art works fine on Windows and Linux terminals, just replace 'asciiArt' with
   the art itself, and remove these two #ifdefs and above ascii arts */
#ifdef _WIN32
const std::string asciiArt = windowsAsciiArt;
#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif
