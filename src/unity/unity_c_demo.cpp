/*
File containing ALL source files for unity build (aka single translation unit)
this gives higher perfomance (like -flto, but better) and decreases compile times on github actions
(github action's CPU is 2 threads)
*/

#include "../../lum-al/src/unity.cpp"

#include "../renderer/ao_lut.cpp"
#include "../renderer/load_stuff.cpp"
#include "../renderer/render_ui_interface.cpp"
#include "../renderer/render.cpp"
#include "../renderer/setup.cpp"
#include "../renderer/ui.cpp"

#include "ogt_vox.cpp"
#include "ogt_voxel_meshify.cpp"
#include "meshopt.cpp"

#include "../lum.cpp"
#include "../clum.cpp"
#include "../cdemo.c"