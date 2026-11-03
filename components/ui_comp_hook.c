// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
//
// 本文件对应 SquareLine 导出的 components/ui_comp_hook.c，
// 用于放置"组件创建后需要执行的自定义代码"。
// SquareLine 每次导出都会覆盖 ui_comp_hook.c，所以自定义逻辑放这里最安全。
//
// 本项目用途：统一给所有中文标签套用工程字库，
//            避免在 SquareLine 里忘记逐个设置字体导致中文变方块。

#include "../ui.h"

/* 若将来在 SquareLine 里新增了组件，可以在这里补一个 hook */
void ui_comp_Gateway_hook(lv_obj_t * comp);
void ui_comp_Gateway_hook(lv_obj_t * comp)
{
    (void)comp;
}
