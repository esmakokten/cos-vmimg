# Download helpers. Every fetch is checksum-verified; a mismatch is a hard
# failure. The old build fetched over an unverified URL and, when it 404'd,
# carried on and packed a stale tree -- see README, "What went wrong before".

define fetch_verify
	@mkdir -p $(DL)
	@if [ ! -f "$(DL)/$(1)" ]; then \
		echo "  [FETCH] $(1)"; \
		curl -fsSL -o "$(DL)/$(1).part" "$(2)" || { \
			if [ -n "$(4)" ]; then \
				echo "  [FETCH] primary URL failed, trying fallback"; \
				curl -fsSL -o "$(DL)/$(1).part" "$(4)"; \
			else exit 1; fi; }; \
		mv "$(DL)/$(1).part" "$(DL)/$(1)"; \
	fi
	@echo "$(3)  $(DL)/$(1)" | sha256sum -c --quiet - || { \
		echo "ERROR: checksum mismatch for $(1)"; \
		echo "       expected $(3)"; \
		echo "       got      $$(sha256sum $(DL)/$(1) | cut -d' ' -f1)"; \
		echo "       refusing to build from an unverified archive."; \
		rm -f "$(DL)/$(1)"; exit 1; }
endef
