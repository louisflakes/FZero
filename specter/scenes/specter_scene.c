#include "specter_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const specter_scene_on_enter_handlers[])(void*) = {
#include "specter_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const specter_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "specter_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const specter_scene_on_exit_handlers[])(void*) = {
#include "specter_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers specter_scene_handlers = {
    .on_enter_handlers = specter_scene_on_enter_handlers,
    .on_event_handlers = specter_scene_on_event_handlers,
    .on_exit_handlers = specter_scene_on_exit_handlers,
    .scene_num = SpecterSceneNum,
};
