#!/bin/sh
g++ gtk_ui.cpp -o gtkff -O2 $(pkg-config --cflags --libs gtk+-3.0)
