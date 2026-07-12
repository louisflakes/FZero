#include "rollcall_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const rollcall_scene_on_enter_handlers[])(void*) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const rollcall_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const rollcall_scene_on_exit_handlers[])(void*) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers rollcall_scene_handlers = {
    .on_enter_handlers = rollcall_scene_on_enter_handlers,
    .on_event_handlers = rollcall_scene_on_event_handlers,
    .on_exit_handlers = rollcall_scene_on_exit_handlers,
    .scene_num = RollCallSceneNum,
};
