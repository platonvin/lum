// #include "input/input.hpp"
#include "clum.h"
#include <assert.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

/*
    This is C99 version of demo. It is incomplete, but still works as example
    no animations / input / physics / etc.
    i do not want to include cglm just for that
*/

void cleanup(LumInstance* lum);

// in real world you would store them as ECS components / member in EntityClass
// but for simplicity lets stick to just this
LumMeshModel tank_body = {};
LumMeshTransform tank_body_trans = {};
LumMeshModel tank_head = {};
LumMeshTransform tank_head_trans = {};
LumMeshModel tank_rf_leg = {};
LumMeshTransform tank_rf_leg_trans = {};
LumMeshModel tank_lf_leg = {};
LumMeshTransform tank_lf_leg_trans = {};
LumMeshModel tank_rb_leg = {};
LumMeshTransform tank_rb_leg_trans = {};
LumMeshModel tank_lb_leg = {};
LumMeshTransform tank_lb_leg_trans = {};

int main(){
    LumSettings settings = LUM_DEFAULT_SETTINGS;
        settings.fullscreen = false;
        settings.vsync = false;
        settings.world_size[0] = 48;
        settings.world_size[1] = 48;
        settings.world_size[2] = 16;
        settings.static_block_palette_size = 15;
        settings.maxParticleCount = 8128;

    LumInstance* lum = lum_create_instance(&settings);
    assert(lum);
    lum_init(lum, &settings);
    lum_demo_load_scene(lum, "assets/scene");
    
    // preparation stage. These functions also can be called in runtime
    // If no palette is found, first models defines it
        // this DOES set palette
    tank_body = lum_load_mesh_file(lum, "assets/tank_body.vox", true);
    tank_body_trans.shift[0] = 13.1f * 16.0f;
    tank_body_trans.shift[1] = 14.1f * 16.0f;
    tank_body_trans.shift[2] = 3.1f * 16.0f;
        // this DOES NOT set palette cause already setten.
    tank_head = lum_load_mesh_file(lum, "assets/tank_head.vox", true);
        // material palette can also be loaded from alone
        // lum.loadPalette("my_magicavox_filescene_with_palette_i_want_to_extract.vox")
        // good way to handle this would be to have voxel for each material placed in scene to view them

    tank_rf_leg = lum_load_mesh_file(lum, "assets/tank_rf_lb_leg.vox", false);
    tank_lb_leg = lum_load_mesh_file(lum, "assets/tank_rf_lb_leg.vox", false);
    tank_lf_leg = lum_load_mesh_file(lum, "assets/tank_lf_rb_leg.vox", false);
    tank_rb_leg = lum_load_mesh_file(lum, "assets/tank_lf_rb_leg.vox", false);
    
    LumMeshFoliage* grass = lum_load_foliage(lum, "", 6, 10);
    LumMeshLiquid water = lum_load_liquid(lum, 69, 42);
    uint8_t color[] = {0, 0, 0};
    LumMeshVolumetric smoke = lum_load_volumetric(lum, 1.0f, 0.5f, color);

    lum_load_block_file(lum, 1, "assets/dirt.vox");
    lum_load_block_file(lum, 2, "assets/grass.vox");
    lum_load_block_file(lum, 3, "assets/grassNdirt.vox");
    lum_load_block_file(lum, 4, "assets/stone_dirt.vox");
    lum_load_block_file(lum, 5, "assets/bush.vox");
    lum_load_block_file(lum, 6, "assets/leaves.vox");
    lum_load_block_file(lum, 7, "assets/iron.vox");
    lum_load_block_file(lum, 8, "assets/lamp.vox");
    lum_load_block_file(lum, 9, "assets/stone_brick.vox");
    lum_load_block_file(lum, 10, "assets/stone_brick_cracked.vox");
    lum_load_block_file(lum, 11, "assets/stone_pack.vox");
    lum_load_block_file(lum, 12, "assets/bark.vox");
    lum_load_block_file(lum, 13, "assets/wood.vox");
    lum_load_block_file(lum, 14, "assets/planks.vox");
    // total 15 max blocks specified, so loadBlock(15) is illegal

    // literally uploads data to gpu. You can call them in runtime, but atm its not recommended (a lot of overhead curently, can be significantly reduced)
    lum_upload_block_palette_to_gpu(lum);
    lum_upload_material_palette_to_gpu(lum);
    
    lum_wait_idle(lum);

    // callback functions for glfw action input system 
    // setup_input(input, lum);

    while (!lum_get_should_close(lum)) {
        glfwPollEvents();
        int should_close = lum_get_should_close(lum);
        should_close |= glfwWindowShouldClose(lum_get_glfw_ptr(lum));
        should_close |= (glfwGetKey(lum_get_glfw_ptr(lum), GLFW_KEY_ESCAPE) == GLFW_PRESS);
        lum_set_should_close(lum, should_close);

        lum_start_frame(lum);
        // could be impicit
        lum_draw_world(lum);
        lum_draw_particles(lum);

        // draw tank model
        // It does not look correct, but C demo is not even trying. Whole point is to test if C99 API works
        // If anyone wants to port that to C - you are welcome
        lum_draw_model(lum, tank_body, &tank_body_trans);
        lum_draw_model(lum, tank_head, &tank_head_trans);
        lum_draw_model(lum, tank_rf_leg, &tank_rf_leg_trans);
        lum_draw_model(lum, tank_lf_leg, &tank_lf_leg_trans);
        lum_draw_model(lum, tank_rb_leg, &tank_rb_leg_trans);
        lum_draw_model(lum, tank_lb_leg, &tank_lb_leg_trans);

        // literally procedural grass placement every frame. You probably want to store it as entities in your own structures
        for (int xx = 4; xx < 20; xx++) {
            for (int yy = 4; yy < 20; yy++) {
                if (xx >= 5 && xx < 12 && yy >= 6 && yy < 16) continue;
                int pos[3] = {xx * 16, yy * 16, 16};
                lum_draw_foliage_block(lum, grass, pos);
            }
        }

        // literally procedural water placement every frame. You probably want to store it as entities in your own structures
        for (int xx = 5; xx < 12; xx++) {
            for (int yy = 6; yy < 16; yy++) {
                int pos[3] = {xx * 16, yy * 16, 14};
                lum_draw_liquid_block(lum, water, pos);
            }
        }

        // literally procedural smoke placement every frame. You probably want to store it as entities in your own structures
        for (int xx = 8; xx < 10; xx++) {
            for (int yy = 10; yy < 13; yy++) {
                int pos[3] = {xx * 16, yy * 16, 20};
                lum_draw_volumetric_block(lum, smoke, pos);
            }
        }

        lum_prepare_frame(lum);
        lum_submit_frame(lum);
    }

    // prints GPU profile data
    if (lum_is_profiling_enabled(lum)) {
        char name[32];
        printf("%-22s: %7.3f: %7.3f\n", "Start", 0.0, 0.0);

        int timestamp_count = lum_get_current_timestamp_count(lum);
        double previous_timestamp = lum_get_timestamp(lum, 0);

        for (int i = 1; i < timestamp_count; i++) {
            double current_timestamp = lum_get_timestamp(lum, i);
            double time_diff = lum_get_timestamp_difference(lum, i - 1, i);
            
            lum_get_timestamp_name(lum, i, name, sizeof(name));
            printf("%3d %-22s: %7.3f: %7.3f\n", i, name, current_timestamp, time_diff);
            
            previous_timestamp = current_timestamp;
        }
    }
    cleanup(lum);
}

void cleanup(LumInstance* lum){
    // save_scene functions are not really supposed to be used, i implemented them for demo
    // and they are not ported to C++. Why would anyone use them?

    // automatically frees all allocated blocks and meshes
    lum_cleanup(lum);
}