/*
 * Hover text table and handler.
 */

#include "pgy_lsp_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *word;
    const char *text;
} LspHoverEntry;

static const LspHoverEntry k_hover_entries[] = {
    { "func", "**func** - Function declaration" },
    { "let", "**let** - Variable declaration" },
    { "struct", "**struct** - Value type (passed by value)" },
    { "object", "**object** - Passive state-bearing object type; can react but does not initiate intent" },
    { "tobject", "**tobject** - Transfer object. Boundary transfer data type" },
    { "roster", "**roster** - Party container with capacity constraints. Groups multiple parties (e.g., 4-party dungeon raid)" },
    { "party", "**party** - Authority-bearing participant declaration" },
    { "class", "**class** - Passive nominal host type (value semantics)" },
    { "subject", "**subject** - Identity-bearing host type\n- subject values are anchored handles, not plain copied values\n- subject actions can carry contract clauses like `requires`, `within`, `authorized by`, `causes`" },
    { "action", "**action** - Subject-host contract-bearing operation\n- carries an action contract pack: `within`, `requires`, `authorized by`, `causes`\n- matching intent steps may reuse this contract pack instead of repeating every clause" },
    { "where", "**where** - Contract/location clause\n- in generics: type constraint surface\n- in intent steps: current zone contract surface" },
    { "who", "**who** - Intent step participant clause\n- names which intent participant performs the step\n- may be inherited from a unique matching subject action" },
    { "using", "**using** - Intent step bound-zone alias clause\n- binds the step to a specific zone participant alias\n- may be derived from the `transfer` target contract" },
    { "transfer", "**transfer** - Cross-zone handoff clause\n- `transfer: from -> to` can derive the step `where` and `using` contract from the target zone\n- diagnostics should explain both current contract and the derived target contract" },
    { "authority", "**authority** - Zone mutation authority declaration\n- `authority subjectSlot requires Ability` declares which participant may mutate authority-bearing zone state\n- this often overlaps with action/step `requires`, so diagnostics should explain when the requirement was inherited rather than written twice" },
    { "requires", "**requires** - Ability contract clause\n- on `action`: part of the action contract pack\n- on `step`: explicit override or restatement of ability requirements" },
    { "within", "**within** - Action zone contract clause\n- fixes which zone type the action runs inside\n- matching intent steps may reuse this zone contract" },
    { "causes", "**causes** - Effect contract clause\n- declares which effect layer/state transition the action or step drives\n- matching intent steps may reuse this from the action contract" },
    { "authorized", "**authorized by** - Authority contract clause\n- required when a step/action mutates authority-bearing zone state or runs secure/effectful work inside an authority boundary\n- matching intent steps may reuse this from the action contract" },
    { "by", "**by** - Authority clause tail\n- used in `authorized by` and domain lifecycle commands to name the approving participant" },
    { "with", "**with** - Two different surfaces share this word\n- `with slot ... as ...` is a scoped binding/resource surface\n- `with effects ...` is a declaration-local effect contract\n- `with effects` is not part of the action -> step contract pack" },
    { "effects", "**effects** - Declaration-local effect contract tail\n- used in `with effects ...`\n- currently stays on the function/action declaration surface\n- not reused as part of the intent step contract pack" },
    { "match", "**match** - Pattern matching expression" },
    { "parallel", "**parallel** - Parallel execution block" },
    { "import", "**import** - Import module" },
    { "Log", "**Log(value)** - Print value with newline\n- If argument is multiline string, it is normalized and emitted via banner-style output" },
    { "LogBlock", "**LogBlock(text)** - Print multiline block text with banner-style normalization" },
    { "LogBanner", "**LogBanner(text)** - Print banner text with indentation-normalized output" },
    { "LogRaw", "**LogRaw(value)** - Print raw string value including embedded escapes/newlines" },
    { "Ok", "**Ok(value)** - Create success Result" },
    { "Err", "**Err(message)** - Create error Result" },
    { "Unwrap", "**Unwrap(result)** - Extract value or panic" },
};

static const char *
lsp_hover_text_for_word(const char *word)
{
    if (word == NULL)
        return NULL;

    for (size_t i = 0; i < sizeof(k_hover_entries) / sizeof(k_hover_entries[0]); i++) {
        if (strcmp(k_hover_entries[i].word, word) == 0)
            return k_hover_entries[i].text;
    }
    return NULL;
}

void
respond_hover(int id, const char *source_text, int line, int character)
{
    char word[128];
    const char *hover_text;

    if (!extract_word_at_position(source_text, line, character, word, sizeof(word))) {
        lsp_respond(id, "null");
        return;
    }

    hover_text = lsp_hover_text_for_word(word);
    if (hover_text != NULL) {
        char hover_resp[1024];
        snprintf(hover_resp, sizeof(hover_resp),
            "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
            hover_text);
        lsp_respond(id, hover_resp);
    } else {
        lsp_respond(id, "null");
    }
}
