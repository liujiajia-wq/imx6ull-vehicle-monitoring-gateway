SET(SOURCES screens/ui_Screen1.c
    screens/ui_Screen3.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    fonts/ui_font_FontHeiTi.c)

add_library(ui ${SOURCES})
