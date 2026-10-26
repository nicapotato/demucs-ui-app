# Product GUI — CI/release workflows use workflow_dispatch.

CI_WORKFLOW := .github/workflows/ci.yml
RELEASE_WORKFLOW := .github/workflows/release.yml

REPO ?= $(shell git remote get-url origin 2>/dev/null | sed -E 's|git@github\.com:||; s|https://github\.com/||; s|\.git$$||')
GH_R := $(if $(REPO),-R "$(REPO)",)
REF ?= $(shell git branch --show-current 2>/dev/null)

PUBLISH_ITCH ?= true
PUBLISH_GH_RELEASE ?= true
BUILD_PLATFORM ?= all

.PHONY: run run-c run-py run-onnx process process-c process-py process-onnx build \
	bundle bundle-mlx-c bundle-mlx-py bundle-py bundle-onnx \
	verify-mp3 verify-psarc models models-py \
	worker worker-c worker-py worker-fetch-c worker-fetch-py worker-fetch-cuda worker-fetch-onnx worker-onnx \
	clean ci ci-watch release release-watch

run run-c run-py run-onnx process process-c process-py process-onnx build \
bundle bundle-mlx-c bundle-mlx-py bundle-py bundle-onnx \
verify-mp3 verify-psarc models models-py \
worker worker-c worker-py worker-fetch-c worker-fetch-py worker-fetch-cuda worker-fetch-onnx worker-onnx clean:
	$(MAKE) -C app $@

ci:
	@test -n "$(REPO)" || (echo "ERROR: could not resolve origin repo; set REPO=owner/name" >&2; exit 1)
	gh $(GH_R) workflow run "$(CI_WORKFLOW)" \
		$(if $(REF),-r "$(REF)",) \
		-f publish_itch="$(PUBLISH_ITCH)" \
		-f build_platform="$(BUILD_PLATFORM)" \
		$(if $(VERSION),-f version="$(VERSION)",)

release:
	@test -n "$(REPO)" || (echo "ERROR: could not resolve origin repo; set REPO=owner/name" >&2; exit 1)
	gh $(GH_R) workflow run "$(RELEASE_WORKFLOW)" \
		$(if $(REF),-r "$(REF)",) \
		-f publish_itch="$(PUBLISH_ITCH)" \
		-f publish_gh_release="$(PUBLISH_GH_RELEASE)" \
		-f build_platform="$(BUILD_PLATFORM)" \
		$(if $(VERSION),-f version="$(VERSION)",)

ci-watch: ci
	@sleep 2
	@RID=$$(gh $(GH_R) run list --workflow="$(CI_WORKFLOW)" -L 1 --json databaseId -q '.[0].databaseId'); \
		test -n "$$RID"; \
		gh $(GH_R) run watch "$$RID"

release-watch: release
	@sleep 2
	@RID=$$(gh $(GH_R) run list --workflow="$(RELEASE_WORKFLOW)" -L 1 --json databaseId -q '.[0].databaseId'); \
		test -n "$$RID"; \
		gh $(GH_R) run watch "$$RID"
