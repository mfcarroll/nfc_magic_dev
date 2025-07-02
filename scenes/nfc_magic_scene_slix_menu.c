#include "../nfc_magic_app_i.h"

enum SubmenuIndex {
    SubmenuIndexWipe,
    SubmenuIndexMoreInfo,
};

void nfc_magic_scene_slix_menu_submenu_callback(void* context, uint32_t index) {
    NfcMagicApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

/**
 * @brief SLIX menu scene on_enter handler.
 * @param context Pointer to the NfcMagicApp context.
 */
void nfc_magic_scene_slix_menu_on_enter(void* context) {
    NfcMagicApp* instance = context;

    Submenu* submenu = instance->submenu;
    submenu_add_item(
        submenu, "Wipe", SubmenuIndexWipe, nfc_magic_scene_slix_menu_submenu_callback, instance);
    submenu_add_item(
        submenu,
        "More info (WIP)",
        SubmenuIndexMoreInfo,
        nfc_magic_scene_slix_menu_submenu_callback,
        instance);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneSlixMenu));
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewMenu);
}

/**
 * @brief SLIX menu scene on_event handler.
 * @param context Pointer to the NfcMagicApp context.
 * @param event SceneManagerEvent event.
 * @return true if the event was consumed, false otherwise.
 */
bool nfc_magic_scene_slix_menu_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexWipe) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWipe);
            consumed = true;
        } else if(event.event == SubmenuIndexMoreInfo) {
            // Placeholder: just go back to the previous scene
            consumed = scene_manager_previous_scene(instance->scene_manager);
        }
        scene_manager_set_scene_state(instance->scene_manager, NfcMagicSceneSlixMenu, event.event);
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneStart);
    }

    return consumed;
}

/**
 * @brief SLIX menu scene on_exit handler.
 * @param context Pointer to the NfcMagicApp context.
 */
void nfc_magic_scene_slix_menu_on_exit(void* context) {
    NfcMagicApp* instance = context;

    submenu_reset(instance->submenu);
}
