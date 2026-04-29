/*
 * Hover text table and handler.
 */

#include "pgy_lsp_internal.h"

#include <stdio.h>
#include <string.h>

static const char *
lsp_hover_text_for_word(const char *word)
{
    if (strcmp(word, "func") == 0)
        return "**func** — Function declaration";
    if (strcmp(word, "let") == 0)
        return "**let** — Variable declaration";
    if (strcmp(word, "struct") == 0)
        return "**struct** — Value type (passed by value)";
    if (strcmp(word, "object") == 0)
        return "**object** — Passive state-bearing object type; can react but does not initiate intent";
    if (strcmp(word, "tobject") == 0)
        return "**tobject** — Transfer object. Boundary transfer data type";
    if (strcmp(word, "roster") == 0)
        return "**roster** — Party container with capacity constraints. Groups multiple parties (e.g., 4-party dungeon raid)";
    if (strcmp(word, "party") == 0)
        return "**party** — Authority-bearing participant declaration";
    if (strcmp(word, "class") == 0)
        return "**class** — Passive nominal host type (value semantics)";
    if (strcmp(word, "subject") == 0)
        return "**subject** — Identity-bearing host type\n- subject values are anchored handles, not plain copied values\n- subject actions can carry contract clauses like `requires`, `within`, `authorized by`, `causes`";
    if (strcmp(word, "action") == 0)
        return "**action** — Subject-host contract-bearing operation\n- carries an action contract pack: `within`, `requires`, `authorized by`, `causes`\n- matching intent steps may reuse this contract pack instead of repeating every clause";
    if (strcmp(word, "where") == 0)
        return "**where** — Contract/location clause\n- in generics: type constraint surface\n- in intent steps: current zone contract surface";
    if (strcmp(word, "who") == 0)
        return "**who** — Intent step participant clause\n- names which intent participant performs the step\n- may be inherited from a unique matching subject action";
    if (strcmp(word, "using") == 0)
        return "**using** — Intent step bound-zone alias clause\n- binds the step to a specific zone participant alias\n- may be derived from the `transfer` target contract";
    if (strcmp(word, "transfer") == 0)
        return "**transfer** — Cross-zone handoff clause\n- `transfer: from -> to` can derive the step `where` and `using` contract from the target zone\n- diagnostics should explain both current contract and the derived target contract";
    if (strcmp(word, "authority") == 0)
        return "**authority** — Zone mutation authority declaration\n- `authority subjectSlot requires Ability` declares which participant may mutate authority-bearing zone state\n- this often overlaps with action/step `requires`, so diagnostics should explain when the requirement was inherited rather than written twice";
    if (strcmp(word, "requires") == 0)
        return "**requires** — Ability contract clause\n- on `action`: part of the action contract pack\n- on `step`: explicit override or restatement of ability requirements";
    if (strcmp(word, "within") == 0)
        return "**within** — Action zone contract clause\n- fixes which zone type the action runs inside\n- matching intent steps may reuse this zone contract";
    if (strcmp(word, "causes") == 0)
        return "**causes** — Effect contract clause\n- declares which effect layer/state transition the action or step drives\n- matching intent steps may reuse this from the action contract";
    if (strcmp(word, "authorized") == 0)
        return "**authorized by** — Authority contract clause\n- required when a step/action mutates authority-bearing zone state or runs secure/effectful work inside an authority boundary\n- matching intent steps may reuse this from the action contract";
    if (strcmp(word, "by") == 0)
        return "**by** — Authority clause tail\n- used in `authorized by` and domain lifecycle commands to name the approving participant";
    if (strcmp(word, "with") == 0)
        return "**with** — Two different surfaces share this word\n- `with slot ... as ...` is a scoped binding/resource surface\n- `with effects ...` is a declaration-local effect contract\n- `with effects` is not part of the action -> step contract pack";
    if (strcmp(word, "effects") == 0)
        return "**effects** — Declaration-local effect contract tail\n- used in `with effects ...`\n- currently stays on the function/action declaration surface\n- not reused as part of the intent step contract pack";
    if (strcmp(word, "match") == 0)
        return "**match** — Pattern matching expression";
    if (strcmp(word, "parallel") == 0)
        return "**parallel** — Parallel execution block";
    if (strcmp(word, "import") == 0)
        return "**import** — Import module";
    if (strcmp(word, "Log") == 0)
        return "**Log(value)** — Print value with newline\n- If argument is multiline string, it is normalized and emitted via banner-style output";
    if (strcmp(word, "LogBlock") == 0)
        return "**LogBlock(text)** — Print multiline block text with banner-style normalization";
    if (strcmp(word, "LogBanner") == 0)
        return "**LogBanner(text)** — Print banner text with indentation-normalized output";
    if (strcmp(word, "LogRaw") == 0)
        return "**LogRaw(value)** — Print raw string value including embedded escapes/newlines";
    if (strcmp(word, "Ok") == 0)
        return "**Ok(value)** — Create success Result";
    if (strcmp(word, "Err") == 0)
        return "**Err(message)** — Create error Result";
    if (strcmp(word, "Unwrap") == 0)
        return "**Unwrap(result)** — Extract value or panic";
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
