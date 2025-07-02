#include "../nfc_magic_app_i.h"
#include "magic/protocols/slix/slix.h"

static const char* slix_type_names[SlixTypeCount] = {
    [SlixTypeSlix] = "SLIX",
    [SlixTypeSlixS] = "SLIX-S",
    [SlixTypeSlixL] = "SLIX-L",
    [SlixTypeSlix2] = "SLIX2",
};

void nfc_magic_scene_slix_show_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_slix_show_info_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    if(scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneSlixShowInfo)) {
        notification_message(instance->notifications, &sequence_success);
    }
    scene_manager_set_scene_state(instance->scene_manager, NfcMagicSceneSlixShowInfo, false);

    const SlixData* slix_data = instance->slix_data;
    FuriString* temp_str = furi_string_alloc();

    widget_add_string_element(widget, 3, 4, AlignLeft, AlignTop, FontPrimary, "SLIX Info");

    // Card Type
    if(slix_data->type < SlixTypeCount) {
        furi_string_printf(temp_str, "Type: %s", slix_type_names[slix_data->type]);
    } else {
        furi_string_printf(temp_str, "Type: Unknown");
    }
    widget_add_string_element(
        widget, 3, 17, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    // UID
    furi_string_set(temp_str, "UID: ");
    for(int8_t i = SLIX_UID_LEN - 1; i >= 0; i--) {
        furi_string_cat_printf(temp_str, "%02X ", slix_data->uid[i]);
    }
    widget_add_string_element(
        widget, 3, 27, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    // Memory Info
    const Iso15693_3SystemInfo* iso_info = &slix_data->iso15693_3_info;
    if(iso_info->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) {
        furi_string_printf(
            temp_str, "Mem: %d blocks x %d bytes", iso_info->block_count, iso_info->block_size);
    } else {
        furi_string_printf(temp_str, "Mem: Info not available");
    }
    widget_add_string_element(
        widget, 3, 37, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    // Signature
    if(slix_data->signature_read) {
        furi_string_set(temp_str, "Signature: Read");
    } else if(slix_data->type == SlixTypeSlix2) {
        furi_string_set(temp_str, "Signature: Not Read");
    } else {
        furi_string_set(temp_str, "Signature: N/A");
    }
    widget_add_string_element(
        widget, 3, 47, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_slix_show_info_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneSlixMenu);
    }
    return consumed;
}

void nfc_magic_scene_slix_show_info_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
