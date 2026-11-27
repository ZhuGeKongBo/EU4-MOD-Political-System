#include "ps_script.h"
#include "content_types.h"
#include "native_modifiers.h"
#include "political_text.h"

#include <stdio.h>

int main(void) {
    printf("{\n  \"interface_version\": \"0.9.2\",\n"
           "  \"instance_fragments\": \"%s/*.txt\",\n"
           "  \"type_definitions\": \"%s/*.txt\",\n"
           "  \"layout_definitions\": \"%s/*.txt\",\n"
           "  \"panel_background\": {\"path\": \"interface.background\", \"inherits\": true, "
           "\"clear\": \"use the layout's GUI sprite\", \"override_control\": \"iconType\", "
           "\"on_unavailable\": \"keep the GUI sprite and log a warning\", \"game_campaign_verified\": false},\n"
           "  \"instance_history\": \"%s/*.txt\",\n"
           "  \"native_clause_engine\": {\"loaded_and_checked_by\": \"EU4 1.37.5\", "
           "\"conditions\": [\"window access and entry\", \"member filters\", \"role eligibility\", \"leader eligibility\", "
           "\"nomination\", \"lifecycle\", \"defensive war\", \"resource monthly terms\", \"action and incident options\"], "
           "\"framework_context_selector\": \"INSTANCE\", \"offline_check\": \"structure and source locations\", "
           "\"game_campaign_verified\": false},\n"
           "  \"action_clauses\": {\"conditions\": [\"potential\", \"trigger\", \"actor_trigger\", \"target_trigger\", \"can_stop\"], "
           "\"effects\": [\"effect\", \"on_effect\", \"off_effect\", \"removed_effect\"], "
           "\"continuous_modifiers\": [\"modifier\", \"emperor\", \"member\", \"province\"], "
           "\"native_ai_factor\": \"ai_will_do\", \"incident_model\": \"framework options; native scripts in each option\"},\n"
           "  \"nomination\": {\"path\": \"leader.succession.nomination\", \"fields\": [\"min\", \"max\", \"base\", \"opinion_weight\", \"tooltip\", \"terms\"], "
           "\"term_fields\": [\"add\", \"elector_trigger\", \"candidate_trigger\", \"tooltip\"], \"score_limits\": [-2000, 2000], "
           "\"display\": \"elector to viewer\", \"layout_fields\": [\"nomination_score\", \"nomination_tooltip_control\"]},\n"
           "  \"member_sources\": [\"explicit\", \"leader_subjects\", \"capital_territory\", \"query\", \"mixed\"],\n"
           "  \"history\": {\"founding_fields\": [\"founded\", \"date\"], \"default_founding\": \"earliest date block\", "
           "\"dated_fields\": [\"leader\", \"members\", \"add_members\", \"remove_members\", \"territory\", "
           "\"add_provinces\", \"remove_provinces\", \"resources\", \"roles\"], "
           "\"province_markers\": \"territory.source=province_history and territory.history_key\", "
           "\"country_markers\": \"special_members.<role>.history_key\"},\n"
           "  \"instance_registry\": \"%s\",\n  \"resource\": \"%s\",\n  \"legacy_alias_file\": \"%s\",\n"
           "  \"country_parameters\": [\"country tag\", \"ROOT\", \"THIS\", \"FROM\", \"PREV\"],\n"
           "  \"province_parameter\": [\"province ID\", \"ROOT\", \"THIS\", \"FROM\", \"PREV\"],\n"
           "  \"native_country_scopes\": [\"TAG\", \"FROM\", \"PREV\", \"any_country\", \"all_country\", \"every_country\", \"random_country\"],\n"
           "  \"native_province_scopes\": [\"province ID\", \"capital_scope\", \"any_province\", \"every_province\","
           " \"any_owned_province\", \"every_owned_province\"],\n"
           "  \"native_modifiers\": {\"declarations\": \"%s/*.txt\", \"fields\": [\"scope\", \"localisation\"], "
           "\"scopes\": [\"country\", \"province\"], \"format\": \"percentage\", \"registration\": \"native registry (D11 A)\", "
           "\"binding\": \"resources.<id>.monthly_growth.growth_modifier\", \"binding_fields\": [\"key\", \"source\"], "
           "\"sources\": [\"leader\", \"account_country\", \"leader_capital\", \"account_capital\"], "
           "\"game_campaign_verified\": false},\n"
           "  \"monthly_resource_terms\": {\"one_of\": [\"add\", \"modifier\"], "
           "\"instance_conditions\": [\"political_calculated_value\", \"political_has_reform\"], "
           "\"country_conditions\": \"EU4 trigger block and registered political triggers\", "
           "\"country_scope\": \"account owner or political_leader\"},\n"
           "  \"political_localisation\": {\"bindings\": \"%s/*.txt\", \"template_fields\": [\"parameters\", \"template\"], "
           "\"binding_fields\": [\"source\", \"id\"], \"syntax\": [\"$NAME$\", \"$NAME|format$\"], "
           "\"consumers\": [\"political tooltip\", \"trigger/effect description\", \"notice body and log\"], "
           "\"resource_script_key\": \"resources.<id>.script_description\", "
           "\"threshold_notice_key\": \"resources.<id>.thresholds.<id>.message\", "
           "\"growth_term_label\": \"resources.<id>.monthly_growth.<term>.tooltip\", \"game_campaign_verified\": false},\n"
           "  \"native_messages\": {\"path\": \"messages.<id>\", "
           "\"fields\": [\"title\", \"subtitle\", \"body\", \"log\", \"popup\", \"options\"], "
           "\"body_entries\": 4, \"option_labels\": [\"close\", \"secondary\"], "
           "\"effect\": \"political_send_message\", \"default_target\": \"THIS\", "
           "\"audience\": \"local player in the supported single-player campaign\", \"game_campaign_verified\": false},\n"
           "  \"equivalent_commands\": {",
           PS_ROSTER_DIRECTORY, PS_TYPE_DIRECTORY, PS_LAYOUT_DIRECTORY, PS_HISTORY_DIRECTORY,
           PS_ROSTER_PATH, PS_AUTHORITY, PS_ALIASES_PATH, PS_NATIVE_MODIFIER_DIRECTORY, PS_TEXT_DIRECTORY);
    for (const PsSpelling *spelling = ps_spellings; spelling->name; ++spelling)
        printf("%s\"%s\": \"%s\"", spelling == ps_spellings ? "" : ", ", spelling->name,
               ps_definitions[spelling->operation].name);
    puts("},\n  \"operations\": [");
    for (PsOperation i = PS_UNKNOWN + 1; i < PS_OPERATION_COUNT; ++i) {
        if (i == PS_ABSTAIN) continue; /* A legacy spelling with an explicit error. */
        const PsDefinition *definition = &ps_definitions[i];
        printf("    {\"name\": \"%s\", \"kind\": \"%s\", ",
               definition->name, definition->trigger ? "trigger" : "effect");
        const bool type_selector = ps_accepts_type_selector(i);
        if (type_selector) printf("\"one_of\": [\"instance\", \"type\"], ");
        const bool country_target = ps_accepts_country_target(i);
        if (country_target) printf("\"country_target\": {\"one_of\": [\"country\", \"target\"], \"required\": %s}, ",
                                   definition->required & PS_FIELD(PS_COUNTRY_FIELD) ? "true" : "false");
        printf("\"parameters\": {");
        const char *separator = "";
        for (PsField field = PS_FIELD_NONE + 1; field < PS_FIELD_COUNT; ++field) {
            const unsigned bit = PS_FIELD(field);
            if (!(definition->allowed & bit)) continue;
            const bool required = (definition->required & bit) && (!type_selector || field != PS_INSTANCE_FIELD) &&
                                  (!country_target || field != PS_COUNTRY_FIELD);
            printf("%s\"%s\": {\"required\": %s", separator, ps_field_names[field], required ? "true" : "false");
            if (field == PS_COUNTRY_FIELD || field == PS_ACTOR_FIELD || field == PS_TARGET_FIELD)
                printf(", \"accepts_leader\": %s", ps_accepts_leader_reference(i) ? "true" : "false");
            printf("}");
            separator = ", ";
        }
        printf("}, \"tooltip\": \"%s\"}%s\n", definition->description,
               i + 1 < PS_OPERATION_COUNT ? "," : "");
    }
    puts("  ]\n}");
    return ferror(stdout) ? 1 : 0;
}
