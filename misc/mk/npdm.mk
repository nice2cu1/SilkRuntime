# Build the exact NPDM consumed by the game. The source manifest is the
# Skyline/H1e capability manifest in the workspace; the output name must be
# main.npdm, not the target name SilkModLoader.npdm.

.PHONY: runtime-npdm

runtime-npdm: $(RUNTIME_NPDM)

$(RUNTIME_NPDM): $(RUNTIME_NPDM_JSON)
	@mkdir -p $(dir $@)
	@npdmtool $< $@
	@echo built ... $(notdir $@)

.PHONY: npdm-json

npdm-json:
	@$(PYTHON) $(SCRIPTS_PATH)/make-npdm-json.py
