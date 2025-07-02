#include "../nfc_magic_app_i.h"

enum {
    NfcMagicSceneSlixGetInfoStateCardSearch,
    NfcMagicSceneSlixGetInfoStateCardFound,
};

NfcCommand nfc_magic_scene_slix_get_info_poller_callback(SlixPollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == SlixPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == SlixPollerEventTypeRequestMode) {
        event.data->request_mode.mode = SlixPollerModeGetInfo;
    } else if(event.type == SlixPollerEventTypeSuccess) {
        slix_copy(instance->slix_data, slix_poller_get_data(instance->slix_poller));
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
        command = NfcCommandStop;
    } else if(event.type == SlixPollerEventTypeFail) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
        command = NfcCommandStop;
    }

    return command;
}

static void nfc_magic_scene_slix_get_info_setup_view(NfcMagicApp* instance) {
    Popup* popup = instance->popup;
    popup_reset(popup);
    uint32_t state =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneSlixGetInfo);

    if(state == NfcMagicSceneSlixGetInfoStateCardSearch) {
        popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
        popup_set_text(
            instance->popup, "Apply the\ncard\nto the back", 128, 32, AlignRight, AlignCenter);
    } else {
        popup_set_icon(popup, 12, 23, &I_Loading_24);
        popup_set_header(popup, "Reading\nDon't move...", 52, 32, AlignLeft, AlignCenter);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewPopup);
}

void nfc_magic_scene_slix_get_info_on_enter(void* context) {
    NfcMagicApp* instance = context;

    scene_manager_set_scene_state(
        instance->scene_manager,
        NfcMagicSceneSlixGetInfo,
        NfcMagicSceneSlixGetInfoStateCardSearch);
    nfc_magic_scene_slix_get_info_setup_view(instance);

    nfc_magic_app_blink_start(instance);

    instance->slix_poller = slix_poller_alloc(instance->nfc);
    slix_poller_set_data(instance->slix_poller, instance->slix_data);
    slix_poller_start(
        instance->slix_poller, nfc_magic_scene_slix_get_info_poller_callback, instance);
}

bool nfc_magic_scene_slix_get_info_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventCardDetected) {
            scene_manager_set_scene_state(
                instance->scene_manager,
                NfcMagicSceneSlixGetInfo,
                NfcMagicSceneSlixGetInfoStateCardFound);
            nfc_magic_scene_slix_get_info_setup_view(instance);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventCardLost) {
            scene_manager_set_scene_state(
                instance->scene_manager,
                NfcMagicSceneSlixGetInfo,
                NfcMagicSceneSlixGetInfoStateCardSearch);
            nfc_magic_scene_slix_get_info_setup_view(instance);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerSuccess) {
            // For notification message
            scene_manager_set_scene_state(
                instance->scene_manager, NfcMagicSceneSlixShowInfo, true);
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneSlixShowInfo);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerFail) {
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneSlixMenu);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_slix_get_info_on_exit(void* context) {
    NfcMagicApp* instance = context;

    slix_poller_stop(instance->slix_poller);
    slix_poller_free(instance->slix_poller);
    scene_manager_set_scene_state(
        instance->scene_manager,
        NfcMagicSceneSlixGetInfo,
        NfcMagicSceneSlixGetInfoStateCardSearch);
    popup_reset(instance->popup);

    nfc_magic_app_blink_stop(instance);
}
