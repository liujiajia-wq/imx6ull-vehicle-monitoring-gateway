// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Project name: Gateway

#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////

// EVENTS
lv_obj_t * ui____initial_actions0;

///////////////////// TEST LVGL SETTINGS ////////////////////

#if LV_COLOR_DEPTH != 32
    #error "LV_COLOR_DEPTH should be 32bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP != 0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////

///////////////////// SCREENS ////////////////////

void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                        false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    ui_Gateway_screen_init();
    lv_disp_load_scr(ui_Gateway);

    ui____initial_actions0 = lv_obj_create(NULL);
}

void ui_destroy(void)
{
    ui_Gateway_screen_destroy();
}
